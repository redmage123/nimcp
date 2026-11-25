/**
 * @file nimcp_language_introspection_bridge.c
 * @brief Introspection (metacognitive monitoring of language competence).
 *
 * WHAT: The brain's knowledge of how well it currently understands language
 *       (grounded-language running comprehension confidence), weighted by
 *       how actively the introspection module is self-monitoring, shifts the
 *       produce floor: knowing that it understands -> willing to answer;
 *       knowing that it does not -> "I don't know".
 * WHY:  Metacognitive feeling-of-knowing gates report (Koriat 1993; Fleming
 *       & Dolan 2012).
 * HOW:  brain->introspection: introspection_get_history_stats utilization u
 *       (self-monitoring activity). monitor = 0.5 + 0.5*u.
 *       floor_adjust = -0.10*(conf - 0.5)*monitor.
 *
 * Rewritten 2026-11-24 (dead-code wiring): the 2026-10-21 version returned
 * constants and duplicated grounded_language_integrate_introspection.
 */
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "language/nimcp_grounded_language_internal.h"
#include "language/bridges/nimcp_language_cognitive_wired.h"
#include "utils/memory/nimcp_memory.h"

typedef struct { int unused; } lang_intro_state_t;

static void* intro_create(struct brain_struct* brain) {
    if (!brain || !brain->introspection || !brain->grounded_lang) { return NULL; }
    return nimcp_calloc(1, sizeof(lang_intro_state_t));
}

static void intro_destroy(void* st) { if (st) { nimcp_free(st); } }

static int intro_update(void* st, struct brain_struct* brain,
                        const lang_cog_input_t* in, lang_cog_signal_t* out) {
    lang_cog_signal_neutral(out);
    if (!st || !brain || !brain->introspection || !in) { return -1; }
    uint32_t cur = 0, cap = 0;
    float util = 0.0f;
    if (!introspection_get_history_stats(brain->introspection, &cur, &cap, &util)) {
        util = 0.0f;
    }
    util = lang_cog_clampf(lang_cog_finite_or(util, 0.0f), 0.0f, 1.0f);
    float conf = lang_cog_clampf(in->comprehension_conf, 0.0f, 1.0f);
    float monitor = 0.5f + 0.5f * util;
    out->floor_adjust = lang_cog_clampf(-0.10f * (conf - 0.5f) * monitor, -0.05f, 0.05f);
    out->primary   = conf;
    out->secondary = util;
    out->valid     = true;
    return 0;
}

const lang_cog_bridge_vt_t lang_introspection_bridge_vt = {
    .name = "introspection", .gl_bit = GL_BRIDGE_INTROSPECTION,
    .create = intro_create, .destroy = intro_destroy, .update = intro_update,
    .recall = NULL, .save = NULL, .load = NULL,
};
