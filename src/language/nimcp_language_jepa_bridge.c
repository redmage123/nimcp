/**
 * @file nimcp_language_jepa_bridge.c
 * @brief JEPA (Joint Embedding Predictive Architecture) ↔ language integration
 *
 * WHAT: Predictive semantics from JEPA world model for anticipatory language
 * WHY:  Enables fluent continuation, contrastive learning, counterfactual generation
 * HOW:  JEPA predicts next semantic state → blend into production target
 *
 * JEPA language integration:
 *   - State prediction: s_t → s_{t+1} for fluent continuation
 *   - Contrastive pairs: positive (paraphrase) vs negative (contradiction)
 *   - Action conditioning: "if X then Y" from action-conditioned predictions
 *   - Uncertainty: prediction confidence guides exploration vs exploitation
 *
 * @author NIMCP Development Team
 * @date 2026-10-21
 */

#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_internal.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_handler.h"
#include "utils/logging/nimcp_logging.h"

#include <math.h>
#include <string.h>

/**
 * @brief JEPA prediction result for semantic state
 */
typedef struct {
    float* predicted_state;      /**< Predicted next semantic state */
    float prediction_confidence; /**< Prediction certainty [0,1] */
    float prediction_entropy;    /**< Uncertainty in prediction */
    uint32_t dim;                /**< State dimensionality */
} jepa_semantic_prediction_t;

/**
 * @brief Predict next semantic state from current using JEPA
 *
 * Uses JEPA world model to predict semantic evolution:
 *   s_{t+1} = f_JEPA(s_t)
 *
 * @param brain         Brain with JEPA bridges
 * @param current_state Current semantic state vector
 * @param dim           State dimensionality
 * @param out_pred      Output prediction structure (allocated by caller)
 * @return 0 on success, -1 on error
 */
int jepa_predict_next_semantic_state(brain_t brain,
                                       const float* current_state,
                                       uint32_t dim,
                                       jepa_semantic_prediction_t* out_pred) {
    if (!brain || !current_state || !out_pred) return -1;

    /* Check if JEPA bridges are available.
     * JEPA bridges are tick-driven in brain_learn_vector (Phase 4n-t).
     * We query their current state for prediction. */

    /* Allocate predicted state vector */
    out_pred->predicted_state = nimcp_malloc(dim * sizeof(float));
    if (!out_pred->predicted_state) return -1;
    out_pred->dim = dim;

    /* JEPA prediction via world model forward pass.
     * The JEPA bridges maintain internal state; we extract their
     * prediction of next state given current semantic context.
     *
     * Placeholder implementation: Linear extrapolation + noise.
     * Full implementation: Query jepa_omni_bridge, jepa_engram_bridge, etc. */

    float momentum = 0.9f; /* Temporal smoothing */
    float noise_scale = 0.05f; /* Exploration noise */

    for (uint32_t i = 0; i < dim; i++) {
        /* Predicted state = momentum * current + noise */
        float noise = noise_scale * ((float)rand() / RAND_MAX - 0.5f);
        out_pred->predicted_state[i] = momentum * current_state[i] + noise;
    }

    /* Prediction confidence: high when state is stable, low when uncertain.
     * Compute from state variance or JEPA uncertainty estimates. */
    float state_variance = 0.0f;
    for (uint32_t i = 0; i < dim && i < 32; i++) {
        float deviation = current_state[i] - 0.0f; /* Mean assumed zero */
        state_variance += deviation * deviation;
    }
    state_variance /= fminf(dim, 32);

    /* High variance → low confidence */
    out_pred->prediction_confidence = 1.0f / (1.0f + state_variance);

    /* Prediction entropy: measure of uncertainty */
    out_pred->prediction_entropy = -out_pred->prediction_confidence *
                                     logf(out_pred->prediction_confidence + 1e-8f);

    NIMCP_LOGGING_DEBUG("JEPA prediction: confidence=%.3f entropy=%.3f variance=%.3f",
                        out_pred->prediction_confidence,
                        out_pred->prediction_entropy,
                        state_variance);

    return 0;
}

/**
 * @brief Generate contrastive semantic pairs for language learning
 *
 * Creates positive pairs (paraphrase/similar) and negative pairs
 * (contradictory/dissimilar) for contrastive language learning.
 *
 * Positive pair: "The cat sat" ↔ "A feline rested"
 * Negative pair: "The cat sat" ↔ "The dog ran"
 *
 * @param gl             Grounded language instance
 * @param anchor_text    Anchor text for contrastive pairs
 * @param out_positive   Output positive pair text (caller allocates buffer)
 * @param out_negative   Output negative pair text (caller allocates buffer)
 * @param buffer_size    Size of output buffers
 * @return 0 on success, -1 on error
 */
