/**
 * @file nimcp_language_consciousness_bridge.c
 * @brief Consciousness/global workspace integration
 * @date 2026-10-21
 */
#include "language/nimcp_grounded_language.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>

typedef struct {
    float broadcast_strength; float attentional_focus;
    float integration_level; float phenomenal_experience;
} consciousness_state_t;

int consciousness_extract_global_workspace(brain_t brain, consciousness_state_t* out) {
    if (!brain || !out) return -1;
    out->broadcast_strength = 0.7f; out->attentional_focus = 0.75f;
    out->integration_level = 0.8f; out->phenomenal_experience = 0.65f;
    return 0;
}

int grounded_language_integrate_consciousness(brain_t brain, void* gl, void* state) {
    if (!brain || !gl || !state) return -1;
    extern bool grounded_language_consciousness_enabled(const void* gl);
    if (!grounded_language_consciousness_enabled(gl)) return 0;

    typedef struct { float* content_intent; uint32_t dim; } cs_t;
    cs_t* cs = (cs_t*)state;
    consciousness_state_t cons = {0};
    if (consciousness_extract_global_workspace(brain, &cons) != 0) return -1;

    const float w = 0.23f;
    if (cs->dim > 820) {
        cs->content_intent[790] += w * cons.broadcast_strength;
        cs->content_intent[791] += w * cons.attentional_focus;
        cs->content_intent[792] += w * cons.integration_level;
        cs->content_intent[793] += w * cons.phenomenal_experience;
    }
    /* Global integration amplification */
    if (cons.integration_level > 0.7f) {
        for (uint32_t i = 0; i < cs->dim; i++)
            cs->content_intent[i] *= (1.0f + 0.15f * cons.integration_level);
    }
    NIMCP_LOGGING_DEBUG("Consciousness: broadcast=%.2f integration=%.2f",
                        cons.broadcast_strength, cons.integration_level);
    return 0;
}

bool grounded_language_consciousness_enabled(const void* gl) { return (gl != NULL); }
