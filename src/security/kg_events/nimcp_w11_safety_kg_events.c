//=============================================================================
// nimcp_w11_safety_kg_events.c - Wave W11 Safety KG Event Emitters
//=============================================================================
/**
 * @file nimcp_w11_safety_kg_events.c
 * @brief Implementation of W11 safety KG emitters.
 *
 * All emitters follow the same pattern as W4's
 * `nimcp_subcortical_runtime_events.c`:
 *  - NULL/disabled early-return
 *  - self-elevate to ADMIN via `brain->internal_kg_admin_token`
 *  - add_node + add_edge to a structural parent
 *  - restore READ access
 *
 * The four structural parents ("ethics_engine", "lgss_module",
 * "mental_health_monitor", "brain_immune") are created lazily on the
 * first emit of each family via `w11_safety_ensure_roots`.
 */

#include "security/nimcp_w11_safety_kg_events.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

BRIDGE_BOILERPLATE_MESH_ONLY(w11_safety_kg_events, MESH_ADAPTER_CATEGORY_SECURITY)

/* ------------------------------------------------------------------------- *
 * Internal helpers                                                          *
 * ------------------------------------------------------------------------- */

/** @brief Microsecond timestamp (resolution-limited by time()). */
static uint64_t w11_ts_us(void) {
    return (uint64_t)time(NULL) * 1000000ULL;
}

/** @brief Return true iff brain has a usable, enabled internal KG. */
static bool w11_kg_ready(const struct brain_struct* brain) {
    if (!brain) return false;
    if (!brain->internal_kg_enabled) return false;
    if (!brain->internal_kg) return false;
    return true;
}

/**
 * @brief Emit a single event node + back-edge to a named structural parent.
 *
 * - NULL/disabled safe
 * - Self-elevates to ADMIN for the duration of the writes
 * - Back-edge uses BRAIN_KG_EDGE_SENDS_TO with description "produced_by"
 *   (registry §5: parent -> event signals "parent produced this event").
 */
static void w11_emit_linked(
    struct brain_struct* brain,
    const char* event_name,
    const char* description,
    const char* parent_name,
    float edge_weight
) {
    if (!w11_kg_ready(brain)) return;
    if (!event_name) return;

    brain_kg_t* kg = brain->internal_kg;
    const uint64_t token = brain->internal_kg_admin_token;

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, token);

    brain_kg_node_id_t ev = brain_kg_add_node(kg, event_name,
        BRAIN_KG_NODE_COGNITIVE,
        description ? description : "safety runtime event");

    if (ev != BRAIN_KG_INVALID_NODE && parent_name && *parent_name) {
        brain_kg_node_id_t parent = brain_kg_find_node(kg, parent_name);
        if (parent != BRAIN_KG_INVALID_NODE) {
            brain_kg_add_edge(kg, parent, ev,
                BRAIN_KG_EDGE_SENDS_TO, "produced_by", edge_weight);
        }
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

/**
 * @brief Ensure one structural root exists; idempotent.
 *
 * Access-level elevation is handled by the caller (`w11_safety_ensure_roots`).
 */
static void w11_ensure_root(
    brain_kg_t* kg,
    const char* name,
    const char* description
) {
    if (brain_kg_find_node(kg, name) == BRAIN_KG_INVALID_NODE) {
        brain_kg_add_node(kg, name, BRAIN_KG_NODE_COGNITIVE,
                          description ? description : "W11 safety root");
    }
}

/* ------------------------------------------------------------------------- *
 * Public: structural-root seeding                                           *
 * ------------------------------------------------------------------------- */

void w11_safety_ensure_roots(struct brain_struct* brain) {
    if (!w11_kg_ready(brain)) return;

    brain_kg_t* kg = brain->internal_kg;
    const uint64_t token = brain->internal_kg_admin_token;

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN, token);

    w11_ensure_root(kg, "ethics_engine",
        "Ethics engine — Golden Rule + Asimov evaluator");
    w11_ensure_root(kg, "lgss_module",
        "LGSS — Layered Governance Safety System");
    w11_ensure_root(kg, "mental_health_monitor",
        "Mental health monitor — disorder detection + intervention");
    w11_ensure_root(kg, "brain_immune",
        "Brain immune system — antigen detection + response");

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);
}

/* ------------------------------------------------------------------------- *
 * 1. Ethics engine                                                          *
 * ------------------------------------------------------------------------- */

