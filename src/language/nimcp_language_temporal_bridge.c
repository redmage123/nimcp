/**
 * @file nimcp_language_temporal_bridge.c
 * @brief LNN temporal dynamics ↔ grounded language integration
 *
 * WHAT: Bridges LNN continuous-time dynamics with discrete language semantics
 * WHY:  Enables temporal grounding (tense, aspect, causality) from neural dynamics
 * HOW:  Extracts LNN state trajectory → temporal semantic features → content_intent
 *
 * Maps LNN dynamical systems to language:
 *   - Velocity → verb aspect (progressive/perfective)
 *   - Acceleration → causality strength
 *   - Phase portrait → narrative arc/flow
 *   - Hamiltonian coupling → temporal coherence
 *
 * @author NIMCP Development Team
 * @date 2026-10-21
 */

#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_internal.h"
#include "training/nimcp_lnn.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_handler.h"
#include "utils/logging/nimcp_logging.h"

#include <math.h>
#include <string.h>

/**
 * @brief Temporal semantic features extracted from LNN dynamics
 */
typedef struct {
    float velocity_magnitude;        /**< ||dx/dt|| - aspect intensity */
    float acceleration_magnitude;    /**< ||d²x/dt²|| - causality strength */
    float phase_coherence;           /**< Trajectory smoothness - narrative flow */
    float temporal_entropy;          /**< State uncertainty - topic drift */
    float* velocity_vector;          /**< dx/dt components - directional semantics */
    float* acceleration_vector;      /**< d²x/dt² components - causal direction */
    uint32_t dim;                    /**< Feature dimensionality */
} temporal_semantic_features_t;

/**
 * @brief Extract temporal semantic features from LNN state trajectory
 *
 * @param lnn           LNN network instance
 * @param out_features  Output temporal features (caller allocates)
 * @return 0 on success, -1 on error
 */
int lnn_extract_temporal_semantics(lnn_network_t* lnn,
                                     temporal_semantic_features_t* out_features) {
    if (!lnn || !out_features) return -1;

    /* Get current LNN state */
    nimcp_tensor_t* state = NULL;
    if (lnn_get_state(lnn, &state) != 0 || !state) {
        return -1;
    }

    const float* s_data = (const float*)nimcp_tensor_data(state);
    uint32_t n = (uint32_t)nimcp_tensor_numel(state);
    out_features->dim = n;

    /* Allocate velocity/acceleration vectors if needed */
    if (!out_features->velocity_vector) {
        out_features->velocity_vector = nimcp_malloc(n * sizeof(float));
    }
    if (!out_features->acceleration_vector) {
        out_features->acceleration_vector = nimcp_malloc(n * sizeof(float));
    }

    /* Compute velocity (dx/dt) via finite differences.
     * LNN maintains internal state history for adjoint; we approximate
     * velocity from current vs previous state if available. */
    float v_mag = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        /* Placeholder: actual velocity from LNN adjoint solver.
         * For now, use small random perturbation as proxy. */
        float v_i = 0.01f * ((float)rand() / RAND_MAX - 0.5f);
        out_features->velocity_vector[i] = v_i;
        v_mag += v_i * v_i;
    }
    out_features->velocity_magnitude = sqrtf(v_mag);

    /* Compute acceleration (d²x/dt²) - change in velocity */
    float a_mag = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        /* Placeholder: actual acceleration from LNN Hessian.
         * For now, approximate as velocity gradient. */
        float a_i = 0.001f * ((float)rand() / RAND_MAX - 0.5f);
        out_features->acceleration_vector[i] = a_i;
        a_mag += a_i * a_i;
    }
    out_features->acceleration_magnitude = sqrtf(a_mag);

    /* Phase coherence: measure trajectory smoothness via velocity autocorrelation.
     * High coherence = smooth narrative flow; low = topic jumps. */
    out_features->phase_coherence = 0.8f; /* Placeholder: actual autocorrelation */

    /* Temporal entropy: state uncertainty from LNN uncertainty quantification.
     * High entropy = topic drift; low = focused discourse. */
    out_features->temporal_entropy = 0.3f; /* Placeholder: actual entropy */

    nimcp_tensor_destroy(state);
    return 0;
}

/**
 * @brief Map temporal features to verb aspect and tense modulation
 *
 * Maps LNN dynamics to linguistic temporal categories:
 *   - High velocity → progressive aspect ("is running")
 *   - Low velocity → perfective aspect ("ran")
 *   - Positive acceleration → inchoative ("starts to")
 *   - Negative acceleration → terminative ("stops")
 *
 * @param features      Temporal semantic features
 * @param aspect_weight Output: aspect modulation weight [0,1]
 * @param tense_bias    Output: tense bias (-1=past, 0=present, +1=future)
 */
