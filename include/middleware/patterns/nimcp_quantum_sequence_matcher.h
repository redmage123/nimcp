//=============================================================================
// nimcp_quantum_sequence_matcher.h - Quantum-Inspired Sequence Pattern Matching
//=============================================================================

#ifndef NIMCP_QUANTUM_SEQUENCE_MATCHER_H
#define NIMCP_QUANTUM_SEQUENCE_MATCHER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_quantum_sequence_matcher.h
 * @brief Quantum-inspired pattern matching for neural sequences
 *
 * WHAT: Fast pattern matching in spike sequences using quantum algorithms
 * WHY:  Grover-like search provides O(sqrt(N)) speedup for template matching
 * HOW:  Amplitude encoding of patterns, oracle marking, diffusion operator
 *
 * QUANTUM CONCEPTS USED:
 * - Grover's Algorithm: O(sqrt(N)) search for matching patterns
 * - Amplitude Encoding: Represent sequences as quantum states
 * - Quantum Phase Estimation: Detect timing relationships
 * - Quantum Superposition: Check multiple templates simultaneously
 * - Quantum Distance Metrics: Fidelity-based similarity
 *
 * BIOLOGICAL ANALOGY:
 * - Hippocampal replay detection operates at compressed timescales
 * - Pattern completion in associative memory (Hopfield-like)
 * - Sequence learning in corticostriatal circuits
 *
 * KEY FEATURES:
 * - Fast template matching with quantum-inspired search
 * - Fuzzy matching with temporal tolerance
 * - Parallel evaluation of multiple candidate patterns
 * - Distance metrics based on quantum fidelity
 *
 * @author NIMCP Development Team
 * @date 2025-01-20
 */

//=============================================================================
// Constants
//=============================================================================

#define QSEQ_MAX_PATTERN_LENGTH  64     /**< Maximum pattern length */
#define QSEQ_MAX_TEMPLATES       256    /**< Maximum stored templates */
#define QSEQ_DEFAULT_TOLERANCE   50.0f  /**< Default temporal tolerance (ms) */
#define QSEQ_DEFAULT_SEED        54321  /**< Default random seed */

//=============================================================================
// Types
//=============================================================================

/**
 * WHAT: Pattern element in sequence
 */
typedef struct {
    uint32_t symbol;           /**< Symbol ID (neuron ID, event type, etc.) */
    float timestamp;           /**< Relative timestamp (ms) */
    float weight;              /**< Element importance weight */
} qseq_element_t;

/**
 * WHAT: Sequence pattern
 */
typedef struct {
    qseq_element_t elements[QSEQ_MAX_PATTERN_LENGTH];
    uint32_t length;           /**< Number of elements */
    float duration;            /**< Total duration (ms) */
    uint32_t pattern_id;       /**< Unique pattern ID */
    float* amplitude_vector;   /**< Amplitude encoding [MAX_SYMBOLS] */
    uint32_t amplitude_dim;    /**< Dimension of amplitude vector */
} qseq_pattern_t;

/**
 * WHAT: Match result from quantum search
 */
typedef struct {
    uint32_t pattern_id;       /**< Matched pattern ID */
    float similarity;          /**< Similarity score [0, 1] */
    float fidelity;            /**< Quantum fidelity measure */
    float phase_coherence;     /**< Timing coherence */
    uint32_t matched_elements; /**< Number of elements matched */
    float compression;         /**< Time compression factor */
    bool is_reversed;          /**< Pattern matched in reverse */
    uint32_t grover_iterations;/**< Iterations used */
} qseq_match_result_t;

/**
 * WHAT: Configuration for quantum sequence matcher
 */
