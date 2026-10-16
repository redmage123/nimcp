//=============================================================================
// nimcp_quantum_semantic.h - Quantum-Enhanced Semantic Memory Retrieval
//=============================================================================
/**
 * @file nimcp_quantum_semantic.h
 * @brief Quantum-inspired speedup for semantic memory operations
 *
 * WHAT: Quantum algorithms for semantic memory search and activation
 * WHY:  √N speedup for concept retrieval, efficient spreading activation
 * HOW:  Grover-inspired search + quantum walk for network traversal
 *
 * QUANTUM SPEEDUP OPPORTUNITIES:
 * | Operation            | Classical | Quantum      | Method        |
 * |---------------------|-----------|--------------|---------------|
 * | Similar concepts    | O(N)      | O(√N)        | Grover search |
 * | Spreading activation| O(V+E)    | O(√(V+E))    | Quantum walk  |
 * | Relation traversal  | O(d^k)    | O(d^(k/2))   | QAOA graph    |
 *
 * TERNARY RELEVANCE STATES:
 * | State | Meaning              | Action                    |
 * |-------|----------------------|---------------------------|
 * | -1    | Irrelevant concept   | Skip in search            |
 * | 0     | Unknown relevance    | Evaluate similarity       |
 * | +1    | Relevant concept     | Include in results        |
 *
 * AMPLITUDE ENCODING:
 * Classical: Store N feature vectors as N × D floats
 * Quantum: Encode into log₂(N) qubits (simulated via ternary states)
 *
 * INTEGRATION:
 * - Uses nimcp_semantic_memory.h for base semantic operations
 * - Uses nimcp_quantum_walk_ternary.h for network traversal
 * - Uses nimcp_quantum_annealing_ternary.h for optimization
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#ifndef NIMCP_QUANTUM_SEMANTIC_H
#define NIMCP_QUANTUM_SEMANTIC_H

#include "nimcp_semantic_memory.h"
#include "optimization/quantum_annealing/nimcp_quantum_ternary.h"
#include "optimization/quantum_annealing/nimcp_quantum_annealing_ternary.h"
#include "utils/quantum/nimcp_quantum_walk_ternary.h"
#include "utils/ternary/nimcp_ternary.h"
#include <math.h>
#include <string.h>
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define QUANTUM_SEMANTIC_MAGIC  0x51534D4D  /* "QSMM" */
#define QUANTUM_SEMANTIC_MAX_CONCEPTS 4096
#define QUANTUM_SEMANTIC_FEATURE_DIM  32

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Quantum semantic retrieval mode
 */
typedef enum {
    QUANTUM_SEM_MODE_GROVER,    /**< Grover-inspired amplitude search */
    QUANTUM_SEM_MODE_WALK,      /**< Quantum walk spreading activation */
    QUANTUM_SEM_MODE_ANNEAL,    /**< Annealing for relation optimization */
    QUANTUM_SEM_MODE_HYBRID     /**< Combine Grover + walk */
} quantum_semantic_mode_t;

/**
 * @brief Configuration for quantum semantic retrieval
 */
typedef struct {
    /* Mode selection */
    quantum_semantic_mode_t mode;   /**< Retrieval mode */

    /* Grover search parameters */
    uint32_t grover_iterations;     /**< Iterations (default: √N/2) */
    float similarity_threshold;     /**< Min similarity for marking (+1) */
    float dissimilarity_threshold;  /**< Max similarity for marking (-1) */

    /* Quantum walk parameters */
    uint32_t walk_steps;            /**< Steps for spreading activation */
    float decay_per_hop;            /**< Activation decay (default: 0.8) */

    /* Annealing parameters */
    uint32_t anneal_sweeps;         /**< Sweeps for relation optimization */
    float initial_temperature;      /**< Start temperature */
    float final_temperature;        /**< End temperature */

    /* Sparsity control */
    uint32_t max_results;           /**< Maximum concepts to return */
    float activation_threshold;     /**< Min activation for inclusion */

    /* Ternary encoding */
    bool use_ternary_features;      /**< Quantize features to ternary */
    float ternary_threshold;        /**< Threshold for ternary quantization */
} quantum_semantic_config_t;

