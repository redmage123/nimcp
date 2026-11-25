/**
 * @file nimcp_language_jepa_bridge.c
 * @brief JEPA latent predictor surprise <-> language.
 *
 * WHAT: When the brain's JEPA predictor is currently doing worse than its
 *       long-run average (rising prediction loss = surprise) the produce
 *       floor rises; when it is predicting well it falls.
 * WHY:  Predictive-processing account of confidence: speakers commit when
 *       their generative model is predicting well (Pickering & Garrod 2013).
 *       The predictor is trained on the brain's feature space, not the
 *       language semantic space, so its predictions cannot be blended into
 *       content_intent (the 2026-10-21 version faked them with momentum +
 *       rand()); its loss trajectory is the honest signal.
 * HOW:  brain->jepa_predictor: jepa_predictor_get_stats avg_loss L.
 *       Fast EMA (0.2) vs slow EMA (0.01); ratio = fast/slow.
 *       floor_adjust = 0.10*clamp(ratio - 1, -0.5, 1). Slow EMA persists.
 */
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "language/nimcp_grounded_language_internal.h"
#include "language/bridges/nimcp_language_cognitive_wired.h"
#include "utils/memory/nimcp_memory.h"

#include <string.h>

typedef struct {
    float fast, slow, n;
} lang_jepa_state_t;

static void* jepa_create(struct brain_struct* brain) {
    if (!brain || !brain->jepa_predictor) { return NULL; }
    return nimcp_calloc(1, sizeof(lang_jepa_state_t));
}

static void jepa_destroy(void* st) { if (st) { nimcp_free(st); } }

static int jepa_update(void* st, struct brain_struct* brain,
                       const lang_cog_input_t* in, lang_cog_signal_t* out) {
    lang_cog_signal_neutral(out);
    lang_jepa_state_t* s = (lang_jepa_state_t*)st;
    if (!s || !brain || !brain->jepa_predictor || !in) { return -1; }
    jepa_predictor_stats_t js;
    memset(&js, 0, sizeof(js));
    if (jepa_predictor_get_stats(brain->jepa_predictor, &js) != 0) { return -1; }
    if (js.updates_applied == 0) { out->valid = true; return 0; }   /* untrained: neutral */
    float L = lang_cog_finite_or(js.avg_loss, -1.0f);
    if (L < 0.0f) { return -1; }
    if (s->n < 1.0f) { s->fast = L; s->slow = L; }
    s->fast = 0.8f * s->fast + 0.2f * L;
    s->slow = 0.99f * s->slow + 0.01f * L;
    s->n += 1.0f;
    float ratio = s->fast / (s->slow + 1e-6f);
    out->floor_adjust = 0.10f * lang_cog_clampf(ratio - 1.0f, -0.5f, 1.0f);
    out->primary   = lang_cog_clampf(ratio, 0.0f, 10.0f);
    out->secondary = L;
    out->valid     = true;
    return 0;
}

static int jepa_save(const void* st, FILE* f) {
    const lang_jepa_state_t* s = (const lang_jepa_state_t*)st;
    float blob[3] = { s->fast, s->slow, s->n };
    return lang_cog_write_floats(f, blob, 3);
}

static int jepa_load(void* st, FILE* f) {
    lang_jepa_state_t* s = (lang_jepa_state_t*)st;
    float blob[3];
    if (lang_cog_read_floats(f, blob, 3) != 0) { return -1; }
    s->fast = blob[0] > 0.0f ? blob[0] : 0.0f;
    s->slow = blob[1] > 0.0f ? blob[1] : 0.0f;
    s->n    = blob[2] > 0.0f ? blob[2] : 0.0f;
    return 0;
}

const lang_cog_bridge_vt_t lang_jepa_bridge_vt = {
    .name = "jepa", .gl_bit = GL_BRIDGE_JEPA,
    .create = jepa_create, .destroy = jepa_destroy, .update = jepa_update,
    .recall = NULL, .save = jepa_save, .load = jepa_load,
};