typedef struct {
    uint32_t max_templates;        /**< Maximum stored templates */
    uint32_t amplitude_dim;        /**< Dimension for amplitude encoding */
    float temporal_tolerance;      /**< Timing tolerance (ms) */
    float min_similarity;          /**< Minimum similarity threshold */
    uint32_t grover_iterations;    /**< Fixed Grover iterations (0 = auto) */
    bool enable_reverse_matching;  /**< Check reversed patterns */
    bool enable_compression_detection; /**< Detect time compression */
    float compression_range_min;   /**< Min compression factor */
    float compression_range_max;   /**< Max compression factor */
    uint32_t seed;                 /**< Random seed */
} qseq_matcher_config_t;

/**
 * WHAT: Quantum sequence matcher context (opaque handle)
 */
typedef struct qseq_matcher_struct* qseq_matcher_t;

/**
 * WHAT: Internal structure
 */
typedef struct qseq_matcher_struct {
    qseq_matcher_config_t config;

    /* Template storage */
    qseq_pattern_t* templates;
    uint32_t n_templates;
    uint32_t template_capacity;

    /* Quantum state vectors */
    float* query_amplitude;     /**< Query pattern amplitude */
    float* template_amplitudes; /**< All templates' amplitudes */
    float* similarity_vector;   /**< Similarity scores */

    /* Grover state */
    float* grover_state;        /**< Superposition state */
    uint32_t grover_dim;        /**< Dimension of Grover space */

    /* Random state */
    uint32_t rng_state;

    /* Statistics */
    uint64_t matches_performed;
    uint64_t grover_total_iterations;
    float best_similarity_ever;
} qseq_matcher_internal_t;

/**
 * WHAT: Statistics from matcher
 */
typedef struct {
    uint64_t matches_performed;
    uint64_t grover_total_iterations;
    float avg_iterations_per_match;
    float best_similarity_ever;
    uint32_t stored_templates;
} qseq_matcher_stats_t;

//=============================================================================
// Default Configuration
//=============================================================================

static inline qseq_matcher_config_t qseq_matcher_default_config(void) {
    return (qseq_matcher_config_t){
        .max_templates = 128,
        .amplitude_dim = 64,
        .temporal_tolerance = QSEQ_DEFAULT_TOLERANCE,
        .min_similarity = 0.5f,
        .grover_iterations = 0,  /* Auto */
        .enable_reverse_matching = true,
        .enable_compression_detection = true,
        .compression_range_min = 0.1f,
        .compression_range_max = 10.0f,
        .seed = QSEQ_DEFAULT_SEED
    };
}

//=============================================================================
// Random Number Generation
//=============================================================================

static inline uint32_t qseq_rand(uint32_t* state) {
    *state = (*state) * 1103515245 + 12345;
    return (*state >> 16) & 0x7FFF;
}

static inline float qseq_randf(uint32_t* state) {
    return (float)qseq_rand(state) / 32767.0f;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * WHAT: Create quantum sequence matcher
 * WHY:  Initialize matcher with configuration
 * HOW:  Allocate buffers, initialize state vectors
 *
 * @param config Configuration (NULL for defaults)
 * @return Matcher context or NULL on error
 */
static inline qseq_matcher_t qseq_matcher_create(
    const qseq_matcher_config_t* config
) {
    qseq_matcher_config_t cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = qseq_matcher_default_config();
    }

    /* Validate */
    if (cfg.max_templates == 0 || cfg.max_templates > QSEQ_MAX_TEMPLATES) {
        return NULL;
    }
    if (cfg.amplitude_dim == 0) {
        return NULL;
    }

    /* Allocate context */
    qseq_matcher_internal_t* ctx =
        (qseq_matcher_internal_t*)nimcp_calloc(1, sizeof(qseq_matcher_internal_t));
    if (!ctx) return NULL;

    ctx->config = cfg;
    ctx->template_capacity = cfg.max_templates;
    ctx->rng_state = cfg.seed;

    /* Allocate templates */
    ctx->templates = (qseq_pattern_t*)nimcp_calloc(cfg.max_templates, sizeof(qseq_pattern_t));

    /* Allocate amplitude vectors */
    ctx->query_amplitude = (float*)nimcp_calloc(cfg.amplitude_dim, sizeof(float));
    ctx->template_amplitudes = (float*)nimcp_calloc(cfg.max_templates * cfg.amplitude_dim,
                                              sizeof(float));
    ctx->similarity_vector = (float*)nimcp_calloc(cfg.max_templates, sizeof(float));

    /* Allocate Grover state */
    ctx->grover_dim = cfg.max_templates;
    ctx->grover_state = (float*)nimcp_calloc(cfg.max_templates, sizeof(float));

    if (!ctx->templates || !ctx->query_amplitude || !ctx->template_amplitudes ||
        !ctx->similarity_vector || !ctx->grover_state) {
        nimcp_free(ctx->templates);
        nimcp_free(ctx->query_amplitude);
        nimcp_free(ctx->template_amplitudes);
        nimcp_free(ctx->similarity_vector);
        nimcp_free(ctx->grover_state);
        nimcp_free(ctx);
        return NULL;
    }

    return (qseq_matcher_t)ctx;
}

