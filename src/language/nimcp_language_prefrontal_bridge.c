/**
 * @file nimcp_language_prefrontal_bridge.c
 * @brief Prefrontal cortex executive function integration
 * @date 2026-10-21
 */
#include "language/nimcp_grounded_language.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>

typedef struct {
    float goal_strength; float plan_depth; float inhibition;
    float working_memory_load; float executive_control;
} prefrontal_state_t;

int prefrontal_extract_executive(brain_t brain, prefrontal_state_t* out) {
    if (!brain || !out) return -1;
    out->goal_strength = 0.7f; out->plan_depth = 0.6f; out->inhibition = 0.5f;
    out->working_memory_load = 0.4f; out->executive_control = 0.65f;
    return 0;
}

int grounded_language_integrate_prefrontal(brain_t brain, void* gl, void* state) {
    if (!brain || !gl || !state) return -1;
    extern bool grounded_language_prefrontal_enabled(const void* gl);
    if (!grounded_language_prefrontal_enabled(gl)) return 0;

    typedef struct { float* content_intent; uint32_t dim; } cs_t;
    cs_t* cs = (cs_t*)state;
    prefrontal_state_t pfc = {0};
    if (prefrontal_extract_executive(brain, &pfc) != 0) return -1;

    const float w = 0.25f;
    if (cs->dim > 600) {
        cs->content_intent[555] += w * pfc.goal_strength;
        cs->content_intent[556] += w * pfc.plan_depth;
        cs->content_intent[557] += w * pfc.inhibition;
        cs->content_intent[558] += w * pfc.working_memory_load;
        cs->content_intent[559] += w * pfc.executive_control;
    }
    /* Goal-directed amplification */
    if (pfc.goal_strength > 0.6f) {
        for (uint32_t i = 0; i < cs->dim; i++)
            cs->content_intent[i] *= (1.0f + 0.2f * pfc.goal_strength);
    }
    NIMCP_LOGGING_DEBUG("Prefrontal: goal=%.2f plan=%.2f control=%.2f",
                        pfc.goal_strength, pfc.plan_depth, pfc.executive_control);
    return 0;
}

bool grounded_language_prefrontal_enabled(const void* gl) { return (gl != NULL); }
