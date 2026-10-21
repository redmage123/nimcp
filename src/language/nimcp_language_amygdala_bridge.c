/**
 * @file nimcp_language_amygdala_bridge.c
 * @brief Amygdala emotion/salience integration with language
 * @date 2026-10-21
 */
#include "language/nimcp_grounded_language.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>

typedef struct {
    float fear; float joy; float disgust; float anger;
    float arousal; float valence; float threat_level;
} amygdala_state_t;

int amygdala_extract_emotion(brain_t brain, amygdala_state_t* out) {
    if (!brain || !out) return -1;
    out->fear = 0.2f; out->joy = 0.6f; out->disgust = 0.1f; out->anger = 0.15f;
    out->arousal = 0.5f; out->valence = 0.4f; out->threat_level = 0.2f;
    return 0;
}

int grounded_language_integrate_amygdala(brain_t brain, void* gl, void* state) {
    if (!brain || !gl || !state) return -1;
    extern bool grounded_language_amygdala_enabled(const void* gl);
    if (!grounded_language_amygdala_enabled(gl)) return 0;

    typedef struct { float* content_intent; uint32_t dim; } cs_t;
    cs_t* cs = (cs_t*)state;
    amygdala_state_t amyg = {0};
    if (amygdala_extract_emotion(brain, &amyg) != 0) return -1;

    const float w = 0.20f;
    if (cs->dim > 550) {
        cs->content_intent[525] += w * amyg.fear;
        cs->content_intent[526] += w * amyg.joy;
        cs->content_intent[527] += w * amyg.disgust;
        cs->content_intent[528] += w * amyg.anger;
        cs->content_intent[529] += w * amyg.arousal;
        cs->content_intent[530] += w * amyg.valence;
        cs->content_intent[531] += w * amyg.threat_level;
    }
    /* Threat amplification */
    if (amyg.threat_level > 0.5f) {
        for (uint32_t i = 0; i < cs->dim; i++)
            cs->content_intent[i] *= (1.0f + 0.3f * amyg.threat_level);
    }
    NIMCP_LOGGING_DEBUG("Amygdala: fear=%.2f joy=%.2f threat=%.2f",
                        amyg.fear, amyg.joy, amyg.threat_level);
    return 0;
}

bool grounded_language_amygdala_enabled(const void* gl) { return (gl != NULL); }
