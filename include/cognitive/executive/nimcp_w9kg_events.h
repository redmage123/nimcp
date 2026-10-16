/**
 * @file nimcp_w9kg_events.h
 * @brief Wave W9-kg: KG root/event/query helpers for the cognitive-control
 *        family (attention / working memory / executive / introspection /
 *        self_model / global_workspace / self_awareness_extended /
 *        autobiographical_memory).
 *
 * WHAT: Shared helper API that complements the already-WIRED
 *       `kg_module_init`-based self-node-only registration. Provides:
 *         (1) `w9kg_init_roots(brain)` — idempotent structural init of 8
 *             canonical root nodes (one per module in scope) under the
 *             `cognitive` umbrella.
 *         (2) Event emitters that write timestamped runtime event nodes
 *             (broadcast winners, WM items, executive decisions, etc.).
 *         (3) Read-path queries for hot-paths that want to consult KG state
 *             (prior salience events, related semantic nodes, prior ethics
 *             events, wellbeing events, autobiographical events).
 *
 * WHY:  The W9-kg modules are PARTIAL or UNWIRED — each has at most a
 *       self-node registered via `kg_module_init`. Without read-paths the
 *       KG doesn't influence their hot-path decisions; without write-paths
 *       other modules (W16 consumers) can't see their events. This helper
 *       closes both gaps in ~50 LOC per module.
 *
 * HOW:  Follows the reference pattern in
 *       `src/cognitive/memory/nimcp_memory_kg_events.c` (W6):
 *         - Every emit self-elevates via `brain->internal_kg_admin_token`,
 *           writes, restores READ.
 *         - NULL-safe — passing a NULL brain no-ops cleanly.
 *         - Canonical node names from `docs/claude/kg-node-naming-registry.md`.
 *
 * @date 2026-04-24
 * @version 1.0.0 (W9-kg)
 */

#ifndef NIMCP_W9KG_EVENTS_H
#define NIMCP_W9KG_EVENTS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct brain_struct;

/* =========================================================================
 * Structural init (idempotent)
 * ========================================================================= */

/**
 * @brief Register the 8 W9-kg cognitive-control root nodes, plus any
 *        missing `cognitive` umbrella. Idempotent — safe across checkpoint
 *        reloads.
 *
 * Nodes created:
 *   - cog_attention
 *   - cog_working_memory
 *   - cog_executive
 *   - cog_introspection
 *   - cog_self_model
 *   - cog_global_workspace
 *   - cog_self_awareness_extended
 *   - cog_autobiographical_memory
 *
 * Each root links via BRAIN_KG_EDGE_INTEGRATES_WITH to the `cognitive`
 * umbrella (also created if missing).
 *
 * @param brain Brain handle (NULL-safe).
 * @return 0 on success, -1 on KG error (partial init is acceptable).
 */
int w9kg_init_roots(struct brain_struct* brain);

/**
 * @brief Register a brain pointer for hot-path emit calls in modules that
 *        don't thread brain_t through their signatures. Called by
 *        `w9kg_init_roots`; re-exposed so tests can override.
 */
void w9kg_set_registered_brain(struct brain_struct* brain);
struct brain_struct* w9kg_get_registered_brain(void);

/* =========================================================================
 * WRITE-path — runtime event emitters
 * ========================================================================= */

/* --- attention --- */
void w9kg_emit_attention_salience(struct brain_struct* brain,
                                  uint32_t head_idx,
                                  float salience);
void w9kg_emit_attention_focus_shift(struct brain_struct* brain,
                                     uint32_t from_module,
                                     uint32_t to_module,
                                     float strength);

/* --- working memory --- */
void w9kg_emit_wm_item_stored(struct brain_struct* brain,
                              uint64_t item_id,
                              float priority);
void w9kg_emit_wm_item_evicted(struct brain_struct* brain,
                               uint64_t item_id);

/* --- executive --- */
void w9kg_emit_executive_decision(struct brain_struct* brain,
                                  uint64_t decision_id,
                                  float confidence);
void w9kg_emit_executive_task_switch(struct brain_struct* brain,
                                     uint32_t from_task,
                                     uint32_t to_task);

/* --- introspection --- */
void w9kg_emit_introspection_report(struct brain_struct* brain,
                                    uint64_t report_id,
                                    float health_score);

/* --- self_model --- */
void w9kg_emit_self_model_update(struct brain_struct* brain,
                                 uint64_t update_id,
                                 float drift);

/* --- global_workspace --- */
void w9kg_emit_gws_broadcast(struct brain_struct* brain,
                             uint32_t winner_module,
                             float strength,
                             uint32_t broadcast_id);
void w9kg_emit_gws_competition(struct brain_struct* brain,
                               uint32_t num_competitors);

/* --- self_awareness_extended --- */
void w9kg_emit_self_awareness_reflection(struct brain_struct* brain,
                                         uint32_t reflection_id,
                                         float confidence);

/* --- autobiographical memory --- */
void w9kg_emit_autobio_stored(struct brain_struct* brain,
                              uint64_t memory_id,
                              float importance);
void w9kg_emit_autobio_retrieved(struct brain_struct* brain,
                                 uint64_t memory_id);

/* =========================================================================
 * READ-path — hot-path queries that consult the KG
 * ========================================================================= */

/**
 * @brief Count recent salience events currently in the KG (prefix
 *        "cog_attention_event_salience_"). Attention hot-path uses this
 *        to bias gating — more recent salience → heightened alert.
 * @return event count, 0 if KG unavailable.
 */
uint32_t w9kg_query_attention_salience_count(struct brain_struct* brain);

/**
 * @brief Check whether a named semantic-memory node is visible in the KG.
 *        WM uses this to know whether to prefer KG-backed retrieval over
 *        its own bounded store.
 */
bool w9kg_query_wm_has_semantic(struct brain_struct* brain,
                                const char* concept_name);

/**
 * @brief Count ethics event nodes in the KG (any name containing
 *        "ethics_event_"). Executive uses this as an alignment signal —
 *        nonzero count nudges decisions toward safer policies.
 * @return event count, 0 if KG unavailable.
 */
uint32_t w9kg_query_executive_ethics_events(struct brain_struct* brain);

/**
 * @brief Count wellbeing event nodes in the KG (any name containing
 *        "wellbeing_event_"). Introspection uses this for
 *        health/self-report context.
 */
uint32_t w9kg_query_introspection_wellbeing(struct brain_struct* brain);

/**
 * @brief Count autobiographical event nodes in the KG. Self-model uses
 *        this to index how much historical self-evidence is available.
 */
uint32_t w9kg_query_self_model_autobio(struct brain_struct* brain);

/**
 * @brief Count outgoing edges from `cog_global_workspace`. GWS uses this
 *        to discover how many subscribers/sinks are reachable via the KG
 *        (complements its in-process subscriber list).
 */
uint32_t w9kg_query_gws_winners_by_salience(struct brain_struct* brain);

/**
 * @brief Count total introspection event nodes. self_awareness_extended
 *        uses this to bias its reflection cadence.
 */
uint32_t w9kg_query_self_awareness_events(struct brain_struct* brain);

/**
 * @brief Count autobiographical event nodes. autobiographical_memory's
 *        consolidation path uses this to know when external (KG) evidence
 *        converges with internal storage.
 */
uint32_t w9kg_query_autobio_event_count(struct brain_struct* brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_W9KG_EVENTS_H */