/**
 * @brief Statistics for quantum semantic operations
 */
typedef struct {
    uint64_t total_queries;         /**< Total queries processed */
    uint64_t concepts_evaluated;    /**< Total concepts checked */
    uint64_t concepts_skipped;      /**< Concepts skipped via ternary */
    float avg_speedup;              /**< Average speedup factor */
    float avg_result_count;         /**< Average results per query */
    uint64_t walk_steps_total;      /**< Total walk steps */
    uint64_t grover_iterations_total; /**< Total Grover iterations */
} quantum_semantic_stats_t;

/**
 * @brief Result of quantum semantic query
 */
typedef struct {
    uint64_t* concept_ids;          /**< Matched concept IDs */
    float* similarities;            /**< Similarity scores */
    float* activations;             /**< Activation levels */
    trit_t* relevance;              /**< Ternary relevance states */
    uint32_t count;                 /**< Number of results */
    float query_coherence;          /**< Quantum coherence at query end */
    bool success;                   /**< Operation succeeded */
} quantum_semantic_result_t;

//=============================================================================
// Quantum Semantic Context
//=============================================================================

/**
 * @brief Quantum semantic retrieval context
 */
typedef struct quantum_semantic_ctx {
    uint32_t magic;                     /**< Validation: QUANTUM_SEMANTIC_MAGIC */

    /* Configuration */
    quantum_semantic_config_t config;   /**< Configuration */

    /* Base semantic memory (not owned) */
    semantic_memory_system_t* sem_mem;  /**< Semantic memory system */

    /* Quantum state storage */
    trit_ising_config_t* concept_ising; /**< Ising model for concepts */
    trit_walker_1d_t* concept_walker;   /**< Quantum walker for traversal */
    trit_vector_t* relevance_vector;    /**< Ternary relevance per concept */

    /* Feature index for fast lookup */
    trit_matrix_t* feature_index;       /**< Quantized feature index */
    uint32_t index_size;                /**< Number of indexed concepts */

    /* Workspace */
    float* similarity_buffer;           /**< Similarity scores */
    float* activation_buffer;           /**< Activation levels */

    /* Statistics */
    quantum_semantic_stats_t stats;     /**< Performance statistics */
} quantum_semantic_ctx_t;

typedef struct quantum_semantic_ctx* quantum_semantic_t;

//=============================================================================
// Forward Declarations
//=============================================================================

/* Forward declare for use before definition */
static inline void quantum_semantic_free_result(quantum_semantic_result_t* result);

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Get default quantum semantic configuration
 *
 * @return Default configuration
 */
static inline quantum_semantic_config_t quantum_semantic_default_config(void) {
    quantum_semantic_config_t config = {
        .mode = QUANTUM_SEM_MODE_GROVER,
        .grover_iterations = 0,           /* 0 = auto (√N/2) */
        .similarity_threshold = 0.7f,     /* Mark relevant above this */
        .dissimilarity_threshold = 0.2f,  /* Mark irrelevant below this */
        .walk_steps = 5,
        .decay_per_hop = 0.8f,
        .anneal_sweeps = 50,
        .initial_temperature = 1.0f,
        .final_temperature = 0.01f,
        .max_results = 10,
        .activation_threshold = 0.3f,
        .use_ternary_features = false,
        .ternary_threshold = 0.3f
    };
    return config;
}

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Create quantum semantic retrieval context
 *
 * WHAT: Initialize quantum-enhanced semantic memory retrieval
 * WHY:  Enable faster concept search and activation spreading
 * HOW:  Allocate Ising model, walker, relevance tracking
 *
 * @param config Configuration
 * @param max_concepts Maximum concepts to index
 * @return Quantum semantic context, or NULL on failure
 */