int jepa_generate_contrastive_pairs(grounded_language_t* gl,
                                      const char* anchor_text,
                                      char* out_positive,
                                      char* out_negative,
                                      size_t buffer_size) {
    if (!gl || !anchor_text || !out_positive || !out_negative) return -1;

    /* Comprehend anchor text to get semantic state */
    gl_comprehension_result_t anchor_comp;
    if (grounded_language_comprehend(gl, anchor_text, &anchor_comp) != 0) {
        return -1;
    }

    /* Generate positive pair: small perturbation in semantic space.
     * This is paraphrasing via semantic neighbor sampling. */
    float* pos_state = nimcp_malloc(gl->semantic_dim * sizeof(float));
    if (!pos_state) return -1;

    const float pos_epsilon = 0.1f; /* Small perturbation for paraphrase */
    for (uint32_t i = 0; i < gl->semantic_dim; i++) {
        float noise = pos_epsilon * ((float)rand() / RAND_MAX - 0.5f);
        pos_state[i] = anchor_comp.semantic_vector[i] + noise;
    }

    /* Produce text from positive state */
    gl_produce_result_t pos_result;
    extern int grounded_language_produce_from_state(grounded_language_t* gl,
                                                      const float* state,
                                                      uint32_t dim,
                                                      gl_produce_result_t* out);
    if (grounded_language_produce_from_state(gl, pos_state, gl->semantic_dim,
                                              &pos_result) == 0) {
        strncpy(out_positive, pos_result.text, buffer_size - 1);
        out_positive[buffer_size - 1] = '\0';
    }
    nimcp_free(pos_state);

    /* Generate negative pair: large perturbation or semantic negation.
     * This is contradiction via distant semantic point. */
    float* neg_state = nimcp_malloc(gl->semantic_dim * sizeof(float));
    if (!neg_state) return -1;

    const float neg_epsilon = 1.0f; /* Large perturbation for contradiction */
    for (uint32_t i = 0; i < gl->semantic_dim; i++) {
        float noise = neg_epsilon * ((float)rand() / RAND_MAX - 0.5f);
        neg_state[i] = -anchor_comp.semantic_vector[i] + noise; /* Semantic negation */
    }

    /* Produce text from negative state */
    gl_produce_result_t neg_result;
    if (grounded_language_produce_from_state(gl, neg_state, gl->semantic_dim,
                                              &neg_result) == 0) {
        strncpy(out_negative, neg_result.text, buffer_size - 1);
        out_negative[buffer_size - 1] = '\0';
    }
    nimcp_free(neg_state);

    NIMCP_LOGGING_DEBUG("JEPA contrastive: anchor='%s' pos='%s' neg='%s'",
                        anchor_text, out_positive, out_negative);

    return 0;
}

/**
 * @brief Integrate JEPA predictive semantics into grounded language
 *
 * Blends JEPA-predicted next semantic state into language production target
 * for fluent, anticipatory continuation.
 *
 * @param brain  Brain with JEPA bridges
 * @param gl     Grounded language instance
 * @param state  Cascade state with content_intent
 * @return 0 on success, -1 on error
 */
int grounded_language_integrate_jepa_predictive(brain_t brain,
                                                  grounded_language_t* gl,
                                                  void* cascade_state) {
    if (!brain || !gl || !cascade_state) return -1;

    /* Check if JEPA coupling is enabled */
    extern bool grounded_language_jepa_predictive_enabled(const grounded_language_t* gl);
    if (!grounded_language_jepa_predictive_enabled(gl)) {
        return 0; /* Coupling disabled */
    }

    /* Get cascade state */
    typedef struct {
        float* content_intent;
        uint32_t dim;
    } cascade_state_t;
    cascade_state_t* cs = (cascade_state_t*)cascade_state;

    /* Predict next semantic state from current content_intent */
    jepa_semantic_prediction_t pred;
    if (jepa_predict_next_semantic_state(brain, cs->content_intent,
                                          cs->dim, &pred) != 0) {
        return -1;
    }

    /* Blend predicted state into content_intent:
     * High confidence → use prediction (fluent continuation)
     * Low confidence → explore (creative deviation) */
    const float w_prediction = 0.25f;
    float blend_weight = w_prediction * pred.prediction_confidence;

    for (uint32_t i = 0; i < cs->dim; i++) {
        cs->content_intent[i] = (1.0f - blend_weight) * cs->content_intent[i] +
                                 blend_weight * pred.predicted_state[i];
    }

    /* Log JEPA integration */
    NIMCP_LOGGING_DEBUG("JEPA predictive: confidence=%.3f blend_weight=%.3f "
                        "entropy=%.3f",
                        pred.prediction_confidence, blend_weight,
                        pred.prediction_entropy);

    /* Free allocated prediction */
    if (pred.predicted_state) nimcp_free(pred.predicted_state);

    return 0;
}

/**
 * @brief Query whether JEPA predictive coupling is enabled
 *
 * @param gl  Grounded language instance
 * @return true if JEPA predictive integration is active
 */
bool grounded_language_jepa_predictive_enabled(const grounded_language_t* gl) {
    if (!gl) return false;
    /* For now, always enabled when JEPA bridges are present.
     * Future: add runtime flag to grounded_language struct. */
    return true;
}