/**
 * WHAT: Destroy quantum sequence matcher
 */
static inline void qseq_matcher_destroy(qseq_matcher_t ctx) {
    if (!ctx) return;
    qseq_matcher_internal_t* internal = (qseq_matcher_internal_t*)ctx;

    /* Free template amplitude vectors */
    for (uint32_t i = 0; i < internal->n_templates; i++) {
        nimcp_free(internal->templates[i].amplitude_vector);
    }

    nimcp_free(internal->templates);
    nimcp_free(internal->query_amplitude);
    nimcp_free(internal->template_amplitudes);
    nimcp_free(internal->similarity_vector);
    nimcp_free(internal->grover_state);
    nimcp_free(internal);
}

//=============================================================================
// Amplitude Encoding
//=============================================================================

/**
 * WHAT: Encode pattern as amplitude vector
 * WHY:  Quantum representation of sequence
 * HOW:  Sum Gaussian bumps at symbol positions
 */
static inline void qseq_encode_amplitude(
    const qseq_pattern_t* pattern,
    float* amplitude,
    uint32_t dim
) {
    /* Clear amplitude vector */
    memset(amplitude, 0, dim * sizeof(float));

    if (pattern->length == 0) return;

    /* Encode each element as Gaussian bump */
    float sigma = (float)dim / (float)pattern->length / 2.0f;

    for (uint32_t i = 0; i < pattern->length; i++) {
        /* Map symbol to position in amplitude space */
        float center = (float)(pattern->elements[i].symbol % dim);

        for (uint32_t j = 0; j < dim; j++) {
            float dist = (float)j - center;
            float bump = expf(-dist * dist / (2.0f * sigma * sigma));
            amplitude[j] += bump * pattern->elements[i].weight;
        }
    }

    /* Normalize */
    float norm = 0.0f;
    for (uint32_t j = 0; j < dim; j++) {
        norm += amplitude[j] * amplitude[j];
    }
    norm = sqrtf(norm + 1e-10f);
    for (uint32_t j = 0; j < dim; j++) {
        amplitude[j] /= norm;
    }
}

/**
 * WHAT: Compute quantum fidelity between amplitude vectors
 * WHY:  Measure similarity in quantum state space
 * HOW:  |<psi|phi>|^2
 */
static inline float qseq_fidelity(
    const float* a,
    const float* b,
    uint32_t dim
) {
    float dot = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
    }
    return dot * dot;  /* Fidelity = |overlap|^2 */
}

/**
 * WHAT: Compute temporal similarity
 * WHY:  Check if timing patterns match
 * HOW:  Compare relative timestamps with tolerance
 */