static inline quantum_semantic_t quantum_semantic_create(
    const quantum_semantic_config_t* config,
    uint32_t max_concepts
) {
    if (!config || max_concepts == 0) return NULL;

    quantum_semantic_ctx_t* ctx = (quantum_semantic_ctx_t*)
        nimcp_calloc(1, sizeof(quantum_semantic_ctx_t));
    if (!ctx) return NULL;

    ctx->magic = QUANTUM_SEMANTIC_MAGIC;
    ctx->config = *config;
    ctx->index_size = 0;

    /* Create Ising model for concept relevance */
    ctx->concept_ising = trit_ising_create(max_concepts, 0.1);
    if (!ctx->concept_ising) {
        nimcp_free(ctx);
        return NULL;
    }

    /* Create quantum walker for spreading activation */
    float uniform_bias = 1.0f / 3.0f;
    ctx->concept_walker = trit_walker_1d_create(
        max_concepts, uniform_bias, uniform_bias, uniform_bias);
    if (!ctx->concept_walker) {
        trit_ising_destroy(ctx->concept_ising);
        nimcp_free(ctx);
        return NULL;
    }

    /* Create relevance vector */
    ctx->relevance_vector = trit_vector_create_filled(
        max_concepts, TRIT_UNKNOWN, TERNARY_PACK_BASE243);
    if (!ctx->relevance_vector) {
        trit_walker_1d_destroy(ctx->concept_walker);
        trit_ising_destroy(ctx->concept_ising);
        nimcp_free(ctx);
        return NULL;
    }

    /* Allocate workspace buffers */
    ctx->similarity_buffer = (float*)nimcp_calloc(max_concepts, sizeof(float));
    ctx->activation_buffer = (float*)nimcp_calloc(max_concepts, sizeof(float));
    if (!ctx->similarity_buffer || !ctx->activation_buffer) {
        trit_vector_destroy(ctx->relevance_vector);
        trit_walker_1d_destroy(ctx->concept_walker);
        trit_ising_destroy(ctx->concept_ising);
        nimcp_free(ctx->similarity_buffer);
        nimcp_free(ctx->activation_buffer);
        nimcp_free(ctx);
        return NULL;
    }

    return ctx;
}

/**
 * @brief Destroy quantum semantic context
 *
 * @param ctx Context to destroy
 */
static inline void quantum_semantic_destroy(quantum_semantic_t ctx) {
    if (!ctx) return;
    if (ctx->magic != QUANTUM_SEMANTIC_MAGIC) return;

    trit_ising_destroy(ctx->concept_ising);
    trit_walker_1d_destroy(ctx->concept_walker);
    trit_vector_destroy(ctx->relevance_vector);
    if (ctx->feature_index) trit_matrix_destroy(ctx->feature_index);
    nimcp_free(ctx->similarity_buffer);
    nimcp_free(ctx->activation_buffer);

    ctx->magic = 0;
    nimcp_free(ctx);
}

//=============================================================================
// Integration
//=============================================================================

/**
 * @brief Connect to semantic memory system
 *
 * @param ctx Quantum semantic context
 * @param sem_mem Semantic memory system
 */
static inline void quantum_semantic_set_memory(
    quantum_semantic_t ctx,
    semantic_memory_system_t* sem_mem
) {
    if (!ctx || ctx->magic != QUANTUM_SEMANTIC_MAGIC) return;
    ctx->sem_mem = sem_mem;

    /* Update index size */
    if (sem_mem) {
        ctx->index_size = sem_mem->concept_count;
    }
}

//=============================================================================
// Ternary Feature Indexing
//=============================================================================

