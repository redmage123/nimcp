/**
 * @file nimcp_language_acc_bridge.c
 * @brief Anterior cingulate cortex (conflict / error monitoring) <-> language.
 *
 * WHAT: ACC conflict + error readings, combined with the language system's
 *       own semantic conflict (surprise between consecutive discourse turns),
 *       raise the produce confidence floor.
 * WHY:  ACC conflict signals raise the response threshold (Botvinick 2001;
 *       Cavanagh & Frank 2014: ACC->STN "hold your horses") — under conflict
 *       the speaker hesitates instead of asserting.
 * HOW:  brain->cingulate: cingulate_get_stats (avg conflict / error) and
 *       cingulate_get_control_level. conflict = 0.5*acc + 0.5*language
 *       surprise; floor_adjust = clamp(0.12*(conflict - 0.3), -0.04, 0.10).
 *
 * Rewritten 2026-11-24 (dead-code wiring): the 2026-10-21 version returned
 * constants and duplicated grounded_language_integrate_acc.
 */
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/regions/cingulate/nimcp_cingulate_adapter.h"
#include "language/nimcp_grounded_language_internal.h"
#include "language/bridges/nimcp_language_cognitive_wired.h"
#include "utils/memory/nimcp_memory.h"

#include <math.h>

typedef struct { int unused; } lang_acc_state_t;

static void* acc_create(struct brain_struct* brain) {
    if (!brain || !brain->cingulate) { return NULL; }
    return nimcp_calloc(1, sizeof(lang_acc_state_t));
}

static void acc_destroy(void* st) { if (st) { nimcp_free(st); } }

static int acc_update(void* st, struct brain_struct* brain,
                      const lang_cog_input_t* in, lang_cog_signal_t* out) {
    lang_cog_signal_neutral(out);
    if (!st || !brain || !brain->cingulate || !in) { return -1; }

    cingulate_stats_t s;
    memset(&s, 0, sizeof(s));
    if (!cingulate_get_stats(brain->cingulate, &s)) { return -1; }

    float acc_conflict = lang_cog_clampf(lang_cog_finite_or(s.avg_conflict_level, 0.0f), 0.0f, 1.0f);
    float acc_error    = lang_cog_clampf(lang_cog_finite_or(s.avg_error_magnitude, 0.0f), 0.0f, 1.0f);
    float control      = lang_cog_clampf(lang_cog_finite_or(
                             cingulate_get_control_level(brain->cingulate), 0.0f), 0.0f, 1.0f);

    float conflict = 0.5f * fmaxf(acc_conflict, acc_error) + 0.5f * in->semantic_surprise;
    out->floor_adjust = lang_cog_clampf(0.12f * (conflict - 0.3f), -0.04f, 0.10f);
    out->primary   = conflict;
    out->secondary = control;
    out->valid     = true;
    return 0;
}

const lang_cog_bridge_vt_t lang_acc_bridge_vt = {
    .name = "acc", .gl_bit = GL_BRIDGE_ACC,
    .create = acc_create, .destroy = acc_destroy, .update = acc_update,
    .recall = NULL, .save = NULL, .load = NULL,
};