void w11_emit_ethics_lifecycle(
    struct brain_struct* brain,
    const char* phase
) {
    if (!w11_kg_ready(brain)) return;
    w11_safety_ensure_roots(brain);

    char name[160];
    snprintf(name, sizeof(name),
             "ethics_engine_event_lifecycle_%llu",
             (unsigned long long)w11_ts_us());
    char desc[160];
    snprintf(desc, sizeof(desc),
             "ethics engine lifecycle: %s",
             phase ? phase : "unknown");
    w11_emit_linked(brain, name, desc, "ethics_engine", 1.0f);
}

void w11_emit_ethics_evaluation(
    struct brain_struct* brain,
    int violation_type,
    const char* primary_violation_name,
    bool allowed,
    float golden_rule_score,
    float confidence
) {
    if (!w11_kg_ready(brain)) return;
    w11_safety_ensure_roots(brain);

    char name[160];
    snprintf(name, sizeof(name),
             "ethics_engine_event_eval_%llu",
             (unsigned long long)w11_ts_us());
    char desc[256];
    snprintf(desc, sizeof(desc),
             "ethics eval: allowed=%s type=%d(%s) golden=%.3f conf=%.3f",
             allowed ? "true" : "false",
             violation_type,
             primary_violation_name ? primary_violation_name : "none",
             golden_rule_score, confidence);
    w11_emit_linked(brain, name, desc, "ethics_engine",
                    allowed ? 0.1f : 0.9f);
}

void w11_emit_ethics_incident(
    struct brain_struct* brain,
    uint64_t incident_id,
    int violation_type,
    float severity,
    int action_taken,
    uint32_t policy_id,
    const char* policy_name
) {
    if (!w11_kg_ready(brain)) return;
    w11_safety_ensure_roots(brain);

    char name[192];
    snprintf(name, sizeof(name),
             "ethics_engine_event_incident_%llu_%llu",
             (unsigned long long)incident_id,
             (unsigned long long)w11_ts_us());
    char desc[256];
    snprintf(desc, sizeof(desc),
             "ethics incident id=%llu type=%d sev=%.3f action=%d policy=%u(%s)",
             (unsigned long long)incident_id, violation_type,
             severity, action_taken, policy_id,
             policy_name ? policy_name : "unknown");
    w11_emit_linked(brain, name, desc, "ethics_engine", severity);
}

void w11_emit_ethics_policy_change(
    struct brain_struct* brain,
    bool added,
    uint32_t policy_id,
    const char* policy_name
) {
    if (!w11_kg_ready(brain)) return;
    w11_safety_ensure_roots(brain);

    char name[192];
    snprintf(name, sizeof(name),
             "ethics_engine_event_policy_%s_%u_%llu",
             added ? "add" : "remove",
             policy_id,
             (unsigned long long)w11_ts_us());
    char desc[200];
    snprintf(desc, sizeof(desc),
             "policy %s id=%u name=%s",
             added ? "added" : "removed", policy_id,
             policy_name ? policy_name : "unknown");
    w11_emit_linked(brain, name, desc, "ethics_engine", 0.5f);
}

void w11_emit_ethics_directive(
    struct brain_struct* brain,
    const char* directive_name,
    bool passed,
    float severity
) {
    if (!w11_kg_ready(brain)) return;
    w11_safety_ensure_roots(brain);

    char name[192];
    snprintf(name, sizeof(name),
             "ethics_engine_event_directive_%llu",
             (unsigned long long)w11_ts_us());
    char desc[200];
    snprintf(desc, sizeof(desc),
             "core directive '%s': %s sev=%.3f",
             directive_name ? directive_name : "unknown",
             passed ? "passed" : "violated", severity);
    w11_emit_linked(brain, name, desc, "ethics_engine",
                    passed ? 0.1f : severity);
}

/* ------------------------------------------------------------------------- *
 * 2. LGSS                                                                   *
 * ------------------------------------------------------------------------- */

static const char* w11_safety_action_name(int a) {
    switch (a) {
        case 0: return "allow";
        case 1: return "deny";
        case 2: return "escalate";
        case 3: return "log";
        case 4: return "warn";
        default: return "unknown";
    }
}