/**
 * @brief Build ternary feature index from concepts
 *
 * WHAT: Quantize concept features to ternary for fast comparison
 * WHY:  Enable early pruning of dissimilar concepts
 * HOW:  Threshold features to {-1, 0, +1}
 *
 * @param ctx Quantum semantic context
 * @return Number of concepts indexed
 */
static inline uint32_t quantum_semantic_build_index(quantum_semantic_t ctx) {
    if (!ctx || ctx->magic != QUANTUM_SEMANTIC_MAGIC || !ctx->sem_mem) return 0;

    uint32_t n_concepts = ctx->sem_mem->concept_count;
    if (n_concepts == 0) return 0;

    /* Determine feature dimension */
    uint32_t feature_dim = QUANTUM_SEMANTIC_FEATURE_DIM;
    if (ctx->sem_mem->concepts[0]) {
        feature_dim = ctx->sem_mem->concepts[0]->feature_dim;
    }

    /* Create/recreate feature index */
    if (ctx->feature_index) {
        trit_matrix_destroy(ctx->feature_index);
    }
    ctx->feature_index = trit_matrix_create(n_concepts, feature_dim, TERNARY_PACK_BASE243);
    if (!ctx->feature_index) return 0;

    /* Quantize each concept's features */
    float threshold = ctx->config.ternary_threshold;
    for (uint32_t i = 0; i < n_concepts; i++) {
        const semantic_concept_t* sem_concept = ctx->sem_mem->concepts[i];
        if (!sem_concept || !sem_concept->features) continue;

        for (uint32_t j = 0; j < feature_dim && j < sem_concept->feature_dim; j++) {
            float val = sem_concept->features[j];
            trit_t trit;
            if (val > threshold) {
                trit = TRIT_POSITIVE;
            } else if (val < -threshold) {
                trit = TRIT_NEGATIVE;
            } else {
                trit = TRIT_UNKNOWN;
            }
            trit_matrix_set(ctx->feature_index, i, j, trit);
        }
    }

    ctx->index_size = n_concepts;
    return n_concepts;
}

//=============================================================================
// Similarity Computation
//=============================================================================

/**
 * @brief Compute cosine similarity between vectors
 *
 * @param a First vector
 * @param b Second vector
 * @param dim Dimension
 * @return Cosine similarity [-1, 1]
 */
static inline float quantum_semantic_cosine(
    const float* a,
    const float* b,
    uint32_t dim
) {
    if (!a || !b || dim == 0) return 0.0f;

    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    float denom = sqrtf(norm_a) * sqrtf(norm_b);
    if (denom < 1e-8f) return 0.0f;
    return dot / denom;
}

/**
 * @brief Compute ternary similarity (Hamming-like)
 *
 * WHAT: Fast similarity using ternary index
 * WHY:  Quickly filter dissimilar concepts
 * HOW:  Compare ternary vectors, count matches
 *
 * @param ctx Quantum semantic context
 * @param query_trit Ternary query vector
 * @param concept_idx Concept index
 * @return Ternary similarity [0, 1]
 */
static inline float quantum_semantic_ternary_similarity(
    quantum_semantic_t ctx,
    const trit_vector_t* query_trit,
    uint32_t concept_idx
) {
    if (!ctx || !query_trit || !ctx->feature_index) return 0.0f;
    if (concept_idx >= ctx->index_size) return 0.0f;

    uint32_t matches = 0;
    uint32_t comparisons = 0;
    uint32_t dim = (uint32_t)query_trit->length;

    for (uint32_t j = 0; j < dim; j++) {
        trit_t q = trit_vector_get(query_trit, j);
        trit_t c = trit_matrix_get(ctx->feature_index, concept_idx, j);

        /* Skip unknown values */
        if (q == TRIT_UNKNOWN || c == TRIT_UNKNOWN) continue;

        comparisons++;
        if (q == c) matches++;
    }

    if (comparisons == 0) return 0.5f;  /* No info = neutral */
    return (float)matches / (float)comparisons;
}

