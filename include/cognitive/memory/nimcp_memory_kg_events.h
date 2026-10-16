/**
 * @file nimcp_memory_kg_events.h
 * @brief Wave W6: KG event emitters for the cognitive-memory family
 *
 * WHAT: Shared helper API for memory-family modules to emit KG events
 *       (write-path) and query KG state (read-path).
 * WHY:  The memory family (engram, semantic, hopfield, episodic replay,
 *       systems consolidation, schemas, source memory, reconsolidation)
 *       IS the brain's knowledge store — bidirectional KG integration is
 *       required so symbolic reasoning can consult and enrich memory state.
 * HOW:  Each emit function self-elevates the KG access level using
 *       `brain->internal_kg_admin_token`, writes one event/structural node
 *       + an edge back to the module's root node, then restores READ access.
 *       All functions are null-safe; passing a NULL brain no-ops cleanly so
 *       unit tests and back-compat callers don't need to thread brain_t
 *       through every memory call.
 *
 * DESIGN NOTES:
 *   - `brain_t` is a forward-declared `struct brain_struct*`. Callers that
 *     already have it should pass it directly. Callers that don't may call
 *     `memory_kg_events_get_registered_brain()` which returns whatever brain
 *     pointer was set at init time via `memory_kg_events_set_registered_brain()`.
 *   - All module-root node names match the canonical `cog_memory_*` pattern
 *     from `docs/claude/kg-node-naming-registry.md`.
 *
 * Reference template: `src/cognitive/memory/core/nimcp_pr_kg_bridge.c`
 *
 * @date 2026-04-24
 * @version 1.0.0 (W6)
 */

#ifndef NIMCP_MEMORY_KG_EVENTS_H
#define NIMCP_MEMORY_KG_EVENTS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward decl to keep this header free of brain internals */
struct brain_struct;

/* ========================================================================= */
/* Structural init (idempotent)                                              */
/* ========================================================================= */

/**
 * @brief Register the 8 memory-family root nodes in the KG, plus the
 *        `cognitive` umbrella. Idempotent — safe to call on checkpoint
 *        reload.
 *
 * Root nodes created:
 *   - cog_memory_engram
 *   - cog_memory_semantic
 *   - cog_memory_hopfield
 *   - cog_memory_episodic_replay
 *   - cog_memory_systems_consolidation
 *   - cog_memory_schemas
 *   - cog_memory_source
 *   - cog_memory_reconsolidation
 *   - cognitive (umbrella)
 *
 * Each root is linked via BRAIN_KG_EDGE_INTEGRATES_WITH to `cognitive`.
 *
 * @param brain Brain handle (NULL-safe no-op).
 * @return 0 on success, -1 on any KG error (most individual node failures
 *         are logged but ignored — partial registration is acceptable).
 */
int memory_kg_init_roots(struct brain_struct* brain);

/* ========================================================================= */
/* Registered brain pointer (lets module hot-paths emit without threading    */
/* brain_t through every signature)                                          */
/* ========================================================================= */

/**
 * @brief Store the active brain pointer so hot-path emit calls can be made
 *        from modules that don't currently receive brain_t. Called once
 *        during brain init by `memory_kg_init_roots`.
 */
void memory_kg_events_set_registered_brain(struct brain_struct* brain);

/**
 * @brief Retrieve the registered brain pointer. May return NULL if brain
 *        init hasn't run or the brain was destroyed.
 */
struct brain_struct* memory_kg_events_get_registered_brain(void);

/* ========================================================================= */
/* WRITE path — event emitters (all null-safe, all admin-elevate + restore) */
/* ========================================================================= */

/* --- engram --- */
void memory_kg_emit_engram_form(struct brain_struct* brain,
                                uint64_t engram_id, float strength);
void memory_kg_emit_engram_recall(struct brain_struct* brain,
                                  uint64_t engram_id, float confidence);

/* --- semantic memory --- */
void memory_kg_emit_concept_created(struct brain_struct* brain,
                                    uint64_t concept_id, const char* label);
void memory_kg_emit_spreading_activation(struct brain_struct* brain,
                                         uint64_t seed_concept_id,
                                         uint32_t activated_count);

/* --- hopfield --- */
void memory_kg_emit_pattern_stored(struct brain_struct* brain,
                                   uint32_t pattern_id, float strength);
void memory_kg_emit_pattern_completed(struct brain_struct* brain,
                                      uint32_t pattern_id,
                                      float similarity, bool converged);

/* --- episodic replay --- */
void memory_kg_emit_replay_cycle(struct brain_struct* brain,
                                 uint32_t replayed_count,
                                 uint32_t total_buffer);

/* --- systems consolidation --- */
void memory_kg_emit_consolidation_transfer(struct brain_struct* brain,
                                           uint64_t engram_id,
                                           uint64_t cortical_node_id,
                                           float consolidation_strength);

/* --- schemas --- */
void memory_kg_emit_schema_added(struct brain_struct* brain,
                                 uint64_t schema_id, const char* name);
void memory_kg_emit_schema_activated(struct brain_struct* brain,
                                     uint64_t schema_id);

/* --- source memory --- */
void memory_kg_emit_source_bound(struct brain_struct* brain,
                                 uint64_t memory_id, int source_type);

/* --- reconsolidation --- */
void memory_kg_emit_reconsolidation_opened(struct brain_struct* brain,
                                           uint64_t memory_id,
                                           float activation_strength);
void memory_kg_emit_reconsolidation_committed(struct brain_struct* brain,
                                              uint64_t memory_id,
                                              int outcome);

/* ========================================================================= */
/* READ path — queries that let hot paths consult the KG                    */
/* ========================================================================= */

/**
 * @brief Check whether an engram-id's event node is registered in the KG.
 * @return true if `cog_memory_engram_event_form_<id>` exists.
 */
bool memory_kg_has_engram_event(struct brain_struct* brain, uint64_t engram_id);

/**
 * @brief Count the outgoing edges from the semantic-memory root — a proxy
 *        for "does the KG know about semantic structure I can lean on".
 * @return edge count, or 0 if KG unavailable.
 */
uint32_t memory_kg_semantic_neighbor_count(struct brain_struct* brain);

/**
 * @brief Check whether a schema with the given name is present in the KG.
 * @return true if found.
 */
bool memory_kg_has_schema(struct brain_struct* brain, const char* schema_name);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MEMORY_KG_EVENTS_H */