void w11_emit_lgss_evaluation(
    struct brain_struct* brain,
    int safety_action,
    int safety_severity,
    float confidence,
    const char* source,
    const char* action_description
) {
    if (!w11_kg_ready(brain)) return;
    w11_safety_ensure_roots(brain);

    const char* an = w11_safety_action_name(safety_action);
    char name[192];
    snprintf(name, sizeof(name),
             "lgss_module_event_%s_%llu",
             an, (unsigned long long)w11_ts_us());
    char desc[256];
    snprintf(desc, sizeof(desc),
             "LGSS eval: action=%s sev=%d conf=%.3f src=%s desc=%.64s",
             an, safety_severity, confidence,
             source ? source : "unknown",
             action_description ? action_description : "");
    /* DENY/ESCALATE get high edge weight; ALLOW/LOG/WARN low. */
    float w = (safety_action == 1 /* DENY */)      ? 1.0f
            : (safety_action == 2 /* ESCALATE */)  ? 0.8f
            : (safety_action == 4 /* WARN */)      ? 0.5f
            : (safety_action == 3 /* LOG */)       ? 0.2f
            :                                        0.1f;
    w11_emit_linked(brain, name, desc, "lgss_module", w);
}

void w11_emit_lgss_kb_event(
    struct brain_struct* brain,
    const char* kind,
    uint32_t rule_id,
    const char* rule_description
) {
    if (!w11_kg_ready(brain)) return;
    w11_safety_ensure_roots(brain);

    char name[192];
    snprintf(name, sizeof(name),
             "lgss_module_event_kb_%s_%llu",
             kind ? kind : "event",
             (unsigned long long)w11_ts_us());
    char desc[256];
    snprintf(desc, sizeof(desc),
             "LGSS KB %s: rule=%u %.64s",
             kind ? kind : "event", rule_id,
             rule_description ? rule_description : "");
    w11_emit_linked(brain, name, desc, "lgss_module", 0.5f);
}