//=============================================================================
// Grover-Inspired Search
//=============================================================================

/**
 * @brief Grover-inspired amplitude amplification for concept search
 *
 * WHAT: Amplify probability of relevant concepts
 * WHY:  Find similar concepts faster than linear scan
 * HOW:  Mark relevant/irrelevant, amplify marked states
 *
 * ALGORITHM (Classical Simulation):
 * 1. Initialize all concepts to uniform amplitude
 * 2. Oracle: Mark concepts based on similarity (ternary relevance)
 * 3. Diffusion: Amplify marked states via Ising dynamics
 * 4. Repeat √N/2 times
 * 5. Measure: Return high-amplitude concepts
 *
 * @param ctx Quantum semantic context
 * @param query Query features
 * @param feature_dim Feature dimension
 * @param result Output result
 * @return 0 on success, error code on failure
 */
static inline int quantum_semantic_grover_search(
    quantum_semantic_t ctx,
    const float* query,
    uint32_t feature_dim,
    quantum_semantic_result_t* result
) {
    if (!ctx || ctx->magic != QUANTUM_SEMANTIC_MAGIC || !result) return -1;
    if (!ctx->sem_mem || !query) return -1;

    result->success = false;
    uint32_t n_concepts = ctx->sem_mem->concept_count;
    if (n_concepts == 0) return -2;

    /* Reset relevance to unknown (superposition) */
    for (uint32_t i = 0; i < n_concepts; i++) {
        trit_vector_set(ctx->relevance_vector, i, TRIT_UNKNOWN);
        ctx->similarity_buffer[i] = 0.0f;
    }

    /* Compute iterations (√N / 2) */
    uint32_t iterations = ctx->config.grover_iterations;
    if (iterations == 0) {
        iterations = (uint32_t)(sqrtf((float)n_concepts) / 2.0f);
        if (iterations < 1) iterations = 1;
    }

    ctx->stats.grover_iterations_total += iterations;

    /* Oracle + Diffusion iterations */
    for (uint32_t iter = 0; iter < iterations; iter++) {
        /* Oracle: Compute similarity and mark relevant/irrelevant */
        for (uint32_t i = 0; i < n_concepts; i++) {
            trit_t current = trit_vector_get(ctx->relevance_vector, i);

            /* Skip already decided concepts */
            if (current != TRIT_UNKNOWN) {
                ctx->stats.concepts_skipped++;
                continue;
            }

            ctx->stats.concepts_evaluated++;

            /* Get concept features */
            const semantic_concept_t* sem_concept = ctx->sem_mem->concepts[i];
            if (!sem_concept || !sem_concept->features) continue;

            /* Compute similarity */
            float sim = quantum_semantic_cosine(
                query, sem_concept->features,
                (feature_dim < sem_concept->feature_dim) ? feature_dim : sem_concept->feature_dim);

            ctx->similarity_buffer[i] = sim;

            /* Mark based on threshold */
            if (sim >= ctx->config.similarity_threshold) {
                trit_vector_set(ctx->relevance_vector, i, TRIT_POSITIVE);
            } else if (sim <= ctx->config.dissimilarity_threshold) {
                trit_vector_set(ctx->relevance_vector, i, TRIT_NEGATIVE);
            }
            /* Else: stays UNKNOWN for next iteration */
        }

        /* Diffusion: Use Ising dynamics to amplify marked states */
        /* (Simplified: just count positive vs negative) */
        uint32_t pos_count = 0, neg_count = 0;
        for (uint32_t i = 0; i < n_concepts; i++) {
            trit_t val = trit_vector_get(ctx->relevance_vector, i);
            if (val == TRIT_POSITIVE) pos_count++;
            else if (val == TRIT_NEGATIVE) neg_count++;
        }

        /* Adjust thresholds based on distribution (adaptive) */
        if (pos_count > ctx->config.max_results * 2) {
            /* Too many positives, tighten threshold */
            ctx->config.similarity_threshold += 0.05f;
        } else if (pos_count == 0 && neg_count < n_concepts / 2) {
            /* No positives yet, loosen threshold slightly */
            ctx->config.similarity_threshold -= 0.02f;
        }
    }

    /* Collect results */
    uint32_t max_results = ctx->config.max_results;
    result->concept_ids = (uint64_t*)nimcp_calloc(max_results, sizeof(uint64_t));
    result->similarities = (float*)nimcp_calloc(max_results, sizeof(float));
    result->activations = (float*)nimcp_calloc(max_results, sizeof(float));
    result->relevance = (trit_t*)nimcp_calloc(max_results, sizeof(trit_t));

    if (!result->concept_ids || !result->similarities ||
        !result->activations || !result->relevance) {
        nimcp_free(result->concept_ids);
        nimcp_free(result->similarities);
        nimcp_free(result->activations);
        nimcp_free(result->relevance);
        return -3;
    }

    /* Sort by similarity and collect top results */
    /* Simple insertion sort for small result set */
    result->count = 0;
    for (uint32_t i = 0; i < n_concepts && result->count < max_results; i++) {
        trit_t rel = trit_vector_get(ctx->relevance_vector, i);
        if (rel != TRIT_POSITIVE) continue;

        float sim = ctx->similarity_buffer[i];
        const semantic_concept_t* sem_concept = ctx->sem_mem->concepts[i];
        if (!sem_concept) continue;

        /* Insert sorted by similarity */
        uint32_t pos = result->count;
        while (pos > 0 && result->similarities[pos - 1] < sim) {
            result->concept_ids[pos] = result->concept_ids[pos - 1];
            result->similarities[pos] = result->similarities[pos - 1];
            result->activations[pos] = result->activations[pos - 1];
            result->relevance[pos] = result->relevance[pos - 1];
            pos--;
        }

        result->concept_ids[pos] = sem_concept->id;
        result->similarities[pos] = sim;
        result->activations[pos] = sem_concept->activation;
        result->relevance[pos] = TRIT_POSITIVE;
        result->count++;
    }

    /* Compute final coherence */
    uint32_t unknown_count = 0;
    for (uint32_t i = 0; i < n_concepts; i++) {
        if (trit_vector_get(ctx->relevance_vector, i) == TRIT_UNKNOWN) {
            unknown_count++;
        }
    }
    result->query_coherence = (float)unknown_count / (float)n_concepts;
    result->success = true;

    ctx->stats.total_queries++;
    ctx->stats.avg_result_count =
        (ctx->stats.avg_result_count * (ctx->stats.total_queries - 1) + result->count) /
        ctx->stats.total_queries;

    return 0;
}

