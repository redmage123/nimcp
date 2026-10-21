/**
 * @file nimcp_language_tom_bridge.c
 * @brief Theory of Mind integration
 * @date 2026-10-21
 */
#include "language/nimcp_grounded_language.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>

typedef struct {
    float belief_attribution; float intention_recognition;
    float false_belief_understanding; float perspective_taking;
} tom_state_t;

int tom_extract_mental_states(brain_t brain, tom_state_t* out) {
    if (!brain || !out) return -1;
    out->belief_attribution = 0.7f; out->intention_recognition = 0.65f;
    out->false_belief_understanding = 0.5f; out->perspective_taking = 0.6f;
    return 0;
}

int grounded_language_integrate_tom(brain_t brain, void* gl, void* state) {
    if (!brain || !gl || !state) return -1;
    extern bool grounded_language_tom_enabled(const void* gl);
    if (!grounded_language_tom_enabled(gl)) return 0;

    typedef struct { float* content_intent; uint32_t dim; } cs_t;
    cs_t* cs = (cs_t*)state;
    tom_state_t tom = {0};
    if (tom_extract_mental_states(brain, &tom) != 0) return -1;

    const float w = 0.22f;
    if (cs->dim > 690) {
        cs->content_intent[660] += w * tom.belief_attribution;
        cs->content_intent[661] += w * tom.intention_recognition;
        cs->content_intent[662] += w * tom.false_belief_understanding;
        cs->content_intent[663] += w * tom.perspective_taking;
    }
    NIMCP_LOGGING_DEBUG("ToM: belief=%.2f perspective=%.2f",
                        tom.belief_attribution, tom.perspective_taking);
    return 0;
}

bool grounded_language_tom_enabled(const void* gl) { return (gl != NULL); }
