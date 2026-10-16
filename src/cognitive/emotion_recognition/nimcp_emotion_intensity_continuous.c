/**
 * @file nimcp_emotion_intensity_continuous.c
 * @brief Continuous emotion intensity effects computation
 *
 * WHAT: Compute smooth intensity effects from continuous float [0.0-1.0]
 * WHY:  Replaces discrete NONE/LOW/MEDIUM/HIGH/EXTREME dispatch with
 *       smooth mathematical functions for proportional response scaling
 * HOW:  Sigmoid for urgency, inverted-U for empathy, exponential for
 *       intervention, power law for de-escalation, quadratic for grounding
 */

#include "cognitive/nimcp_emotion_recognition.h"
#include <math.h>
#include <stddef.h>

int emotion_compute_intensity_effects(float intensity,
                                      emotion_intensity_effects_t *out) {
    if (!out) {
        return -1;
    }

    /* Clamp input to [0, 1] */
    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;

    /* Response urgency: sigmoid centered at 0.5 — urgent above moderate */
    out->response_urgency = 1.0f / (1.0f + expf(-10.0f * (intensity - 0.5f)));

    /* Empathy weight: always high, peaks at moderate-high intensity */
    /* Inverted-U: peak at 0.65, stays high across range */
    float t = (intensity - 0.65f) / 0.4f;
    out->empathy_weight = 0.5f + 0.5f * expf(-t * t);

    /* Intervention probability: exponential above 0.3 threshold */
    if (intensity < 0.3f) {
        out->intervention_probability = 0.0f;
    } else {
        out->intervention_probability = 1.0f - expf(-4.0f * (intensity - 0.3f));
        if (out->intervention_probability > 1.0f)
            out->intervention_probability = 1.0f;
    }

    /* De-escalation strength: higher for more intense emotions */
    out->de_escalation_strength = powf(intensity, 0.7f);

    /* Grounding need: quadratic — low emotions don't need grounding */
    out->grounding_need = intensity * intensity;

    /* Derive discrete label */
    out->label = emotion_intensity_from_float(intensity);

    return 0;
}
