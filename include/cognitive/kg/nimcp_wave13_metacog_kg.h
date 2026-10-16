//=============================================================================
// nimcp_wave13_metacog_kg.h - Wave W13: Curiosity / Meta / Consolidation / Sleep
//=============================================================================
/**
 * @file nimcp_wave13_metacog_kg.h
 * @brief Wave W13 emit + query helpers for 9 meta-cognitive modules.
 *
 * WHY: W13 wires the following `src/cognitive/` modules into
 *      `brain->internal_kg` with structural root nodes + runtime emit events
 *      + one query/read-back each:
 *
 *        1. curiosity/nimcp_curiosity.c            — cog_curiosity
 *        2. curiosity/nimcp_information_forager.c  — cog_information_forager
 *        3. meta_learning/nimcp_meta_learning.c    — cog_meta_learning
 *        4. consolidation/nimcp_consolidation.c    — cog_consolidation_cognitive
 *           (NOTE: separate from cog_memory_systems_consolidation in W6;
 *            this module lives in src/cognitive/consolidation/ whereas W6
 *            covered src/cognitive/memory/nimcp_systems_consolidation*.c)
 *        5. sleep_wake/nimcp_sleep_wake.c          — cog_sleep_wake
 *        6. nimcp_self_curriculum.c                — cog_self_curriculum
 *        7. nimcp_analogical_transfer.c            — cog_analogical_transfer
 *        8. nimcp_multiscale_memory.c              — cog_multiscale_memory
 *        9. nimcp_contrastive_self.c               — cog_contrastive_self
 *
 * Template: W10's `nimcp_wave10_affective_kg.h/.c` shared-helper file pattern.
 *
 * Each emit function:
 *   - is null/disabled/kg-missing tolerant (silent no-op),
 *   - self-elevates to ADMIN via `brain->internal_kg_admin_token`
 *     (see docs/claude/kg-node-naming-registry.md §7),
 *   - emits a unique event node `<owner>_event_<kind>_<ts>` linked back
 *     to the structural root via BRAIN_KG_EDGE_SENDS_TO,
 *   - restores READ access before returning.
 *
 * Each query function returns a single float (bias value in [0,1] or a
 * neutral 0.5f when no relevant state node is present). Curiosity additionally
 * has a "recent surprise" read-path used by its own gap detector.
 *
 * @date 2026-04-24
 */

#ifndef NIMCP_WAVE13_METACOG_KG_H
#define NIMCP_WAVE13_METACOG_KG_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

struct brain_struct;

/* ---------------------------------------------------------------------------
 * Structural root init — called once per brain at init time. Idempotent.
 * Creates all 9 root nodes + the cross-edges that make sense without runtime
 * state (e.g. curiosity drives meta_learning rate, sleep triggers
 * consolidation, etc.).
 * --------------------------------------------------------------------------- */
NIMCP_EXPORT int nimcp_wave13_metacog_kg_init(struct brain_struct* brain);

/* ---------------------------------------------------------------------------
 * 1. curiosity: emit a knowledge-gap detection / curiosity-spike event.
 * --------------------------------------------------------------------------- */
NIMCP_EXPORT void wave13_curiosity_emit_gap(
    struct brain_struct* brain,
    const char* concept_str,
    float gap_size,
    float curiosity_intensity);

/** Returns last-spike intensity bias ([0,1], else 0.5f neutral). */
NIMCP_EXPORT float wave13_curiosity_query_spike_bias(
    struct brain_struct* brain);

/** Returns last recorded surprise (gap_size); used by gap detectors.
 *  Default 0.0f means "no recent surprise". */
NIMCP_EXPORT float wave13_curiosity_query_recent_surprise(
    struct brain_struct* brain);

/* ---------------------------------------------------------------------------
 * 2. information_forager: emit a forage learn / tick outcome event.
 * --------------------------------------------------------------------------- */
NIMCP_EXPORT void wave13_forager_emit_learn(
    struct brain_struct* brain,
    const char* topic,
    float quality_score,
    int result_code);

