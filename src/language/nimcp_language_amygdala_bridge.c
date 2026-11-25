/**
 * @file nimcp_language_amygdala_bridge.c
 * @brief Amygdala (fear / anxiety salience) <-> language.
 *
 * WHAT: Amygdala fear amplifies the salience of what is about to be said
 *       (content_intent gain); fear/anxiety arousal raises the produce floor.
 * WHY:  Emotional arousal boosts the salience/encoding of emotional content
 *       (Phelps 2004) while anxiety inhibits verbal fluency and assertion.
 * HOW:  brain->amygdala: amygdala_get_fear_level / amygdala_get_anxiety_level
 *       (both [0,1]). floor_adjust = 0.10*max(fear, anxiety);
 *       intent_gain = 1 + 0.10*fear.
 *
 * Rewritten 2026-11-24 (dead-code wiring): the 2026-10-21 version returned
 * constants and duplicated grounded_language_integrate_amygdala.
 */
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/subcortical/nimcp_amygdala.h"
#include "language/nimcp_grounded_language_internal.h"
#include "language/bridges/nimcp_language_cognitive_wired.h"
#include "utils/memory/nimcp_memory.h"

#include <math.h>

typedef struct { int unused; } lang_amyg_state_t;

static void* amyg_create(struct brain_struct* brain) {
    if (!brain || !brain->amygdala) { return NULL; }
    return nimcp_calloc(1, sizeof(lang_amyg_state_t));
}

static void amyg_destroy(void* st) { if (st) { nimcp_free(st); } }

static int amyg_update(void* st, struct brain_struct* brain,
                       const lang_cog_input_t* in, lang_cog_signal_t* out) {
    lang_cog_signal_neutral(out);
    if (!st || !brain || !brain->amygdala || !in) { return -1; }
    const amygdala_t* a = (const amygdala_t*)brain->amygdala;

    float fear    = lang_cog_clampf(lang_cog_finite_or(amygdala_get_fear_level(a), 0.0f), 0.0f, 1.0f);
    float anxiety = lang_cog_clampf(lang_cog_finite_or(amygdala_get_anxiety_level(a), 0.0f), 0.0f, 1.0f);

    out->floor_adjust = 0.10f * fmaxf(fear, anxiety);
    out->intent_gain  = 1.0f + 0.10f * fear;
    out->primary   = fear;
    out->secondary = anxiety;
    out->valid     = true;
    return 0;
}

const lang_cog_bridge_vt_t lang_amygdala_bridge_vt = {
    .name = "amygdala", .gl_bit = GL_BRIDGE_AMYGDALA,
    .create = amyg_create, .destroy = amyg_destroy, .update = amyg_update,
    .recall = NULL, .save = NULL, .load = NULL,
};