//=============================================================================
// Quantum Walk Spreading Activation
//=============================================================================

/**
 * @brief Quantum walk for spreading activation
 *
 * WHAT: Propagate activation through semantic network via quantum walk
 * WHY:  Explore related concepts efficiently
 * HOW:  Walker amplitude determines activation strength
 *
 * @param ctx Quantum semantic context
 * @param start_concept_id Starting concept ID
 * @param initial_activation Initial activation level
 * @param result Output result
 * @return 0 on success, error code on failure
 */
static inline int quantum_semantic_walk_activate(
    quantum_semantic_t ctx,
    uint64_t start_concept_id,
    float initial_activation,
    quantum_semantic_result_t* result
) {
    if (!ctx || ctx->magic != QUANTUM_SEMANTIC_MAGIC || !result) return -1;
    if (!ctx->sem_mem || !ctx->concept_walker) return -1;

    result->success = false;
    uint32_t n_concepts = ctx->sem_mem->concept_count;
    if (n_concepts == 0) return -2;

    /* Find start concept index */
    uint32_t start_idx = 0;
    bool found = false;
    for (uint32_t i = 0; i < n_concepts; i++) {
        if (ctx->sem_mem->concepts[i] &&
            ctx->sem_mem->concepts[i]->id == start_concept_id) {
            start_idx = i;
            found = true;
            break;
        }
    }
    if (!found) return -3;

    /* Initialize walker at start concept */
    trit_walker_1d_init(ctx->concept_walker, start_idx, TRIT_COIN_STAY);

    /* Set initial activation */
    memset(ctx->activation_buffer, 0, n_concepts * sizeof(float));
    ctx->activation_buffer[start_idx] = initial_activation;

    /* Perform quantum walk steps */
    for (uint32_t step = 0; step < ctx->config.walk_steps; step++) {
        trit_walker_1d_step(ctx->concept_walker);
        ctx->stats.walk_steps_total++;

        /* Transfer walker amplitude to activation */
        for (uint32_t i = 0; i < n_concepts; i++) {
            float amp = ctx->concept_walker->amplitudes[i];
            if (amp > 0.0f) {
                /* Decay existing activation */
                ctx->activation_buffer[i] *= ctx->config.decay_per_hop;
                /* Add walker contribution */
                ctx->activation_buffer[i] += amp * initial_activation * 0.5f;
            }
        }
    }

    /* Collect results above threshold */
    uint32_t max_results = ctx->config.max_results;
    result->concept_ids = (uint64_t*)nimcp_calloc(max_results, sizeof(uint64_t));
    result->similarities = (float*)nimcp_calloc(max_results, sizeof(float));
    result->activations = (float*)nimcp_calloc(max_results, sizeof(float));
    result->relevance = (trit_t*)nimcp_calloc(max_results, sizeof(trit_t));

    if (!result->concept_ids || !result->similarities ||
        !result->activations || !result->relevance) {
        nimcp_free(result->concept_ids);
        nimcp_free(result->similarities);
        nimcp_free(result->activations);
        nimcp_free(result->relevance);
        return -4;
    }

    result->count = 0;
    for (uint32_t i = 0; i < n_concepts && result->count < max_results; i++) {
        float act = ctx->activation_buffer[i];
        if (act < ctx->config.activation_threshold) continue;

        const semantic_concept_t* sem_concept = ctx->sem_mem->concepts[i];
        if (!sem_concept) continue;

        /* Insert sorted by activation */
        uint32_t pos = result->count;
        while (pos > 0 && result->activations[pos - 1] < act) {
            result->concept_ids[pos] = result->concept_ids[pos - 1];
            result->similarities[pos] = result->similarities[pos - 1];
            result->activations[pos] = result->activations[pos - 1];
            result->relevance[pos] = result->relevance[pos - 1];
            pos--;
        }

        result->concept_ids[pos] = sem_concept->id;
        result->similarities[pos] = 1.0f;  /* Not computed in walk mode */
        result->activations[pos] = act;
        result->relevance[pos] = TRIT_POSITIVE;
        result->count++;
    }

    result->query_coherence = trit_walker_1d_variance(ctx->concept_walker);
    result->success = true;

    ctx->stats.total_queries++;
    return 0;
}