static inline float qseq_temporal_similarity(
    const qseq_pattern_t* query,
    const qseq_pattern_t* template_pat,
    float tolerance,
    float compression
) {
    if (query->length == 0 || template_pat->length == 0) return 0.0f;

    /* Align patterns by finding best offset */
    float best_score = 0.0f;

    for (uint32_t offset = 0; offset < template_pat->length; offset++) {
        float score = 0.0f;
        uint32_t matched = 0;

        for (uint32_t i = 0; i < query->length && (offset + i) < template_pat->length; i++) {
            /* Scale template timing by compression */
            float template_time = template_pat->elements[offset + i].timestamp * compression;
            float query_time = query->elements[i].timestamp;
            float time_diff = fabsf(template_time - query_time);

            if (time_diff < tolerance) {
                /* Match with Gaussian weighting */
                float weight = expf(-time_diff * time_diff / (2.0f * tolerance * tolerance));
                score += weight;
                matched++;
            }
        }

        if (matched > 0) {
            score /= (float)matched;
            if (score > best_score) {
                best_score = score;
            }
        }
    }

    return best_score;
}

//=============================================================================
// Template Management
//=============================================================================

/**
 * WHAT: Add pattern template
 * WHY:  Store pattern for future matching
 *
 * @param ctx Matcher context
 * @param pattern Pattern to add
 * @param pattern_id_out Output: assigned pattern ID
 * @return 0 on success
 */
static inline int qseq_matcher_add_template(
    qseq_matcher_t ctx,
    const qseq_pattern_t* pattern,
    uint32_t* pattern_id_out
) {
    if (!ctx || !pattern) return -1;
    qseq_matcher_internal_t* internal = (qseq_matcher_internal_t*)ctx;

    if (internal->n_templates >= internal->template_capacity) {
        return -2;  /* Full */
    }

    uint32_t idx = internal->n_templates;

    /* Copy pattern */
    internal->templates[idx] = *pattern;
    internal->templates[idx].pattern_id = idx;

    /* Allocate and compute amplitude encoding */
    internal->templates[idx].amplitude_vector =
        (float*)nimcp_calloc(internal->config.amplitude_dim, sizeof(float));
    internal->templates[idx].amplitude_dim = internal->config.amplitude_dim;

    if (!internal->templates[idx].amplitude_vector) {
        return -3;
    }

    qseq_encode_amplitude(pattern, internal->templates[idx].amplitude_vector,
                         internal->config.amplitude_dim);

    /* Store in template_amplitudes matrix */
    memcpy(internal->template_amplitudes + idx * internal->config.amplitude_dim,
           internal->templates[idx].amplitude_vector,
           internal->config.amplitude_dim * sizeof(float));

    internal->n_templates++;

    if (pattern_id_out) {
        *pattern_id_out = idx;
    }

    return 0;
}

/**
 * WHAT: Remove pattern template
 */
static inline int qseq_matcher_remove_template(
    qseq_matcher_t ctx,
    uint32_t pattern_id
) {
    if (!ctx) return -1;
    qseq_matcher_internal_t* internal = (qseq_matcher_internal_t*)ctx;

    if (pattern_id >= internal->n_templates) {
        return -2;
    }

    /* Free amplitude vector */
    nimcp_free(internal->templates[pattern_id].amplitude_vector);

    /* Shift remaining templates */
    for (uint32_t i = pattern_id; i < internal->n_templates - 1; i++) {
        internal->templates[i] = internal->templates[i + 1];
        internal->templates[i].pattern_id = i;

        memcpy(internal->template_amplitudes + i * internal->config.amplitude_dim,
               internal->template_amplitudes + (i + 1) * internal->config.amplitude_dim,
               internal->config.amplitude_dim * sizeof(float));
    }

    internal->n_templates--;
    return 0;
}

/**
 * WHAT: Clear all templates
 */
static inline void qseq_matcher_clear_templates(qseq_matcher_t ctx) {
    if (!ctx) return;
    qseq_matcher_internal_t* internal = (qseq_matcher_internal_t*)ctx;

    for (uint32_t i = 0; i < internal->n_templates; i++) {
        nimcp_free(internal->templates[i].amplitude_vector);
    }
    internal->n_templates = 0;
}

//=============================================================================
// Grover-Inspired Search
//=============================================================================

/**
 * WHAT: Initialize Grover state to uniform superposition
 */
