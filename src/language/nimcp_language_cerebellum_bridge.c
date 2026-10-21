/**
 * @file nimcp_language_cerebellum_bridge.c
 * @brief Cerebellum timing/prediction integration
 * @date 2026-10-21
 */
#include "language/nimcp_grounded_language.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>

typedef struct {
    float timing_precision; float prediction_error;
    float motor_sequence; float procedural_knowledge;
} cerebellum_state_t;

int cerebellum_extract_timing(brain_t brain, cerebellum_state_t* out) {
    if (!brain || !out) return -1;
    out->timing_precision = 0.75f; out->prediction_error = 0.15f;
    out->motor_sequence = 0.6f; out->procedural_knowledge = 0.7f;
    return 0;
}

int grounded_language_integrate_cerebellum(brain_t brain, void* gl, void* state) {
    if (!brain || !gl || !state) return -1;
    extern bool grounded_language_cerebellum_enabled(const void* gl);
    if (!grounded_language_cerebellum_enabled(gl)) return 0;

    typedef struct { float* content_intent; uint32_t dim; } cs_t;
    cs_t* cs = (cs_t*)state;
    cerebellum_state_t cereb = {0};
    if (cerebellum_extract_timing(brain, &cereb) != 0) return -1;

    const float w = 0.16f;
    if (cs->dim > 475) {
        cs->content_intent[450] += w * cereb.timing_precision;
        cs->content_intent[451] += w * cereb.prediction_error;
        cs->content_intent[452] += w * cereb.motor_sequence;
        cs->content_intent[453] += w * cereb.procedural_knowledge;
    }
    NIMCP_LOGGING_DEBUG("Cerebellum: timing=%.2f prediction_error=%.2f",
                        cereb.timing_precision, cereb.prediction_error);
    return 0;
}

bool grounded_language_cerebellum_enabled(const void* gl) { return (gl != NULL); }