/** Returns stored quality-score bias, else 0.5f. */
NIMCP_EXPORT float wave13_forager_query_quality_bias(
    struct brain_struct* brain);

/* ---------------------------------------------------------------------------
 * 3. meta_learning: emit LR-adaptation / inner-loop completion event.
 * --------------------------------------------------------------------------- */
NIMCP_EXPORT void wave13_meta_emit_lr_adaptation(
    struct brain_struct* brain,
    int region_type,
    float old_lr,
    float new_lr,
    float loss);

NIMCP_EXPORT void wave13_meta_emit_inner_loop(
    struct brain_struct* brain,
    uint32_t num_steps,
    float final_loss);

/** Returns adaptation magnitude bias (|new-old|/old), else 0.5f. */
NIMCP_EXPORT float wave13_meta_query_adaptation_bias(
    struct brain_struct* brain);

/* ---------------------------------------------------------------------------
 * 4. consolidation (cognitive module — not W6's memory/systems_consolidation).
 * --------------------------------------------------------------------------- */
NIMCP_EXPORT void wave13_consolidation_emit_cycle(
    struct brain_struct* brain,
    uint32_t synapses_consolidated,
    uint32_t synapses_pruned,
    float duration_ms);

/** Returns stored consolidation-load bias, else 0.5f. */
NIMCP_EXPORT float wave13_consolidation_query_load_bias(
    struct brain_struct* brain);

/* ---------------------------------------------------------------------------
 * 5. sleep_wake: emit sleep-stage transition events.
 * --------------------------------------------------------------------------- */
NIMCP_EXPORT void wave13_sleep_emit_stage_transition(
    struct brain_struct* brain,
    int from_state,
    int to_state,
    float pressure);

/** Returns stored sleep-pressure bias, else 0.5f. */
NIMCP_EXPORT float wave13_sleep_query_pressure_bias(
    struct brain_struct* brain);

/* ---------------------------------------------------------------------------
 * 6. self_curriculum: emit curriculum-advancement / item-generation event.
 * --------------------------------------------------------------------------- */
NIMCP_EXPORT void wave13_curriculum_emit_uncertainty_update(
    struct brain_struct* brain,
    const char* domain,
    float uncertainty);

NIMCP_EXPORT void wave13_curriculum_emit_item_generated(
    struct brain_struct* brain,
    const char* domain,
    uint32_t item_count);

/** Returns stored uncertainty bias, else 0.5f. */
NIMCP_EXPORT float wave13_curriculum_query_uncertainty_bias(
    struct brain_struct* brain);

/* ---------------------------------------------------------------------------
 * 7. analogical_transfer: emit an analogy-match / transfer-apply event.
 * --------------------------------------------------------------------------- */
NIMCP_EXPORT void wave13_analogy_emit_match(
    struct brain_struct* brain,
    const char* label,
    float similarity,
    float success_score);

/** Returns stored similarity bias, else 0.5f. */
NIMCP_EXPORT float wave13_analogy_query_similarity_bias(
    struct brain_struct* brain);

/* ---------------------------------------------------------------------------
 * 8. multiscale_memory: emit a multi-timescale push / query event.
 * --------------------------------------------------------------------------- */
NIMCP_EXPORT void wave13_multiscale_emit_push(
    struct brain_struct* brain,
    const char* label,
    float importance);

/** Returns stored importance bias, else 0.5f. */
NIMCP_EXPORT float wave13_multiscale_query_importance_bias(
    struct brain_struct* brain);

/* ---------------------------------------------------------------------------
 * 9. contrastive_self: emit a contrastive-record / negative-generation event.
 * --------------------------------------------------------------------------- */
NIMCP_EXPORT void wave13_contrastive_emit_record(
    struct brain_struct* brain,
    const char* label,
    uint32_t buffer_count);

/** Returns stored buffer-fill bias in [0,1], else 0.5f. */
NIMCP_EXPORT float wave13_contrastive_query_buffer_bias(
    struct brain_struct* brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WAVE13_METACOG_KG_H */
