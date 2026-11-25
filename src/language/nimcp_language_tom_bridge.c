/**
 * @file nimcp_language_tom_bridge.c
 * @brief Theory of Mind (perspective taking) <-> language.
 *
 * WHAT: Good self/other perspective separation makes the speaker willing to
 *       address a listener; poor separation makes it more conservative.
 * WHY:  Audience design depends on modelling the listener's knowledge
 *       (Clark 1996; Frith & Frith 2006).
 * HOW:  brain->theory_of_mind: tom_get_perspective_score p in [0,1].
 *       floor_adjust = -0.05*(2p - 1).
 *
 * Rewritten 2026-11-24 (dead-code wiring): the 2026-10-21 version returned
 * constants and duplicated grounded_language_integrate_tom.
 */
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/nimcp_theory_of_mind.h"
#include "language/nimcp_grounded_language_internal.h"
#include "language/bridges/nimcp_language_cognitive_wired.h"
#include "utils/memory/nimcp_memory.h"

typedef struct { int unused; } lang_tom_state_t;

static void* tom_bridge_create(struct brain_struct* brain) {
    if (!brain || !brain->theory_of_mind) { return NULL; }
    return nimcp_calloc(1, sizeof(lang_tom_state_t));
}

static void tom_bridge_destroy(void* st) { if (st) { nimcp_free(st); } }

static int tom_bridge_update(void* st, struct brain_struct* brain,
                             const lang_cog_input_t* in, lang_cog_signal_t* out) {
    lang_cog_signal_neutral(out);
    if (!st || !brain || !brain->theory_of_mind || !in) { return -1; }
    float p = lang_cog_clampf(lang_cog_finite_or(
                  tom_get_perspective_score(brain->theory_of_mind), 0.5f), 0.0f, 1.0f);
    out->floor_adjust = -0.05f * (2.0f * p - 1.0f);
    out->primary = p;
    out->valid   = true;
    return 0;
}

const lang_cog_bridge_vt_t lang_tom_bridge_vt = {
    .name = "tom", .gl_bit = GL_BRIDGE_TOM,
    .create = tom_bridge_create, .destroy = tom_bridge_destroy, .update = tom_bridge_update,
    .recall = NULL, .save = NULL, .load = NULL,
};
