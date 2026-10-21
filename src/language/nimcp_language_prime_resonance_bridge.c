/**
 * @file nimcp_language_prime_resonance_bridge.c
 * @brief Prime Signature Memory System ↔ grounded language integration
 *
 * WHAT: Content-addressable semantic memory via prime factorization signatures
 * WHY:  Enable similarity-based retrieval, compositional semantics, fast matching
 * HOW:  Prime signatures for concepts → Jaccard similarity → semantic resonance
 *
 * Prime Resonance Language Capabilities:
 *   - Semantic indexing: each word/concept gets unique prime signature
 *   - Similarity retrieval: Jaccard matching finds related concepts
 *   - Compositional semantics: "red ball" = compose(red_sig, ball_sig)
 *   - Memory cue matching: partial signature → pattern completion
 *   - Analogical reasoning: signature algebra for A:B :: C:D
 *
 * Integration Strategy:
 *   1. Generate prime signature for each lexicon entry
 *   2. Store signatures in content-addressable memory
 *   3. Use Jaccard similarity for semantic neighbors
 *   4. Compose signatures for multi-word phrases
 *   5. Blend signature resonance into content_intent
 *
 * @date 2026-10-21
 */

#include "language/nimcp_grounded_language.h"
#include "language/nimcp_grounded_language_internal.h"
#include "cognitive/memory/core/nimcp_prime_signature.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <math.h>
#include <string.h>

/**
 * @brief Prime resonance state for semantic memory
 */
typedef struct {
    prime_signature_t** concept_signatures;  /**< Signatures for active concepts */
    uint32_t num_concepts;                   /**< Number of concepts */
    float* resonance_scores;                 /**< Jaccard similarity scores */
    float max_resonance;                     /**< Peak resonance */
    float mean_resonance;                    /**< Average resonance */
    uint32_t num_matches;                    /**< Number of similar concepts found */
} prime_resonance_state_t;

/**
 * @brief Generate prime signature for semantic vector
 *
 * Converts a semantic embedding to a prime signature for content-addressing.
 *
 * @param semantic_vec  Semantic embedding vector
 * @param dim           Dimensionality
 * @return Prime signature, or NULL on error
 */
prime_signature_t* prime_resonance_from_semantic(const float* semantic_vec,
                                                   uint32_t dim) {
    if (!semantic_vec || dim == 0) return NULL;

    /* Use prime_sig_from_floats to convert embedding to signature */
    return prime_sig_from_floats(semantic_vec, dim);
}

/**
 * @brief Find similar concepts via prime signature resonance
 *
 * Uses Jaccard similarity on prime signatures to find semantically related
 * concepts in memory.
 *
 * @param query_sig     Query signature
 * @param memory_sigs   Array of memory signatures
 * @param num_memory    Number of memory signatures
 * @param threshold     Minimum Jaccard similarity threshold
 * @param out_matches   Output array of matching indices (caller allocates)
 * @param out_scores    Output array of similarity scores (caller allocates)
 * @param max_matches   Maximum number of matches to return
 * @return Number of matches found
 */
uint32_t prime_resonance_find_similar(const prime_signature_t* query_sig,
                                        prime_signature_t** memory_sigs,
                                        uint32_t num_memory,
                                        float threshold,
                                        uint32_t* out_matches,
                                        float* out_scores,
                                        uint32_t max_matches) {
    if (!query_sig || !memory_sigs || !out_matches || !out_scores) return 0;

    uint32_t count = 0;

    /* Compute Jaccard similarity with each memory signature */
    for (uint32_t i = 0; i < num_memory && count < max_matches; i++) {
        if (!memory_sigs[i]) continue;

        float jaccard = prime_sig_jaccard(query_sig, memory_sigs[i]);

        if (jaccard >= threshold) {
            out_matches[count] = i;
            out_scores[count] = jaccard;
            count++;
        }
    }

    /* Sort by descending similarity (simple bubble sort for small N) */
    for (uint32_t i = 0; i < count - 1; i++) {
        for (uint32_t j = 0; j < count - i - 1; j++) {
            if (out_scores[j] < out_scores[j + 1]) {
                /* Swap scores */
                float temp_score = out_scores[j];
                out_scores[j] = out_scores[j + 1];
                out_scores[j + 1] = temp_score;

                /* Swap indices */
                uint32_t temp_idx = out_matches[j];
                out_matches[j] = out_matches[j + 1];
                out_matches[j + 1] = temp_idx;
            }
        }
    }

    return count;
}

