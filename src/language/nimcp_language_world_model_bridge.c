/**
 * @file nimcp_language_world_model_bridge.c
 * @brief World-model prediction error <-> language.
 *
 * WHAT: When the brain's world-model trainer is currently mispredicting more
 *       than usual, the produce floor rises (speak less confidently about a
 *       world you do not currently model well); when it predicts well it
 *       falls.
 * WHY:  Original intent (2026-10-21): ground language in world-model state
 *       for counterfactual reasoning. That version used last_decision output
 *       as a fake "latent state" and simulated transitions with rand();
 *       nimcp_wmt_predict runs full brain inference (too heavy for a tick).
 *       The trainer's EMA prediction error is the honest, cheap signal.
 * HOW:  brain->world_model_trainer: nimcp_wmt_get_prediction_error e.
 *       Fast EMA (0.2) vs slow EMA (0.01); ratio = fast/slow;
 *       floor_adjust = 0.10*clamp(ratio - 1, -0.5, 1). Slow EMA persists.
 */
#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/nimcp_world_model_trainer.h"
#include "language/nimcp_grounded_language_internal.h"
#include "language/bridges/nimcp_language_cognitive_wired.h"
#include "utils/memory/nimcp_memory.h"

typedef struct {
    float fast, slow, n;
} lang_wm_state_t;

static void* wm_create(struct brain_struct* brain) {
    if (!brain || !brain->world_model_trainer) { return NULL; }
    return nimcp_calloc(1, sizeof(lang_wm_state_t));
}

static void wm_destroy(void* st) { if (st) { nimcp_free(st); } }

static int wm_update(void* st, struct brain_struct* brain,
                     const lang_cog_input_t* in, lang_cog_signal_t* out) {
    lang_cog_signal_neutral(out);
    lang_wm_state_t* s = (lang_wm_state_t*)st;
    if (!s || !brain || !brain->world_model_trainer || !in) { return -1; }
    float e = lang_cog_finite_or(nimcp_wmt_get_prediction_error(
                  (const nimcp_world_model_trainer_t*)brain->world_model_trainer), -1.0f);
    if (e <= 0.0f) { out->valid = true; return 0; }    /* untrained: neutral */
    if (s->n < 1.0f) { s->fast = e; s->slow = e; }
    s->fast = 0.8f * s->fast + 0.2f * e;
    s->slow = 0.99f * s->slow + 0.01f * e;
    s->n += 1.0f;
    float ratio = s->fast / (s->slow + 1e-6f);
    out->floor_adjust = 0.10f * lang_cog_clampf(ratio - 1.0f, -0.5f, 1.0f);
    out->primary   = lang_cog_clampf(ratio, 0.0f, 10.0f);
    out->secondary = e;
    out->valid     = true;
    return 0;
}

static int wm_save(const void* st, FILE* f) {
    const lang_wm_state_t* s = (const lang_wm_state_t*)st;
    float blob[3] = { s->fast, s->slow, s->n };
    return lang_cog_write_floats(f, blob, 3);
}

static int wm_load(void* st, FILE* f) {
    lang_wm_state_t* s = (lang_wm_state_t*)st;
    float blob[3];
    if (lang_cog_read_floats(f, blob, 3) != 0) { return -1; }
    s->fast = blob[0] > 0.0f ? blob[0] : 0.0f;
    s->slow = blob[1] > 0.0f ? blob[1] : 0.0f;
    s->n    = blob[2] > 0.0f ? blob[2] : 0.0f;
    return 0;
}

const lang_cog_bridge_vt_t lang_world_model_bridge_vt = {
    .name = "world_model", .gl_bit = GL_BRIDGE_WORLD_MODEL,
    .create = wm_create, .destroy = wm_destroy, .update = wm_update,
    .recall = NULL, .save = wm_save, .load = wm_load,
};
