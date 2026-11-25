/**
 * @file nimcp_language_basal_ganglia_bridge.c
 * @brief Basal ganglia (go / no-go, reward prediction error) <-> language.
 *
 * WHAT: A striatal value estimate V of "speaking" learned from the external
 *       reward the caregiver critic assigns to the brain's language output
 *       (brain->last_external_reward). Positive value lowers the produce
 *       floor (direct "go" pathway), negative value raises it ("no-go").
 * WHY:  Dopaminergic RPE (Schultz 1997) trains corticostriatal values; the
 *       direct/indirect balance gates whether a prepared action — here an
 *       utterance — is released (Frank 2005).
 * HOW:  On every NEW reward event (timestamp changed): delta = r - V,
 *       V += 0.1*delta (r clamped [-1,1]). floor_adjust = -0.10*tanh(2V).
 *       V and the event count persist in the group sidecar.
 *       Created only when brain->basal_ganglia exists.
 *
 * Rewritten 2026-11-24 (dead-code wiring): the 2026-10-21 version used
 * rand() action values and duplicated grounded_language_integrate_basal_ganglia.
 */
#include "core/brain/nimcp_brain_internal.h"
#include "language/nimcp_grounded_language_internal.h"
#include "language/bridges/nimcp_language_cognitive_wired.h"
#include "utils/memory/nimcp_memory.h"

#include <math.h>

typedef struct {
    float    value;          /* V: learned value of speaking */
    float    last_rpe;       /* last delta */
    float    n_events;       /* reward events seen (float for blob I/O) */
    uint64_t last_reward_us; /* runtime only */
} lang_bg_state_t;

static void* bg_create(struct brain_struct* brain) {
    if (!brain || !brain->basal_ganglia) { return NULL; }
    return nimcp_calloc(1, sizeof(lang_bg_state_t));
}

static void bg_destroy(void* st) { if (st) { nimcp_free(st); } }

static int bg_update(void* st, struct brain_struct* brain,
                     const lang_cog_input_t* in, lang_cog_signal_t* out) {
    lang_cog_signal_neutral(out);
    lang_bg_state_t* s = (lang_bg_state_t*)st;
    if (!s || !brain || !in) { return -1; }

    if (in->external_reward_us != 0 && in->external_reward_us != s->last_reward_us) {
        s->last_reward_us = in->external_reward_us;
        float r = lang_cog_clampf(lang_cog_finite_or(in->external_reward, 0.0f), -1.0f, 1.0f);
        float delta = r - s->value;
        s->value = lang_cog_clampf(s->value + 0.1f * delta, -1.0f, 1.0f);
        s->last_rpe = lang_cog_clampf(delta, -2.0f, 2.0f);
        s->n_events += 1.0f;
    }
    out->floor_adjust = -0.10f * tanhf(2.0f * s->value);
    out->primary   = s->value;
    out->secondary = s->last_rpe;
    out->valid     = true;
    return 0;
}

static int bg_save(const void* st, FILE* f) {
    const lang_bg_state_t* s = (const lang_bg_state_t*)st;
    float blob[3] = { s->value, s->last_rpe, s->n_events };
    return lang_cog_write_floats(f, blob, 3);
}

static int bg_load(void* st, FILE* f) {
    lang_bg_state_t* s = (lang_bg_state_t*)st;
    float blob[3];
    if (lang_cog_read_floats(f, blob, 3) != 0) { return -1; }
    s->value    = lang_cog_clampf(blob[0], -1.0f, 1.0f);
    s->last_rpe = lang_cog_clampf(blob[1], -2.0f, 2.0f);
    s->n_events = fmaxf(0.0f, blob[2]);
    return 0;
}

const lang_cog_bridge_vt_t lang_basal_ganglia_bridge_vt = {
    .name = "basal_ganglia", .gl_bit = GL_BRIDGE_BASAL_GANGLIA,
    .create = bg_create, .destroy = bg_destroy, .update = bg_update,
    .recall = NULL, .save = bg_save, .load = bg_load,
};