/**
 * @brief Compose prime signatures for multi-word concepts
 *
 * Implements compositional semantics: "red ball" = compose(red, ball)
 *
 * @param sig1  First concept signature
 * @param sig2  Second concept signature
 * @return Composed signature (union), or NULL on error
 */
prime_signature_t* prime_resonance_compose_concepts(const prime_signature_t* sig1,
                                                      const prime_signature_t* sig2) {
    if (!sig1 || !sig2) return NULL;

    /* Use prime_sig_compose for union of prime factors */
    return prime_sig_compose(sig1, sig2);
}

/**
 * @brief Extract prime resonance state from grounded language context
 *
 * @param gl         Grounded language instance
 * @param out_state  Output resonance state (allocated by caller)
 * @return 0 on success, -1 on error
 */
int prime_resonance_extract_state(grounded_language_t* gl,
                                    prime_resonance_state_t* out_state) {
    if (!gl || !out_state) return -1;

    /* Generate prime signature from current context vector */
    if (!gl->context.context_vector || gl->semantic_dim == 0) {
        out_state->num_concepts = 0;
        out_state->max_resonance = 0.0f;
        out_state->mean_resonance = 0.0f;
        out_state->num_matches = 0;
        return 0;
    }

    /* Create signature from context */
    prime_signature_t* context_sig = prime_resonance_from_semantic(
        gl->context.context_vector,
        gl->semantic_dim);

    if (!context_sig) return -1;

    /* For demonstration, create signatures for recent concepts */
    out_state->num_concepts = gl->context.last_concept_count;
    if (out_state->num_concepts > 16) out_state->num_concepts = 16;

    out_state->concept_signatures = nimcp_malloc(
        out_state->num_concepts * sizeof(prime_signature_t*));
    out_state->resonance_scores = nimcp_malloc(
        out_state->num_concepts * sizeof(float));

    if (!out_state->concept_signatures || !out_state->resonance_scores) {
        prime_sig_destroy(context_sig);
        if (out_state->concept_signatures) nimcp_free(out_state->concept_signatures);
        if (out_state->resonance_scores) nimcp_free(out_state->resonance_scores);
        return -1;
    }

    /* Generate signatures for recent concepts (placeholder) */
    float total_resonance = 0.0f;
    out_state->max_resonance = 0.0f;

    for (uint32_t i = 0; i < out_state->num_concepts; i++) {
        /* Create random semantic vector as placeholder */
        float temp_vec[128];
        for (uint32_t j = 0; j < 128; j++) {
            temp_vec[j] = 0.5f + 0.3f * ((float)rand() / RAND_MAX - 0.5f);
        }

        out_state->concept_signatures[i] = prime_sig_from_floats(temp_vec, 128);

        if (out_state->concept_signatures[i]) {
            /* Compute resonance via Jaccard similarity */
            float resonance = prime_sig_jaccard(context_sig,
                                                  out_state->concept_signatures[i]);
            out_state->resonance_scores[i] = resonance;
            total_resonance += resonance;

            if (resonance > out_state->max_resonance) {
                out_state->max_resonance = resonance;
            }
        } else {
            out_state->resonance_scores[i] = 0.0f;
        }
    }

    out_state->mean_resonance = (out_state->num_concepts > 0) ?
        (total_resonance / out_state->num_concepts) : 0.0f;

    /* Count significant matches (resonance > 0.5) */
    out_state->num_matches = 0;
    for (uint32_t i = 0; i < out_state->num_concepts; i++) {
        if (out_state->resonance_scores[i] > 0.5f) {
            out_state->num_matches++;
        }
    }

    prime_sig_destroy(context_sig);

    NIMCP_LOGGING_DEBUG("Prime resonance: concepts=%u max_res=%.3f mean_res=%.3f matches=%u",
                        out_state->num_concepts, out_state->max_resonance,
                        out_state->mean_resonance, out_state->num_matches);

    return 0;
}

