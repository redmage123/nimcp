/**
 * @file nimcp_stage_table.c
 * @brief Slice E (Option-1 architectural rebuild) — static developmental
 *        stage constraints table.
 *
 * @date 2026-05-19
 *
 * See include/cognitive/grounded_language/nimcp_stage_table.h for the
 * full design notes. This .c only owns the static table data + the
 * three accessors.
 */

#include "cognitive/grounded_language/nimcp_stage_table.h"

#include <stddef.h>
#include <stdint.h>

/* Stage 0 — single content word ("dog", "ball"). Pure noun production. */
#define STAGE0_GRAMMAR  (GRAMMAR_NOUN_ONLY)

/* Stage 1 — two-word telegraphic productions. The four canonical Brown-
 * stage-I templates (agent-action, modifier-noun, action-patient,
 * possessor-owned) plus single-noun fallback. */
#define STAGE1_GRAMMAR  (GRAMMAR_NOUN_ONLY |    \
                          GRAMMAR_AGENT_ACTION |  \
                          GRAMMAR_MODIFIER_NOUN | \
                          GRAMMAR_ACTION_PATIENT |\
                          GRAMMAR_POSSESSOR_OWNED)

/* Stage 2 — telegraphic SVO + early multi-word combinations. Adds the
 * full SVO and the copula-less SVO ("grass green"). */
#define STAGE2_GRAMMAR  (STAGE1_GRAMMAR | GRAMMAR_SVO | GRAMMAR_SVO_TELEGRAPHIC)

/* Stage 3+ — adult grammar; the grammar check is effectively off. */
#define STAGE3_GRAMMAR  GRAMMAR_FULL

/* The constraints table itself. ROW INDEX == stage_idx (no gaps). */
static const stage_constraints_t kStageTable[] = {
    /* stage 0: single-word noun production */
    {
        .stage_idx            = 0u,
        .max_visible_vocab    = 50u,
        .min_produce_words    = 1u,
        .max_produce_words    = 1u,
        .allowed_grammar_mask = STAGE0_GRAMMAR,
    },
    /* stage 1: two-word telegraphic */
    {
        .stage_idx            = 1u,
        .max_visible_vocab    = 200u,
        .min_produce_words    = 2u,
        .max_produce_words    = 2u,
        .allowed_grammar_mask = STAGE1_GRAMMAR,
    },
    /* stage 2: 3-4 word combos with early SVO */
    {
        .stage_idx            = 2u,
        .max_visible_vocab    = 800u,
        .min_produce_words    = 3u,
        .max_produce_words    = 4u,
        .allowed_grammar_mask = STAGE2_GRAMMAR,
    },
    /* stage 3: full grammar, 4-8 words */
    {
        .stage_idx            = 3u,
        .max_visible_vocab    = 3000u,
        .min_produce_words    = 4u,
        .max_produce_words    = 8u,
        .allowed_grammar_mask = STAGE3_GRAMMAR,
    },
    /* stage 4+: unlimited vocab, 5-20 words */
    {
        .stage_idx            = 4u,
        .max_visible_vocab    = SIZE_MAX,
        .min_produce_words    = 5u,
        .max_produce_words    = 20u,
        .allowed_grammar_mask = GRAMMAR_FULL,
    },
};

#define NIMCP_STAGE_TABLE_NUM_ROWS \
    ((uint32_t)(sizeof(kStageTable) / sizeof(kStageTable[0])))

/* "No constraints" fallback — used by stage_table_default() and by
 * callers that pass a stage > stage_table_max_stage() AND want to
 * disambiguate from "table clamp" semantics. Note this is NOT the same
 * as the stage-4+ row: that row has min_produce_words=5 which is a real
 * developmental constraint. */
static const stage_constraints_t kStageTableDefault = {
    .stage_idx            = 0xFFFFFFFFu,
    .max_visible_vocab    = SIZE_MAX,
    .min_produce_words    = 0u,
    .max_produce_words    = UINT16_MAX,
    .allowed_grammar_mask = GRAMMAR_FULL,
};

const stage_constraints_t* stage_table_get(uint32_t stage_idx)
{
    if (stage_idx >= NIMCP_STAGE_TABLE_NUM_ROWS) {
        /* Clamp to the highest defined row (stage 4+). */
        return &kStageTable[NIMCP_STAGE_TABLE_NUM_ROWS - 1u];
    }
    return &kStageTable[stage_idx];
}

const stage_constraints_t* stage_table_default(void)
{
    return &kStageTableDefault;
}

uint32_t stage_table_max_stage(void)
{
    return NIMCP_STAGE_TABLE_NUM_ROWS - 1u;
}