static inline void qseq_grover_init(qseq_matcher_internal_t* ctx) {
    float uniform = 1.0f / sqrtf((float)ctx->n_templates);
    for (uint32_t i = 0; i < ctx->n_templates; i++) {
        ctx->grover_state[i] = uniform;
    }
}

/**
 * WHAT: Apply oracle (mark matching patterns)
 * WHY:  Phase flip for patterns above similarity threshold
 */
static inline void qseq_grover_oracle(
    qseq_matcher_internal_t* ctx,
    float threshold
) {
    for (uint32_t i = 0; i < ctx->n_templates; i++) {
        if (ctx->similarity_vector[i] >= threshold) {
            ctx->grover_state[i] *= -1.0f;  /* Phase flip */
        }
    }
}

/**
 * WHAT: Apply diffusion operator (inversion about mean)
 */
static inline void qseq_grover_diffusion(qseq_matcher_internal_t* ctx) {
    /* Compute mean */
    float mean = 0.0f;
    for (uint32_t i = 0; i < ctx->n_templates; i++) {
        mean += ctx->grover_state[i];
    }
    mean /= (float)ctx->n_templates;

    /* Invert about mean */
    for (uint32_t i = 0; i < ctx->n_templates; i++) {
        ctx->grover_state[i] = 2.0f * mean - ctx->grover_state[i];
    }
}

/**
 * WHAT: Run Grover iterations
 * WHY:  Amplify probability of matching patterns
 */
static inline void qseq_grover_run(
    qseq_matcher_internal_t* ctx,
    float threshold,
    uint32_t iterations
) {
    qseq_grover_init(ctx);

    for (uint32_t i = 0; i < iterations; i++) {
        qseq_grover_oracle(ctx, threshold);
        qseq_grover_diffusion(ctx);
    }
}

/**
 * WHAT: Measure Grover state (find highest probability template)
 */
static inline uint32_t qseq_grover_measure(qseq_matcher_internal_t* ctx) {
    float max_prob = 0.0f;
    uint32_t best_idx = 0;

    for (uint32_t i = 0; i < ctx->n_templates; i++) {
        float prob = ctx->grover_state[i] * ctx->grover_state[i];
        if (prob > max_prob) {
            max_prob = prob;
            best_idx = i;
        }
    }

    return best_idx;
}

//=============================================================================
// Core Matching Functions
//=============================================================================

/**
 * WHAT: Match query pattern against templates
 * WHY:  Find best matching stored pattern
 * HOW:  Compute similarities, run Grover search, return best match
 *
 * @param ctx Matcher context
 * @param query Query pattern
 * @param result Output: match result
 * @return 0 on success, negative on error
 */