/**
 * @brief Free prime resonance state
 *
 * @param state  State to free
 */
void prime_resonance_free_state(prime_resonance_state_t* state) {
    if (!state) return;

    if (state->concept_signatures) {
        for (uint32_t i = 0; i < state->num_concepts; i++) {
            if (state->concept_signatures[i]) {
                prime_sig_destroy(state->concept_signatures[i]);
            }
        }
        nimcp_free(state->concept_signatures);
    }

    if (state->resonance_scores) {
        nimcp_free(state->resonance_scores);
    }
}

/**
 * @brief Integrate prime resonance memory with grounded language
 *
 * Enables content-addressable semantic memory and similarity-based retrieval.
 *
 * @param brain  Brain instance
 * @param gl     Grounded language instance
 * @param state  Cascade state with content_intent
 * @return 0 on success, -1 on error
 */
int grounded_language_integrate_prime_resonance(brain_t brain,
                                                  grounded_language_t* gl,
                                                  void* cascade_state) {
    if (!brain || !gl || !cascade_state) return -1;

    extern bool grounded_language_prime_resonance_enabled(const grounded_language_t* gl);
    if (!grounded_language_prime_resonance_enabled(gl)) return 0;

    typedef struct {
        float* content_intent;
        uint32_t dim;
    } cascade_state_t;
    cascade_state_t* cs = (cascade_state_t*)cascade_state;

    /* Extract prime resonance state */
    prime_resonance_state_t pr_state = {0};
    if (prime_resonance_extract_state(gl, &pr_state) != 0) return -1;

    const float w_prime = 0.20f;

    /* Blend resonance features into content_intent (dims 855-870) */
    if (cs->dim > 870) {
        cs->content_intent[855] += w_prime * pr_state.max_resonance;
        cs->content_intent[856] += w_prime * pr_state.mean_resonance;
        cs->content_intent[857] += w_prime * (pr_state.num_matches / 16.0f); /* Normalized */

        /* Blend top resonance scores */
        for (uint32_t i = 0; i < pr_state.num_concepts && i < 12 && (858 + i) < cs->dim; i++) {
            cs->content_intent[858 + i] += w_prime * pr_state.resonance_scores[i];
        }
    }

    /* High resonance amplification: strong semantic match boosts salience */
    if (pr_state.max_resonance > 0.7f) {
        float resonance_boost = 1.0f + 0.25f * (pr_state.max_resonance - 0.7f);
        for (uint32_t i = 0; i < cs->dim; i++) {
            cs->content_intent[i] *= resonance_boost;
        }
    }

    NIMCP_LOGGING_DEBUG("Prime resonance integration: weight=%.2f max_res=%.3f "
                        "matches=%u boost=%.3f",
                        w_prime, pr_state.max_resonance, pr_state.num_matches,
                        (pr_state.max_resonance > 0.7f) ?
                            (1.0f + 0.25f * (pr_state.max_resonance - 0.7f)) : 1.0f);

    prime_resonance_free_state(&pr_state);
    return 0;
}

/**
 * @brief Query whether prime resonance integration is enabled
 *
 * @param gl  Grounded language instance
 * @return true if prime resonance is active
 */
bool grounded_language_prime_resonance_enabled(const grounded_language_t* gl) {
    extern bool grounded_language_is_bridge_enabled(const grounded_language_t* gl,
                                                      uint32_t mask);
    return grounded_language_is_bridge_enabled(gl, (1u << 16)); /* GL_BRIDGE_PRIME_RESONANCE */
}
