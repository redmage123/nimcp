/**
 * @file nimcp_language_insula_bridge.c
 * @brief Insula interoception/empathy integration
 * @date 2026-10-21
 */
#include "language/nimcp_grounded_language.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>

typedef struct {
    float heartrate; float breathing; float temperature; float pain;
    float disgust; float empathy; float self_awareness;
} insula_state_t;

int insula_extract_interoception(brain_t brain, insula_state_t* out) {
    if (!brain || !out) return -1;
    out->heartrate = 0.5f; out->breathing = 0.5f; out->temperature = 0.5f;
    out->pain = 0.1f; out->disgust = 0.2f; out->empathy = 0.6f;
    out->self_awareness = 0.7f;
    return 0;
}

int grounded_language_integrate_insula(brain_t brain, void* gl, void* state) {
    if (!brain || !gl || !state) return -1;
    extern bool grounded_language_insula_enabled(const void* gl);
    if (!grounded_language_insula_enabled(gl)) return 0;

    typedef struct { float* content_intent; uint32_t dim; } cs_t;
    cs_t* cs = (cs_t*)state;
    insula_state_t ins = {0};
    if (insula_extract_interoception(brain, &ins) != 0) return -1;

    const float w = 0.18f;
    if (cs->dim > 630) {
        cs->content_intent[605] += w * ins.heartrate;
        cs->content_intent[606] += w * ins.breathing;
        cs->content_intent[607] += w * ins.temperature;
        cs->content_intent[608] += w * ins.pain;
        cs->content_intent[609] += w * ins.disgust;
        cs->content_intent[610] += w * ins.empathy;
        cs->content_intent[611] += w * ins.self_awareness;
    }
    NIMCP_LOGGING_DEBUG("Insula: empathy=%.2f self_aware=%.2f",
                        ins.empathy, ins.self_awareness);
    return 0;
}

bool grounded_language_insula_enabled(const void* gl) { return (gl != NULL); }
