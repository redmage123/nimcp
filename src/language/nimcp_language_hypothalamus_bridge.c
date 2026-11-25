/**
 * @file nimcp_language_hypothalamus_bridge.c
 * @brief Hypothalamus (HPA stress / autonomic state) <-> language.
 *
 * WHAT: Stress (cortisol) lowers the drive behind an utterance and raises
 *       the produce floor; sympathetic arousal is reported alongside.
 * WHY:  Acute stress / HPA activation impairs verbal fluency and retrieval
 *       (Kirschbaum 1996; de Quervain 2000).
 * HOW:  brain->hypothalamus: hypothalamus_get_cortisol, _get_autonomic_balance
 *       ([0,1]). floor_adjust = 0.10*clamp((c-0.4)/0.6, 0, 1);
 *       intent_gain = 1 - 0.30*max(0, c-0.5)  (>= 0.85).
 *
 * Rewritten 2026-11-24 (dead-code wiring): the 2026-10-21 version used rand()
 * hunger/thirst and duplicated grounded_language_integrate_hypothalamus.
 */
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_adapter.h"
#include "language/nimcp_grounded_language_internal.h"
#include "language/bridges/nimcp_language_cognitive_wired.h"
#include "utils/memory/nimcp_memory.h"

#include <math.h>

typedef struct { int unused; } lang_hypo_state_t;

static void* hypo_create(struct brain_struct* brain) {
    if (!brain || !brain->hypothalamus) { return NULL; }
    return nimcp_calloc(1, sizeof(lang_hypo_state_t));
}

static void hypo_destroy(void* st) { if (st) { nimcp_free(st); } }

static int hypo_update(void* st, struct brain_struct* brain,
                       const lang_cog_input_t* in, lang_cog_signal_t* out) {
    lang_cog_signal_neutral(out);
    if (!st || !brain || !brain->hypothalamus || !in) { return -1; }
    float c = lang_cog_clampf(lang_cog_finite_or(
                  hypothalamus_get_cortisol(brain->hypothalamus), 0.0f), 0.0f, 1.0f);
    float b = lang_cog_clampf(lang_cog_finite_or(
                  hypothalamus_get_autonomic_balance(brain->hypothalamus), 0.5f), 0.0f, 1.0f);
    out->floor_adjust = 0.10f * lang_cog_clampf((c - 0.4f) / 0.6f, 0.0f, 1.0f);
    out->intent_gain  = 1.0f - 0.30f * fmaxf(0.0f, c - 0.5f);
    out->primary   = c;
    out->secondary = b;
    out->valid     = true;
    return 0;
}

const lang_cog_bridge_vt_t lang_hypothalamus_bridge_vt = {
    .name = "hypothalamus", .gl_bit = GL_BRIDGE_HYPOTHALAMUS,
    .create = hypo_create, .destroy = hypo_destroy, .update = hypo_update,
    .recall = NULL, .save = NULL, .load = NULL,
};