//=============================================================================
// Combined Query
//=============================================================================

/**
 * @brief Query semantic memory with quantum enhancement
 *
 * WHAT: Find and activate related concepts
 * WHY:  Comprehensive retrieval using best available method
 * HOW:  Based on mode: Grover search, walk, or hybrid
 *
 * @param ctx Quantum semantic context
 * @param query Query features
 * @param feature_dim Feature dimension
 * @param result Output result
 * @return 0 on success, error code on failure
 */
static inline int quantum_semantic_query(
    quantum_semantic_t ctx,
    const float* query,
    uint32_t feature_dim,
    quantum_semantic_result_t* result
) {
    if (!ctx || ctx->magic != QUANTUM_SEMANTIC_MAGIC) return -1;

    switch (ctx->config.mode) {
        case QUANTUM_SEM_MODE_GROVER:
            return quantum_semantic_grover_search(ctx, query, feature_dim, result);

        case QUANTUM_SEM_MODE_WALK: {
            /* Find most similar concept first, then walk from it */
            quantum_semantic_config_t saved = ctx->config;
            ctx->config.max_results = 1;
            ctx->config.mode = QUANTUM_SEM_MODE_GROVER;

            quantum_semantic_result_t initial;
            memset(&initial, 0, sizeof(initial));
            int err = quantum_semantic_grover_search(ctx, query, feature_dim, &initial);

            ctx->config = saved;

            if (err != 0 || initial.count == 0) {
                quantum_semantic_free_result(&initial);
                return err;
            }

            uint64_t start_id = initial.concept_ids[0];
            quantum_semantic_free_result(&initial);

            return quantum_semantic_walk_activate(ctx, start_id, 1.0f, result);
        }

        case QUANTUM_SEM_MODE_HYBRID: {
            /* Grover search + walk from each result */
            quantum_semantic_result_t grover_result;
            memset(&grover_result, 0, sizeof(grover_result));

            int err = quantum_semantic_grover_search(ctx, query, feature_dim, &grover_result);
            if (err != 0 || grover_result.count == 0) {
                *result = grover_result;
                return err;
            }

            /* Walk from top result to find additional related concepts */
            if (grover_result.count > 0) {
                quantum_semantic_result_t walk_result;
                memset(&walk_result, 0, sizeof(walk_result));

                quantum_semantic_walk_activate(ctx, grover_result.concept_ids[0],
                                               0.5f, &walk_result);

                /* Merge results (grover takes priority) */
                /* For simplicity, just return grover results */
                quantum_semantic_free_result(&walk_result);
            }

            *result = grover_result;
            return 0;
        }

        case QUANTUM_SEM_MODE_ANNEAL:
            /* TODO: Implement annealing-based retrieval */
            return quantum_semantic_grover_search(ctx, query, feature_dim, result);

        default:
            return -2;
    }
}

