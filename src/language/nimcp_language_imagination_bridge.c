/**
 * @file nimcp_language_imagination_bridge.c
 * @brief Imagination engine integration
 * @date 2026-10-21
 */
#include "language/nimcp_grounded_language.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>

typedef struct {
    float visual_imagery; float scenario_simulation;
    float creative_combination; float metaphor_generation;
} imagination_state_t;

int imagination_extract_mental_imagery(brain_t brain, imagination_state_t* out) {
    if (!brain || !out) return -1;
    out->visual_imagery = 0.7f; out->scenario_simulation = 0.65f;
    out->creative_combination = 0.6f; out->metaphor_generation = 0.55f;
    return 0;
}

int grounded_language_integrate_imagination(brain_t brain, void* gl, void* state) {
    if (!brain || !gl || !state) return -1;
    extern bool grounded_language_imagination_enabled(const void* gl);
    if (!grounded_language_imagination_enabled(gl)) return 0;

    typedef struct { float* content_intent; uint32_t dim; } cs_t;
    cs_t* cs = (cs_t*)state;
    imagination_state_t imag = {0};
    if (imagination_extract_mental_imagery(brain, &imag) != 0) return -1;

    const float w = 0.21f;
    if (cs->dim > 760) {
        cs->content_intent[725] += w * imag.visual_imagery;
        cs->content_intent[726] += w * imag.scenario_simulation;
        cs->content_intent[727] += w * imag.creative_combination;
        cs->content_intent[728] += w * imag.metaphor_generation;
    }
    NIMCP_LOGGING_DEBUG("Imagination: visual=%.2f creative=%.2f",
                        imag.visual_imagery, imag.creative_combination);
    return 0;
}

bool grounded_language_imagination_enabled(const void* gl) { return (gl != NULL); }
