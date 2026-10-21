/**
 * @file nimcp_language_acc_bridge.c
 * @brief Anterior cingulate cortex error/conflict integration
 * @date 2026-10-21
 */
#include "language/nimcp_grounded_language.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>

typedef struct {
    float error_signal; float conflict_magnitude;
    float performance_monitoring; float uncertainty;
} acc_state_t;

int acc_extract_monitoring(brain_t brain, acc_state_t* out) {
    if (!brain || !out) return -1;
    out->error_signal = 0.2f; out->conflict_magnitude = 0.3f;
    out->performance_monitoring = 0.6f; out->uncertainty = 0.4f;
    return 0;
}

int grounded_language_integrate_acc(brain_t brain, void* gl, void* state) {
    if (!brain || !gl || !state) return -1;
    extern bool grounded_language_acc_enabled(const void* gl);
    if (!grounded_language_acc_enabled(gl)) return 0;

    typedef struct { float* content_intent; uint32_t dim; } cs_t;
    cs_t* cs = (cs_t*)state;
    acc_state_t acc = {0};
    if (acc_extract_monitoring(brain, &acc) != 0) return -1;

    const float w = 0.17f;
    if (cs->dim > 655) {
        cs->content_intent[635] += w * acc.error_signal;
        cs->content_intent[636] += w * acc.conflict_magnitude;
        cs->content_intent[637] += w * acc.performance_monitoring;
        cs->content_intent[638] += w * acc.uncertainty;
    }
    /* High uncertainty → dampening */
    if (acc.uncertainty > 0.6f) {
        for (uint32_t i = 0; i < cs->dim; i++)
            cs->content_intent[i] *= (1.0f - 0.2f * acc.uncertainty);
    }
    NIMCP_LOGGING_DEBUG("ACC: error=%.2f conflict=%.2f uncertainty=%.2f",
                        acc.error_signal, acc.conflict_magnitude, acc.uncertainty);
    return 0;
}

bool grounded_language_acc_enabled(const void* gl) { return (gl != NULL); }