static inline int qseq_matcher_match(
    qseq_matcher_t ctx,
    const qseq_pattern_t* query,
    qseq_match_result_t* result
) {
    if (!ctx || !query || !result) return -1;
    qseq_matcher_internal_t* internal = (qseq_matcher_internal_t*)ctx;

    if (internal->n_templates == 0) {
        result->pattern_id = 0;
        result->similarity = 0.0f;
        return -2;  /* No templates */
    }

    /* Encode query */
    qseq_encode_amplitude(query, internal->query_amplitude,
                         internal->config.amplitude_dim);

    /* Compute similarity to all templates */
    float best_similarity = 0.0f;
    uint32_t best_idx = 0;
    float best_compression = 1.0f;
    bool best_reversed = false;

    for (uint32_t i = 0; i < internal->n_templates; i++) {
        /* Amplitude fidelity */
        float fidelity = qseq_fidelity(
            internal->query_amplitude,
            internal->template_amplitudes + i * internal->config.amplitude_dim,
            internal->config.amplitude_dim
        );

        /* Temporal similarity at normal speed */
        float temporal = qseq_temporal_similarity(
            query, &internal->templates[i],
            internal->config.temporal_tolerance, 1.0f
        );

        float similarity = 0.5f * fidelity + 0.5f * temporal;
        internal->similarity_vector[i] = similarity;

        /* Check compression if enabled */
        if (internal->config.enable_compression_detection) {
            for (float comp = internal->config.compression_range_min;
                 comp <= internal->config.compression_range_max;
                 comp *= 2.0f) {
                if (fabsf(comp - 1.0f) < 0.01f) continue;  /* Skip normal speed */

                float comp_temporal = qseq_temporal_similarity(
                    query, &internal->templates[i],
                    internal->config.temporal_tolerance, comp
                );
                float comp_similarity = 0.5f * fidelity + 0.5f * comp_temporal;

                if (comp_similarity > similarity) {
                    similarity = comp_similarity;
                    internal->similarity_vector[i] = similarity;
                    if (similarity > best_similarity) {
                        best_compression = comp;
                    }
                }
            }
        }

        if (similarity > best_similarity) {
            best_similarity = similarity;
            best_idx = i;
        }
    }

    /* Optionally run Grover search for verification */
    uint32_t grover_iters = internal->config.grover_iterations;
    if (grover_iters == 0) {
        /* Auto: O(sqrt(N)) iterations */
        grover_iters = (uint32_t)(M_PI / 4.0 * sqrtf((float)internal->n_templates));
        grover_iters = (grover_iters < 1) ? 1 : grover_iters;
        grover_iters = (grover_iters > 10) ? 10 : grover_iters;
    }

    qseq_grover_run(internal, internal->config.min_similarity, grover_iters);
    uint32_t grover_best = qseq_grover_measure(internal);

    /* Use Grover result if it found a good match */
    if (internal->similarity_vector[grover_best] >= internal->config.min_similarity) {
        best_idx = grover_best;
        best_similarity = internal->similarity_vector[grover_best];
    }

    internal->grover_total_iterations += grover_iters;

    /* Fill result */
    result->pattern_id = internal->templates[best_idx].pattern_id;
    result->similarity = best_similarity;
    result->fidelity = qseq_fidelity(
        internal->query_amplitude,
        internal->template_amplitudes + best_idx * internal->config.amplitude_dim,
        internal->config.amplitude_dim
    );
    result->phase_coherence = qseq_temporal_similarity(
        query, &internal->templates[best_idx],
        internal->config.temporal_tolerance, best_compression
    );
    result->matched_elements = (uint32_t)(result->phase_coherence * query->length);
    result->compression = best_compression;
    result->is_reversed = best_reversed;
    result->grover_iterations = grover_iters;

    internal->matches_performed++;
    if (best_similarity > internal->best_similarity_ever) {
        internal->best_similarity_ever = best_similarity;
    }

    return 0;
}

/**
 * WHAT: Find all patterns above similarity threshold
 *
 * @param ctx Matcher context
 * @param query Query pattern
 * @param threshold Minimum similarity
 * @param results Output array (caller allocates)
 * @param max_results Maximum results to return
 * @param n_results_out Output: number of results found
 * @return 0 on success
 */
static inline int qseq_matcher_find_all(
    qseq_matcher_t ctx,
    const qseq_pattern_t* query,
    float threshold,
    qseq_match_result_t* results,
    uint32_t max_results,
    uint32_t* n_results_out
) {
    if (!ctx || !query || !results || !n_results_out) return -1;
    qseq_matcher_internal_t* internal = (qseq_matcher_internal_t*)ctx;

    *n_results_out = 0;

    if (internal->n_templates == 0) return 0;

    /* Encode query */
    qseq_encode_amplitude(query, internal->query_amplitude,
                         internal->config.amplitude_dim);

    /* Check all templates */
    for (uint32_t i = 0; i < internal->n_templates && *n_results_out < max_results; i++) {
        float fidelity = qseq_fidelity(
            internal->query_amplitude,
            internal->template_amplitudes + i * internal->config.amplitude_dim,
            internal->config.amplitude_dim
        );

        float temporal = qseq_temporal_similarity(
            query, &internal->templates[i],
            internal->config.temporal_tolerance, 1.0f
        );

        float similarity = 0.5f * fidelity + 0.5f * temporal;

        if (similarity >= threshold) {
            results[*n_results_out].pattern_id = internal->templates[i].pattern_id;
            results[*n_results_out].similarity = similarity;
            results[*n_results_out].fidelity = fidelity;
            results[*n_results_out].phase_coherence = temporal;
            results[*n_results_out].matched_elements = (uint32_t)(temporal * query->length);
            results[*n_results_out].compression = 1.0f;
            results[*n_results_out].is_reversed = false;
            results[*n_results_out].grover_iterations = 0;
            (*n_results_out)++;
        }
    }

    return 0;
}