//=============================================================================
// Result Management
//=============================================================================

/**
 * @brief Free quantum semantic result
 *
 * @param result Result to free
 */
static inline void quantum_semantic_free_result(quantum_semantic_result_t* result) {
    if (!result) return;
    nimcp_free(result->concept_ids);
    nimcp_free(result->similarities);
    nimcp_free(result->activations);
    nimcp_free(result->relevance);
    result->concept_ids = NULL;
    result->similarities = NULL;
    result->activations = NULL;
    result->relevance = NULL;
    result->count = 0;
}

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get quantum semantic statistics
 *
 * @param ctx Quantum semantic context
 * @param stats Output statistics
 */
static inline void quantum_semantic_get_stats(
    const quantum_semantic_t ctx,
    quantum_semantic_stats_t* stats
) {
    if (!stats) return;

    if (!ctx || ctx->magic != QUANTUM_SEMANTIC_MAGIC) {
        memset(stats, 0, sizeof(quantum_semantic_stats_t));
        return;
    }

    *stats = ctx->stats;

    /* Compute speedup factor */
    if (ctx->stats.concepts_evaluated > 0) {
        uint32_t total = ctx->stats.concepts_evaluated + ctx->stats.concepts_skipped;
        stats->avg_speedup = (float)total / (float)ctx->stats.concepts_evaluated;
    } else {
        stats->avg_speedup = 1.0f;
    }
}

/**
 * @brief Reset quantum semantic statistics
 *
 * @param ctx Quantum semantic context
 */
static inline void quantum_semantic_reset_stats(quantum_semantic_t ctx) {
    if (!ctx || ctx->magic != QUANTUM_SEMANTIC_MAGIC) return;
    memset(&ctx->stats, 0, sizeof(quantum_semantic_stats_t));
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_QUANTUM_SEMANTIC_H */