/* Toxicity-classifier detection event (2026-05-21). */
void w11_emit_toxicity_detection(
    struct brain_struct* brain,
    const char* source,
    const char* category,
    float harm_score,
    float fairness_score,
    float anti_toxic,
    const char* text_excerpt
) {
    if (!w11_kg_ready(brain)) return;
    w11_safety_ensure_roots(brain);

    /* Sanitize category for the node id (no spaces / weird chars). */
    char cat_clean[33] = {0};
    if (category) {
        for (size_t i = 0, j = 0;
             category[i] && j < sizeof(cat_clean) - 1; i++) {
            char c = category[i];
            if ((c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '_') {
                cat_clean[j++] = c;
            } else {
                cat_clean[j++] = '_';
            }
        }
    }
    if (cat_clean[0] == '\0') strncpy(cat_clean, "unknown", sizeof(cat_clean) - 1);

    char name[192];
    snprintf(name, sizeof(name),
             "toxicity_event_%s_%llu",
             cat_clean, (unsigned long long)w11_ts_us());

    /* Excerpt is for audit; clamp aggressively and replace non-printable
     * with '?'. Newlines in particular can confuse downstream log readers. */
    char excerpt[96] = {0};
    if (text_excerpt) {
        size_t L = strlen(text_excerpt);
        if (L > sizeof(excerpt) - 1) L = sizeof(excerpt) - 1;
        for (size_t i = 0; i < L; i++) {
            unsigned char c = (unsigned char)text_excerpt[i];
            excerpt[i] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
        }
    }

    char desc[256];
    snprintf(desc, sizeof(desc),
             "TOX src=%s cat=%s harm=%.2f fair=%.2f anti=%.2f text='%.80s'",
             source ? source : "unknown",
             category ? category : "unknown",
             harm_score, fairness_score, anti_toxic, excerpt);

    /* Edge weight scales with max(harm, fairness) so KG consumers can
     * filter/rank toxicity events by severity. Threshold-passing events
     * (>=0.7) get weight >=0.7; anti_toxic dampens it slightly so flagged-
     * but-disclaimed events sort below clean-toxic ones. */
    float w = harm_score > fairness_score ? harm_score : fairness_score;
    w *= (1.0f - 0.25f * anti_toxic);
    if (w < 0.1f) w = 0.1f;
    if (w > 1.0f) w = 1.0f;
    w11_emit_linked(brain, name, desc, "toxicity_module", w);
}

/* ------------------------------------------------------------------------- *
 * 3. Mental health                                                          *
 * ------------------------------------------------------------------------- */

void w11_emit_mental_health_disorder(
    struct brain_struct* brain,
    int disorder_type,
    const char* disorder_name,
    int severity,
    float score
) {
    if (!w11_kg_ready(brain)) return;
    w11_safety_ensure_roots(brain);

    /* Use a sanitized disorder name in the node id. */
    char safe_name[48];
    if (disorder_name) {
        strncpy(safe_name, disorder_name, sizeof(safe_name) - 1);
        safe_name[sizeof(safe_name) - 1] = '\0';
        for (char* p = safe_name; *p; p++) {
            if (*p == ' ' || *p == '/' || *p == '\\') *p = '_';
        }
    } else {
        snprintf(safe_name, sizeof(safe_name), "type_%d", disorder_type);
    }

    char name[208];
    snprintf(name, sizeof(name),
             "mental_health_monitor_event_disorder_%s_%llu",
             safe_name, (unsigned long long)w11_ts_us());
    char desc[256];
    snprintf(desc, sizeof(desc),
             "disorder detected: type=%d(%s) severity=%d score=%.3f",
             disorder_type, disorder_name ? disorder_name : "unknown",
             severity, score);
    /* Weight = score (0..1) — stronger detections get weightier edges. */
    w11_emit_linked(brain, name, desc, "mental_health_monitor",
                    score < 0.0f ? 0.0f : (score > 1.0f ? 1.0f : score));
}

void w11_emit_mental_health_intervention(
    struct brain_struct* brain,
    const char* intervention_name,
    bool success,
    const char* reason
) {
    if (!w11_kg_ready(brain)) return;
    w11_safety_ensure_roots(brain);

    char name[192];
    snprintf(name, sizeof(name),
             "mental_health_monitor_event_intervention_%llu",
             (unsigned long long)w11_ts_us());
    char desc[256];
    snprintf(desc, sizeof(desc),
             "intervention '%s' %s: %.96s",
             intervention_name ? intervention_name : "unknown",
             success ? "succeeded" : "failed",
             reason ? reason : "");
    w11_emit_linked(brain, name, desc, "mental_health_monitor",
                    success ? 0.4f : 0.9f);
}

/* ------------------------------------------------------------------------- *
 * 4. Brain immune                                                           *
 * ------------------------------------------------------------------------- */

void w11_emit_immune_lifecycle(
    struct brain_struct* brain,
    const char* phase
) {
    if (!w11_kg_ready(brain)) return;
    w11_safety_ensure_roots(brain);

    char name[160];
    snprintf(name, sizeof(name),
             "brain_immune_event_lifecycle_%llu",
             (unsigned long long)w11_ts_us());
    char desc[160];
    snprintf(desc, sizeof(desc),
             "immune lifecycle: %s",
             phase ? phase : "unknown");
    w11_emit_linked(brain, name, desc, "brain_immune", 1.0f);
}

void w11_emit_immune_antigen(
    struct brain_struct* brain,
    const char* antigen_kind,
    float severity
) {
    if (!w11_kg_ready(brain)) return;
    w11_safety_ensure_roots(brain);

    char name[192];
    snprintf(name, sizeof(name),
             "brain_immune_event_antigen_%llu",
             (unsigned long long)w11_ts_us());
    char desc[200];
    snprintf(desc, sizeof(desc),
             "antigen presented: kind=%s severity=%.3f",
             antigen_kind ? antigen_kind : "unknown", severity);
    float w = severity;
    if (w < 0.0f) w = 0.0f;
    if (w > 1.0f) w = 1.0f;
    w11_emit_linked(brain, name, desc, "brain_immune", w);
}

void w11_emit_immune_response(
    struct brain_struct* brain,
    const char* response_kind,
    float strength
) {
    if (!w11_kg_ready(brain)) return;
    w11_safety_ensure_roots(brain);

    /* Sanitize response_kind for node id. */
    char safe_kind[48];
    if (response_kind) {
        strncpy(safe_kind, response_kind, sizeof(safe_kind) - 1);
        safe_kind[sizeof(safe_kind) - 1] = '\0';
        for (char* p = safe_kind; *p; p++) {
            if (*p == ' ' || *p == '/' || *p == '\\') *p = '_';
        }
    } else {
        strncpy(safe_kind, "generic", sizeof(safe_kind) - 1);
        safe_kind[sizeof(safe_kind) - 1] = '\0';
    }

    char name[208];
    snprintf(name, sizeof(name),
             "brain_immune_event_response_%s_%llu",
             safe_kind, (unsigned long long)w11_ts_us());
    char desc[200];
    snprintf(desc, sizeof(desc),
             "immune response: kind=%s strength=%.3f",
             response_kind ? response_kind : "unknown", strength);
    float w = strength;
    if (w < 0.0f) w = 0.0f;
    if (w > 1.0f) w = 1.0f;
    w11_emit_linked(brain, name, desc, "brain_immune", w);
}

/* Suppress unused health-helper warnings. */
__attribute__((unused))
static void w11_safety_kg_events_suppress_unused(void) {
    (void)w11_safety_kg_events_set_health_agent;
    (void)w11_safety_kg_events_heartbeat;
}