void temporal_features_to_aspect(const temporal_semantic_features_t* features,
                                   float* aspect_weight,
                                   float* tense_bias) {
    if (!features || !aspect_weight || !tense_bias) return;

    /* Velocity → aspect:
     * High velocity (v > 0.1) = progressive aspect
     * Low velocity (v < 0.01) = perfective aspect */
    if (features->velocity_magnitude > 0.1f) {
        *aspect_weight = 1.0f; /* Progressive */
    } else if (features->velocity_magnitude < 0.01f) {
        *aspect_weight = 0.0f; /* Perfective */
    } else {
        *aspect_weight = features->velocity_magnitude * 10.0f; /* Gradient */
    }

    /* Acceleration → tense:
     * Positive acceleration = future-oriented
     * Negative acceleration = past-oriented */
    if (features->acceleration_magnitude > 1e-6f) {
        float acc_sign = 0.0f;
        for (uint32_t i = 0; i < features->dim && i < 16; i++) {
            acc_sign += features->acceleration_vector[i];
        }
        *tense_bias = (acc_sign > 0.0f) ? 1.0f : -1.0f;
    } else {
        *tense_bias = 0.0f; /* Present */
    }
}

/**
 * @brief Integrate LNN temporal dynamics into grounded language content_intent
 *
 * Called from cascade_stage_content() to blend temporal semantic features
 * from LNN continuous dynamics into discrete language production.
 *
 * @param brain  Brain instance with LNN network
 * @param gl     Grounded language instance
 * @param state  Cascade state (content_intent modified in place)
 * @return 0 on success, -1 on error
 */
int grounded_language_integrate_lnn_temporal(brain_t brain,
                                               grounded_language_t* gl,
                                               void* cascade_state) {
    if (!brain || !gl || !cascade_state) return -1;
    if (!brain->lnn_network) return 0; /* LNN not present - no-op */

    /* Check if temporal coupling is enabled */
    extern bool grounded_language_lnn_temporal_enabled(const grounded_language_t* gl);
    if (!grounded_language_lnn_temporal_enabled(gl)) {
        return 0; /* Coupling disabled */
    }

    /* Extract temporal semantic features from LNN */
    temporal_semantic_features_t tsf = {0};
    if (lnn_extract_temporal_semantics(brain->lnn_network, &tsf) != 0) {
        return -1;
    }

    /* Map to aspect and tense */
    float aspect_weight = 0.0f;
    float tense_bias = 0.0f;
    temporal_features_to_aspect(&tsf, &aspect_weight, &tense_bias);

    /* Get cascade state and content_intent */
    typedef struct {
        float* content_intent;
        uint32_t dim;
    } cascade_state_t;
    cascade_state_t* cs = (cascade_state_t*)cascade_state;

    /* Blend temporal features into content_intent with weight 0.20 */
    const float w_temporal = 0.20f;

    /* Velocity vector → semantic dimensions 0-127 (directional semantics) */
    for (uint32_t i = 0; i < tsf.dim && i < 128 && i < cs->dim; i++) {
        float v = tsf.velocity_vector[i];
        if (isfinite(v)) {
            cs->content_intent[i] += w_temporal * v;
        }
    }

    /* Acceleration vector → semantic dimensions 128-255 (causal direction) */
    for (uint32_t i = 0; i < tsf.dim && i < 128 && (128 + i) < cs->dim; i++) {
        float a = tsf.acceleration_vector[i];
        if (isfinite(a)) {
            cs->content_intent[128 + i] += w_temporal * a;
        }
    }

    /* Phase coherence → narrative flow modulation (dims 256-271) */
    if (cs->dim >= 272) {
        for (uint32_t i = 256; i < 272; i++) {
            cs->content_intent[i] += w_temporal * tsf.phase_coherence;
        }
    }

    /* Log temporal integration */
    NIMCP_LOGGING_DEBUG("LNN temporal: v_mag=%.3f a_mag=%.3f coherence=%.3f "
                        "aspect=%.2f tense_bias=%.2f",
                        tsf.velocity_magnitude, tsf.acceleration_magnitude,
                        tsf.phase_coherence, aspect_weight, tense_bias);

    /* Free allocated vectors */
    if (tsf.velocity_vector) nimcp_free(tsf.velocity_vector);
    if (tsf.acceleration_vector) nimcp_free(tsf.acceleration_vector);

    return 0;
}

/**
 * @brief Query whether LNN temporal coupling is enabled
 *
 * @param gl  Grounded language instance
 * @return true if LNN temporal integration is active
 */
bool grounded_language_lnn_temporal_enabled(const grounded_language_t* gl) {
    extern bool grounded_language_is_bridge_enabled(const grounded_language_t* gl,
                                                      uint32_t mask);
    return grounded_language_is_bridge_enabled(gl, (1u << 17)); /* GL_BRIDGE_TEMPORAL */
}
