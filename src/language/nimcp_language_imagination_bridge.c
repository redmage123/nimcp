/**
 * @file nimcp_language_imagination_bridge.c
 * @brief Imagination engine <-> language (vividness-weighted salience).
 *
 * WHAT: Vivid, coherent mental simulation raises the salience of the
 *       content being verbalised.
 * WHY:  Vivid imagery supports richer, more confident description (mental
 *       simulation grounds language: Barsalou 2008). The imagination ACTIVE
 *       VECTOR is already blended into content_intent by cascade stage
 *       content (Tier-1 Step E), so this bridge only contributes the
 *       scalar quality of the simulation, never the vector (no double count).
 * HOW:  brain->imagination: imagination_get_stats -> avg_vividness v,
 *       avg_coherence c. intent_gain = 1 + 0.10*v*c.
 *
 * Rewritten 2026-11-24 (dead-code wiring): the 2026-10-21 version returned
 * constants and duplicated grounded_language_integrate_imagination.
 */
/* The imagination header's typedefs clash with nimcp_brain_internal.h, so this
 * TU never includes the brain struct: the engine pointer comes from
 * lang_cog_brain_imagination() (defined in the group TU). */
#include "cognitive/imagination/nimcp_imagination_engine.h"
#include "language/bridges/nimcp_language_cognitive_wired.h"
#include "utils/memory/nimcp_memory.h"

#include <string.h>

typedef struct { int unused; } lang_imag_state_t;

static void* imag_create(struct brain_struct* brain) {
    if (!lang_cog_brain_imagination(brain)) { return NULL; }
    return nimcp_calloc(1, sizeof(lang_imag_state_t));
}

static void imag_destroy(void* st) { if (st) { nimcp_free(st); } }

static int imag_update(void* st, struct brain_struct* brain,
                       const lang_cog_input_t* in, lang_cog_signal_t* out) {
    lang_cog_signal_neutral(out);
    const imagination_engine_t* eng =
        (const imagination_engine_t*)lang_cog_brain_imagination(brain);
    if (!st || !eng || !in) { return -1; }
    imagination_stats_t s;
    memset(&s, 0, sizeof(s));
    if (imagination_get_stats(eng, &s) != 0) {
        return -1;
    }
    float v = lang_cog_clampf(lang_cog_finite_or(s.avg_vividness, 0.0f), 0.0f, 1.0f);
    float c = lang_cog_clampf(lang_cog_finite_or(s.avg_coherence, 0.0f), 0.0f, 1.0f);
    out->intent_gain = 1.0f + 0.10f * v * c;
    out->primary   = v;
    out->secondary = c;
    out->valid     = true;
    return 0;
}

const lang_cog_bridge_vt_t lang_imagination_bridge_vt = {
    .name = "imagination", .gl_bit = (1u << 9) /* GL_BRIDGE_IMAGINATION */,
    .create = imag_create, .destroy = imag_destroy, .update = imag_update,
    .recall = NULL, .save = NULL, .load = NULL,
};
