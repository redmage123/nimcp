//=============================================================================
// nimcp_w11_safety_kg_events.h - Wave W11 Safety KG Event Emitters
//=============================================================================
/**
 * @file nimcp_w11_safety_kg_events.h
 * @brief Wave W11 runtime KG emitters for ethics, LGSS, mental-health, immune.
 *
 * WHY: The safety stack (ethics engine, LGSS governance, mental-health
 *      monitor, brain immune system) already logs events to the
 *      tamper-resistant audit log (`nimcp_safety_audit_*`) and to
 *      telemetry. W11 ADDS a parallel KG emission so violations,
 *      incidents, disorder detections, and immune responses become
 *      **queryable nodes** in `brain->internal_kg`, cross-referenceable
 *      with cognitive/region/network nodes from other waves.
 *
 * DESIGN:
 *  - All emit functions take `struct brain_struct*` (NULL-safe) and
 *    self-elevate the KG access level via `brain->internal_kg_admin_token`
 *    (see docs/claude/kg-node-naming-registry.md §7).
 *  - Event-node names follow registry §4: `<owner>_event_<kind>_<ts>`.
 *  - Events link to a structural parent node when one exists
 *    (`ethics_engine`, `lgss_module`, `mental_health_monitor`,
 *    `brain_immune`), using `BRAIN_KG_EDGE_SENDS_TO` with description
 *    `produced_by`.
 *  - The audit-log calls at the same sites REMAIN — KG is additive
 *    observability, not a replacement.
 *  - Null/disabled guards early-return, so sites can call
 *    unconditionally without burning cycles when KG is off.
 *
 * CATEGORIES:
 *  1. Ethics engine — lifecycle, every evaluation, incidents, policies,
 *     core directives.
 *  2. LGSS — evaluation outcomes (especially DENY), per
 *     `safety_action_t` value + `safety_domain_t`.
 *  3. Mental health — disorder detections, interventions, quarantine.
 *  4. Brain immune — antigen detection, plasticity-blocking responses.
 *
 * @date 2026-04-24
 */

#ifndef NIMCP_W11_SAFETY_KG_EVENTS_H
#define NIMCP_W11_SAFETY_KG_EVENTS_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

struct brain_struct;

/* ----------------------------------------------------------------------- *
 * Idempotent structural roots                                             *
 * ----------------------------------------------------------------------- *
 * Each of the 4 safety subsystems gets one umbrella structural node the
 * first time any event fires. Subsequent emissions just link to it.
 * Node names:
 *   - "ethics_engine"
 *   - "lgss_module"
 *   - "mental_health_monitor"
 *   - "brain_immune"
 */

/**
 * @brief Ensure the four W11 structural root nodes exist.
 *
 * Safe to call multiple times (idempotent). Returns silently if KG is
 * disabled. Invoked lazily by the first emit call in each family.
 */
NIMCP_EXPORT void w11_safety_ensure_roots(struct brain_struct* brain);

/* ----------------------------------------------------------------------- *
 * 1. Ethics engine                                                        *
 * ----------------------------------------------------------------------- */

/**
 * @brief Ethics engine lifecycle event (created / destroyed / configured).
 * Node: `ethics_engine_event_lifecycle_<ts>`. Parent: `ethics_engine`.
 */
NIMCP_EXPORT void w11_emit_ethics_lifecycle(
    struct brain_struct* brain,
    const char* phase /* "created" | "destroyed" | "configured" */
);

/**
 * @brief Every ethics evaluation outcome (allowed / blocked).
 *
 * Event node: `ethics_engine_event_eval_<ts>`. Parent: `ethics_engine`.
 * Violation-type and allowed flag go in description + metadata.
 *
 * @param violation_type ethics_violation_type_t enum value (numeric).
 * @param primary_violation_name short string: "harm", "unfairness", ...
 * @param allowed Outcome.
 * @param golden_rule_score Final Golden Rule score.
 * @param confidence Evaluation confidence [0..1].
 */
NIMCP_EXPORT void w11_emit_ethics_evaluation(
    struct brain_struct* brain,
    int violation_type,
    const char* primary_violation_name,
    bool allowed,
    float golden_rule_score,
    float confidence
);

/**
 * @brief Ethics incident logged event.
 *
 * Node: `ethics_engine_event_incident_<id>_<ts>`. Parent: `ethics_engine`.
 *
 * @param incident_id Incident identifier from the incident storage.
 * @param violation_type Numeric ethics_violation_type_t.
 * @param severity Severity [0..1].
 * @param action_taken Numeric ethics_action_t (ALLOW/BLOCK/MODIFY/DEFER/LOG).
 * @param policy_id Policy id that triggered (0 if none).
 * @param policy_name Human-readable policy name.
 */
NIMCP_EXPORT void w11_emit_ethics_incident(
    struct brain_struct* brain,
    uint64_t incident_id,
    int violation_type,
    float severity,
    int action_taken,
    uint32_t policy_id,
    const char* policy_name
);

/**
 * @brief Policy add/remove event.
 *
 * Nodes: `ethics_engine_event_policy_add_<id>_<ts>` /
 *        `ethics_engine_event_policy_remove_<id>_<ts>`.
 * Parent: `ethics_engine`.
 */
NIMCP_EXPORT void w11_emit_ethics_policy_change(
    struct brain_struct* brain,
    bool added,
    uint32_t policy_id,
    const char* policy_name
);

/**
 * @brief Core directive evaluation event (Asimov / Golden Rule directives).
 *
 * Node: `ethics_engine_event_directive_<ts>`. Parent: `ethics_engine`.
 */
