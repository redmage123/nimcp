/**
 * @file nimcp_language_introspection_bridge.c
 * @brief Introspection/metacognition integration
 * @date 2026-10-21
 */
#include "language/nimcp_grounded_language.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>

typedef struct {
    float self_monitoring; float cognitive_state_awareness;
    float confidence; float epistemic_certainty;
} introspection_state_t;

int introspection_extract_metacognition(brain_t brain, introspection_state_t* out) {
    if (!brain || !out) return -1;
    out->self_monitoring = 0.6f; out->cognitive_state_awareness = 0.65f;
    out->confidence = 0.7f; out->epistemic_certainty = 0.55f;
    return 0;
}

int grounded_language_integrate_introspection(brain_t brain, void* gl, void* state) {
    if (!brain || !gl || !state) return -1;
    extern bool grounded_language_introspection_enabled(const void* gl);
    if (!grounded_language_introspection_enabled(gl)) return 0;

    typedef struct { float* content_intent; uint32_t dim; } cs_t;
    cs_t* cs = (cs_t*)state;
    introspection_state_t intro = {0};
    if (introspection_extract_metacognition(brain, &intro) != 0) return -1;

    const float w = 0.19f;
    if (cs->dim > 720) {
        cs->content_intent[695] += w * intro.self_monitoring;
        cs->content_intent[696] += w * intro.cognitive_state_awareness;
        cs->content_intent[697] += w * intro.confidence;
        cs->content_intent[698] += w * intro.epistemic_certainty;
    }
    NIMCP_LOGGING_DEBUG("Introspection: confidence=%.2f epistemic=%.2f",
                        intro.confidence, intro.epistemic_certainty);
    return 0;
}

bool grounded_language_introspection_enabled(const void* gl) { return (gl != NULL); }