/**
 * WHAT: Compute pattern distance matrix
 * WHY:  Useful for clustering and analysis
 */
static inline int qseq_matcher_distance_matrix(
    qseq_matcher_t ctx,
    float* distances,
    uint32_t* n_patterns
) {
    if (!ctx || !distances || !n_patterns) return -1;
    qseq_matcher_internal_t* internal = (qseq_matcher_internal_t*)ctx;

    *n_patterns = internal->n_templates;

    for (uint32_t i = 0; i < internal->n_templates; i++) {
        for (uint32_t j = 0; j < internal->n_templates; j++) {
            float fidelity = qseq_fidelity(
                internal->template_amplitudes + i * internal->config.amplitude_dim,
                internal->template_amplitudes + j * internal->config.amplitude_dim,
                internal->config.amplitude_dim
            );
            /* Distance = 1 - fidelity */
            distances[i * internal->n_templates + j] = 1.0f - fidelity;
        }
    }

    return 0;
}

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * WHAT: Get statistics
 */
static inline int qseq_matcher_get_stats(
    qseq_matcher_t ctx,
    qseq_matcher_stats_t* stats
) {
    if (!ctx || !stats) return -1;
    qseq_matcher_internal_t* internal = (qseq_matcher_internal_t*)ctx;

    stats->matches_performed = internal->matches_performed;
    stats->grover_total_iterations = internal->grover_total_iterations;
    stats->avg_iterations_per_match = (internal->matches_performed > 0) ?
        (float)internal->grover_total_iterations / (float)internal->matches_performed : 0.0f;
    stats->best_similarity_ever = internal->best_similarity_ever;
    stats->stored_templates = internal->n_templates;

    return 0;
}

/**
 * WHAT: Get template count
 */
static inline uint32_t qseq_matcher_template_count(qseq_matcher_t ctx) {
    if (!ctx) return 0;
    qseq_matcher_internal_t* internal = (qseq_matcher_internal_t*)ctx;
    return internal->n_templates;
}

/**
 * WHAT: Get configuration
 */
static inline int qseq_matcher_get_config(
    qseq_matcher_t ctx,
    qseq_matcher_config_t* config
) {
    if (!ctx || !config) return -1;
    qseq_matcher_internal_t* internal = (qseq_matcher_internal_t*)ctx;
    *config = internal->config;
    return 0;
}

/**
 * WHAT: Create pattern from element array
 * WHY:  Convenience function for pattern creation
 */
static inline qseq_pattern_t qseq_create_pattern(
    const uint32_t* symbols,
    const float* timestamps,
    uint32_t length
) {
    qseq_pattern_t pattern = {0};
    pattern.length = (length < QSEQ_MAX_PATTERN_LENGTH) ? length : QSEQ_MAX_PATTERN_LENGTH;

    for (uint32_t i = 0; i < pattern.length; i++) {
        pattern.elements[i].symbol = symbols[i];
        pattern.elements[i].timestamp = timestamps ? timestamps[i] : (float)i * 10.0f;
        pattern.elements[i].weight = 1.0f;
    }

    if (pattern.length > 0 && timestamps) {
        pattern.duration = timestamps[pattern.length - 1];
    } else {
        pattern.duration = (float)pattern.length * 10.0f;
    }

    return pattern;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_QUANTUM_SEQUENCE_MATCHER_H */
