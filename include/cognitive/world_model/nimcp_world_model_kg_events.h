/**
 * @file nimcp_world_model_kg_events.h
 * @brief Wave W8: KG event emitters for the world-model / imagination / FEP /
 *        predictive / salience family.
 *
 * WHAT: Shared helper API for the W8 module set (omni world model,
 *       intuitive_physics, scene_graph, entity_tracker, world_simulator,
 *       free_energy, predictive, salience, imagination_workspace,
 *       jepa_brain_bridges) to emit KG events on the write path and query
 *       KG state on the read path.
 * WHY:  These modules collectively form the "simulated world" the brain
 *       reasons over. Making them write object-tracks, predictions,
 *       surprise events, and causal facts to the KG turns a silent
 *       physics-sim substrate into a symbolic consumer for reasoning.
 * HOW:  Each emit self-elevates via `brain->internal_kg_admin_token`,
 *       writes one event/structural node + an edge back to the module's
 *       root node, then restores READ access. Null-safe throughout.
 *
 * Reference template: `src/cognitive/memory/nimcp_memory_kg_events.c` (W6).
 *
 * Naming follows `docs/claude/kg-node-naming-registry.md`:
 *   - Roots use `cog_<family>_<name>` where `<family>` is one of
 *     `omni_wm`, `world_sim`, `imagination`, `predictive`, `salience`,
 *     `fep`, `jepa` (physics sub-modules use `cog_physics_*` to group
 *     them beneath an umbrella physics root).
 *   - Event nodes: `<root>_event_<kind>_<timestamp_us>`.
 *
 * @date 2026-04-24
 * @version 1.0.0 (W8)
 */

#ifndef NIMCP_WORLD_MODEL_KG_EVENTS_H
#define NIMCP_WORLD_MODEL_KG_EVENTS_H

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
 * @brief Register the 10 W8 root nodes in the KG. Idempotent.
 *
 * Root nodes created (all BRAIN_KG_NODE_COGNITIVE):
 *   - cog_omni_wm_world_model
 *   - cog_physics_intuitive
 *   - cog_physics_scene_graph
 *   - cog_physics_entity_tracker
 *   - cog_physics_world_simulator
 *   - cog_fep_free_energy
 *   - cog_predictive
 *   - cog_salience
 *   - cog_imagination_workspace
 *   - cog_jepa_bridges
 *
 * Each root links INTEGRATES_WITH the `cognitive` umbrella node (or
 * creates it if absent). If `imagination_engine` / `jepa_predictor`
 * nodes already exist from earlier waves, cross-edges are added.
 *
 * @param brain Brain handle (NULL-safe).
 * @return 0 on success, -1 on KG error. Partial success is OK.
 */
int world_model_kg_init_roots(struct brain_struct* brain);

/* ========================================================================= */
/* Registered brain pointer (lets module hot-paths emit without brain_t in  */
/* every signature — same pattern as memory_kg_events)                       */
/* ========================================================================= */

void world_model_kg_events_set_registered_brain(struct brain_struct* brain);
struct brain_struct* world_model_kg_events_get_registered_brain(void);

/* ========================================================================= */
/* WRITE path — per-module event emitters                                    */
/* ========================================================================= */

/* --- Omni world model --- */
void world_model_kg_emit_wm_step(struct brain_struct* brain,
                                 uint64_t step_id,
                                 float reward, float surprise);

/* --- Intuitive physics --- */
void world_model_kg_emit_physics_step(struct brain_struct* brain,
                                      uint64_t step_count,
                                      uint32_t active_objects,
                                      uint32_t contacts,
                                      float energy_drift);

/* --- Scene graph --- */
void world_model_kg_emit_scene_rebuild(struct brain_struct* brain,
                                       uint32_t num_relations,
                                       uint64_t rebuild_count);

/* --- Entity tracker --- */
void world_model_kg_emit_entity_event(struct brain_struct* brain,
                                      uint32_t entity_id,
                                      int event_kind,   /* 0=spawn, 1=occlude, 2=reappear, 3=lost */
                                      float confidence);

/* --- World simulator --- */
void world_model_kg_emit_sim_step(struct brain_struct* brain,
                                  uint64_t step_count,
                                  float dt, uint32_t num_engines);

/* --- Free energy / FEP --- */
void world_model_kg_emit_surprise(struct brain_struct* brain,
                                  float free_energy,
                                  float surprise,
                                  float precision);

/* --- Predictive processing --- */
void world_model_kg_emit_prediction_error(struct brain_struct* brain,
                                          uint32_t layer,
                                          float error_norm,
                                          float free_energy);

/* --- Salience transitions --- */
void world_model_kg_emit_salience_transition(struct brain_struct* brain,
                                             int old_level,
                                             int new_level,
                                             float combined_score);

/* --- Imagination workspace --- */
void world_model_kg_emit_workspace_scenario(struct brain_struct* brain,
                                            uint64_t scenario_id,
                                            uint32_t num_steps,
                                            float vividness);

/* --- JEPA brain bridges --- */
void world_model_kg_emit_jepa_tick(struct brain_struct* brain,
                                   uint32_t bridges_active,
                                   float average_loss);

/* ========================================================================= */
/* READ path — queries usable in hot paths                                   */
/* ========================================================================= */

/**
 * @brief Count outgoing edges from the given W8 root. Proxy for "does
 *        the KG already know about events from this module".
 */
uint32_t world_model_kg_root_edge_count(struct brain_struct* brain,
                                        const char* root_name);

/**
 * @brief Check for recent high-surprise / high-error marker nodes.
 * @return true if any `cog_fep_free_energy_event_high_surprise_*` or
 *         `cog_predictive_event_high_error_*` nodes exist.
 */
bool world_model_kg_has_recent_surprise(struct brain_struct* brain);

/**
 * @brief Check whether a named world-model / predictive partner node
 *        exists. Used by hot paths that want to know whether a peer
 *        system is online before emitting cross-edges.
 */
bool world_model_kg_has_partner(struct brain_struct* brain, const char* name);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WORLD_MODEL_KG_EVENTS_H */