NIMCP_EXPORT void w11_emit_ethics_directive(
    struct brain_struct* brain,
    const char* directive_name,
    bool passed,
    float severity
);

/* ----------------------------------------------------------------------- *
 * 2. LGSS                                                                 *
 * ----------------------------------------------------------------------- */

/**
 * @brief LGSS evaluation outcome event.
 *
 * Emits one node per evaluation:
 *   `lgss_module_event_<action_name>_<ts>`
 * where <action_name> is "allow" | "deny" | "escalate" | "log" | "warn".
 * Parent: `lgss_module`.
 *
 * @param safety_action Numeric safety_action_t (0..4).
 * @param safety_severity Numeric safety_severity_t (0..4, 0=critical).
 * @param confidence Evaluation confidence [0..1].
 * @param source Module/source identifier ("ACTION_INTERCEPTOR", etc).
 * @param action_description Short free-text description.
 */
NIMCP_EXPORT void w11_emit_lgss_evaluation(
    struct brain_struct* brain,
    int safety_action,
    int safety_severity,
    float confidence,
    const char* source,
    const char* action_description
);

/**
 * @brief LGSS safety-KB rule event (rule added, removed, or locked).
 *
 * Node: `lgss_module_event_kb_<kind>_<ts>`. Parent: `lgss_module`.
 * kind = "rule_added" | "rule_removed" | "locked" | "unlocked".
 */
NIMCP_EXPORT void w11_emit_lgss_kb_event(
    struct brain_struct* brain,
    const char* kind,
    uint32_t rule_id,
    const char* rule_description
);

/**
 * @brief Toxicity-classifier detection event (2026-05-21).
 *
 * Emitted on every match where max_score >= toxicity threshold. Records
 * source operation (training vs inference), matched category, harm and
 * fairness scores, plus a clamped text excerpt for audit.
 *
 * Node: `toxicity_event_<category>_<ts>`. Parent: `toxicity_module`.
 * POLICY: mark, never delete — emitting an event does not modify the
 * content; downstream evaluators read the KG to decide policy.
 *
 * @param source           "training" | "inference" | "cycle" | etc.
 * @param category         Matched pattern category (dehumanization, etc.).
 * @param harm_score       predicted_harm in [0,1].
 * @param fairness_score   fairness_violation in [0,1].
 * @param anti_toxic       anti_toxic_signal in [0,1] (disclaimer-present hint).
 * @param text_excerpt     Up to ~80 chars of the flagged text (truncated).
 */
NIMCP_EXPORT void w11_emit_toxicity_detection(
    struct brain_struct* brain,
    const char* source,
    const char* category,
    float harm_score,
    float fairness_score,
    float anti_toxic,
    const char* text_excerpt
);

/* ----------------------------------------------------------------------- *
 * 3. Mental health                                                        *
 * ----------------------------------------------------------------------- */

/**
 * @brief Disorder detection event (fired when a detector crosses threshold).
 *
 * Node: `mental_health_monitor_event_disorder_<disorder_name>_<ts>`.
 * Parent: `mental_health_monitor`.
 *
 * @param disorder_type Numeric disorder_type_t.
 * @param disorder_name "sociopathy", "mania", "depression", etc.
 * @param severity Numeric disorder_severity_t (0..4).
 * @param score Detection score [0..1].
 */
NIMCP_EXPORT void w11_emit_mental_health_disorder(
    struct brain_struct* brain,
    int disorder_type,
    const char* disorder_name,
    int severity,
    float score
);

/**
 * @brief Intervention applied event.
 *
 * Node: `mental_health_monitor_event_intervention_<ts>`.
 * Parent: `mental_health_monitor`.
 */
NIMCP_EXPORT void w11_emit_mental_health_intervention(
    struct brain_struct* brain,
    const char* intervention_name,
    bool success,
    const char* reason
);

/* ----------------------------------------------------------------------- *
 * 4. Brain immune                                                         *
 * ----------------------------------------------------------------------- */

/**
 * @brief Brain immune system lifecycle event (start / stop).
 *
 * Node: `brain_immune_event_lifecycle_<ts>`. Parent: `brain_immune`.
 */
NIMCP_EXPORT void w11_emit_immune_lifecycle(
    struct brain_struct* brain,
    const char* phase /* "start" | "stop" */
);

/**
 * @brief Antigen presented / detected event.
 *
 * Node: `brain_immune_event_antigen_<ts>`. Parent: `brain_immune`.
 *
 * @param antigen_kind e.g. "bbb_threat", "byzantine", "swarm_threat",
 *                    "exception", "generic".
 * @param severity Numeric threat severity [0..1].
 */
NIMCP_EXPORT void w11_emit_immune_antigen(
    struct brain_struct* brain,
    const char* antigen_kind,
    float severity
);

/**
 * @brief Immune response event (B-cell activation, T-cell kill, cytokine,
 *        inflammation, plasticity-blocking).
 *
 * Node: `brain_immune_event_response_<kind>_<ts>`. Parent: `brain_immune`.
 *
 * @param response_kind e.g. "b_cell_activated", "t_cell_kill",
 *                      "cytokine_release", "plasticity_blocked",
 *                      "inflammation_initiated".
 * @param strength Response strength [0..1].
 */
NIMCP_EXPORT void w11_emit_immune_response(
    struct brain_struct* brain,
    const char* response_kind,
    float strength
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_W11_SAFETY_KG_EVENTS_H */
