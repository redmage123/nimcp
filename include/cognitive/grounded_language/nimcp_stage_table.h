/**
 * @file nimcp_stage_table.h
 * @brief Slice E (Option-1 architectural rebuild) — developmental stage
 *        scaffolding for production.
 *
 * @date 2026-05-19
 *
 * WHAT:
 *   Per-stage constraints on what the brain *can* produce. Children's
 *   production complexity grows: 1-word -> 2-word -> telegraphic -> SVO.
 *   This table encodes that progression as a max-vocab cap (lexicon mask),
 *   a [min, max] word-count window, and a grammar-template bitmask. The
 *   communication cascade reads the table for the brain's current stage
 *   and enforces the caps in cascade_stage_motor; grounded_language uses
 *   the max_visible_vocab to install a per-stage bitmap mask over the
 *   lexicon during production.
 *
 * WHY:
 *   Pre-Slice-E, every output was unconstrained at every stage. Stage 0
 *   produced 3-word random salad instead of single words because nothing
 *   in the production path enforced developmental level. Length / vocab /
 *   grammar caps are the SCAFFOLDING that lets a child's brain produce
 *   stage-appropriate output without first having to learn every
 *   meta-rule.
 *
 * HOW:
 *   - Static table with one stage_constraints_t row per integer stage
 *     index (0..N). Out-of-range indices clamp to the last row.
 *   - stage_table_default() returns a "no constraints" row so callers can
 *     fall back when stage isn't set or is unknown — preserves the
 *     pre-Slice-E byte-identical contract.
 *   - allowed_grammar_mask is a uint32_t bitmask of grammar_template_id_t
 *     values. Set bits = "this template is allowed at this stage". The
 *     cascade checks the produced sequence against this mask via Broca
 *     CYK (when wired) or skips the grammar check (TODO).
 *
 * NOTES:
 *   - The table is read-only at runtime. No setters / no mutex.
 *   - Lexicon-frequency ordering for the mask is APPROXIMATE in the
 *     initial drop: we take the first N entries in lexicon insertion
 *     order. A future revision should re-rank by caregiver-frequency
 *     before applying the mask.
 *   - APPEND-ONLY: if you add a new grammar template, give it the next
 *     bit. Don't renumber existing bits — checkpoints written under the
 *     old layout would otherwise reinterpret the mask.
 */

#ifndef NIMCP_STAGE_TABLE_H
#define NIMCP_STAGE_TABLE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Grammar template id bitmask.
 *
 * One bit per recognized phrase template. Stage constraints combine
 * templates via bitwise OR; the cascade's grammar check passes iff the
 * produced sequence matches AT LEAST ONE bit in allowed_grammar_mask.
 *
 * GRAMMAR_FULL = 0xFFFFFFFF means "any template" — used for late stages
 * where the production system is no longer constrained to surface
 * scaffolding and Broca + Wernicke shape the output.
 */
typedef enum grammar_template_id {
    GRAMMAR_NONE              = 0,
    GRAMMAR_NOUN_ONLY         = 1u << 0,
    GRAMMAR_VERB_ONLY         = 1u << 1,
    GRAMMAR_AGENT_ACTION      = 1u << 2,   /* "dog run" */
    GRAMMAR_MODIFIER_NOUN     = 1u << 3,   /* "big dog" */
    GRAMMAR_ACTION_PATIENT    = 1u << 4,   /* "eat food" */
    GRAMMAR_POSSESSOR_OWNED   = 1u << 5,   /* "mama hat" */
    GRAMMAR_SVO               = 1u << 6,   /* "dog eat food" */
    GRAMMAR_SVO_TELEGRAPHIC   = 1u << 7,   /* "grass green" — no copula */
    GRAMMAR_FULL              = 0xFFFFFFFFu
} grammar_template_id_t;

/**
 * @brief Per-stage developmental constraints.
 *
 * Fields:
 *   stage_idx          — index of this stage (0..N).
 *   max_visible_vocab  — lexicon mask cap: only the first N tokens (by
 *                        insertion order, which approximates frequency
 *                        at the early curriculum) are active for
 *                        production. SIZE_MAX = unlimited.
 *   min_produce_words  — production count must be >= this; cascade
 *                        returns to settle phase if not met. 0 disables.
 *   max_produce_words  — production count must be <= this; cascade
 *                        truncates extra words. UINT16_MAX = unlimited.
 *   allowed_grammar_mask — bitmask of grammar_template_id_t values that
 *                        the produced sequence is allowed to match.
 *                        GRAMMAR_FULL disables the check.
 */
typedef struct stage_constraints {
    uint32_t stage_idx;
    size_t   max_visible_vocab;
    uint16_t min_produce_words;
    uint16_t max_produce_words;
    uint32_t allowed_grammar_mask;
} stage_constraints_t;

/**
 * @brief Look up the constraints row for a given stage index.
 *
 * Out-of-range indices clamp to the highest defined stage (which is the
 * "unlimited" row at the top of the table). The returned pointer points
 * into static storage and is valid for the lifetime of the process —
 * callers may NOT free it or write through it.
 *
 * @param stage_idx Stage index requested.
 * @return Non-NULL pointer to the constraints row.
 */
const stage_constraints_t* stage_table_get(uint32_t stage_idx);

/**
 * @brief Return a "no constraints" row.
 *
 * Used by call sites where the brain's stage hasn't been set or is
 * otherwise unknown. Returned row has:
 *   max_visible_vocab    = SIZE_MAX
 *   min_produce_words    = 0
 *   max_produce_words    = UINT16_MAX
 *   allowed_grammar_mask = GRAMMAR_FULL
 *
 * The returned pointer points into static storage.
 */
const stage_constraints_t* stage_table_default(void);

/**
 * @brief Highest defined stage index (0..N inclusive).
 *
 * The table defines explicit rows up to and including this index. Any
 * larger index passed to stage_table_get() clamps to this row.
 */
uint32_t stage_table_max_stage(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_STAGE_TABLE_H */
