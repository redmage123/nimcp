/**
 * @file nimcp_language_mirror_bridge.c
 * @brief Mirror-neuron social salience <-> language.
 *
 * WHAT: Social salience from the mirror-neuron system (an observed agent is
 *       acting / attending) raises the drive to communicate.
 * WHY:  Mirror / STS circuits link observed action to communicative intent;
 *       speech is a social act whose initiation depends on a partner
 *       (Rizzolatti & Arbib 1998).
 * HOW:  brain->mirror_neurons: mirror_neurons_get_social_salience s in [0,1].
 *       intent_gain = 1 + 0.10*s; floor_adjust = -0.05*s.
 *
 * Rewritten 2026-11-24 (dead-code wiring): the 2026-10-21 version returned
 * constants and duplicated grounded_language_integrate_mirror.
 */
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/nimcp_mirror_neurons.h"
#include "language/nimcp_grounded_language_internal.h"
#include "language/bridges/nimcp_language_cognitive_wired.h"
#include "utils/memory/nimcp_memory.h"

typedef struct { int unused; } lang_mirror_state_t;

static void* mirror_create(struct brain_struct* brain) {
    if (!brain || !brain->mirror_neurons) { return NULL; }
    return nimcp_calloc(1, sizeof(lang_mirror_state_t));
}

static void mirror_destroy(void* st) { if (st) { nimcp_free(st); } }

static int mirror_update(void* st, struct brain_struct* brain,
                         const lang_cog_input_t* in, lang_cog_signal_t* out) {
    lang_cog_signal_neutral(out);
    if (!st || !brain || !brain->mirror_neurons || !in) { return -1; }
    float s = lang_cog_clampf(lang_cog_finite_or(
                  mirror_neurons_get_social_salience(brain->mirror_neurons), 0.0f), 0.0f, 1.0f);
    out->intent_gain  = 1.0f + 0.10f * s;
    out->floor_adjust = -0.05f * s;
    out->primary = s;
    out->valid   = true;
    return 0;
}

const lang_cog_bridge_vt_t lang_mirror_bridge_vt = {
    .name = "mirror", .gl_bit = GL_BRIDGE_MIRROR,
    .create = mirror_create, .destroy = mirror_destroy, .update = mirror_update,
    .recall = NULL, .save = NULL, .load = NULL,
};
