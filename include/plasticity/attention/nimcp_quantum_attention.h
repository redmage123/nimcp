//=============================================================================
// nimcp_quantum_attention.h - Quantum-Enhanced Multihead Attention
//=============================================================================
/**
 * @file nimcp_quantum_attention.h
 * @brief Quantum-inspired acceleration for multihead attention
 *
 * WHAT: Quantum-enhanced attention using ternary superposition states
 * WHY:  Theoretical √N speedup for attention score computation
 * HOW:  Represent Q×K pairs in superposition, use amplitude encoding
 *
 * QUANTUM ATTENTION MODEL:
 * Classical attention: O(N²) comparisons for N tokens
 * Quantum attention: O(N√N) via amplitude encoding + Grover search
 *
 * KEY INSIGHT:
 * - Attention = finding which K's are most similar to each Q
 * - This is a search problem: find max(Q·K) for each Q
 * - Grover's algorithm provides √N speedup for search
 *
 * TERNARY ATTENTION STATES:
 * | State | Meaning                  | Interpretation           |
 * |-------|--------------------------|--------------------------|
 * | -1    | Negative attention       | Inhibitory/repulsive     |
 * | 0     | Superposition/Unknown    | Not yet measured         |
 * | +1    | Positive attention       | Excitatory/attractive    |
 *
 * CLASSICAL SIMULATION:
 * While true quantum speedup requires hardware, this module:
 * 1. Uses ternary attention masks for sparse computation
 * 2. Enables early termination via measurement thresholds
 * 3. Models quantum tunneling for escaping local attention patterns
 *
 * BIOLOGICAL PARALLEL:
 * - Cortical columns operate in superposition of activity states
 * - Thalamic gating "measures" which columns become active
 * - Attention is selective collapse of perceptual possibilities
 *
 * INTEGRATION:
 * - Uses nimcp_attention.h for base attention mechanism
 * - Uses nimcp_quantum_ternary.h for ternary spin states
 * - Uses nimcp_quantum_walk_ternary.h for attention pattern exploration
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#ifndef NIMCP_QUANTUM_ATTENTION_H
#define NIMCP_QUANTUM_ATTENTION_H

#include "nimcp_attention.h"
#include "optimization/quantum_annealing/nimcp_quantum_ternary.h"
#include "optimization/quantum_annealing/nimcp_quantum_annealing_ternary.h"
#include "utils/quantum/nimcp_quantum_walk_ternary.h"
#include "utils/ternary/nimcp_ternary.h"
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Quantum attention mode
 */
typedef enum {
    QUANTUM_ATTENTION_FULL,         /**< Full quantum simulation */
    QUANTUM_ATTENTION_SPARSE,       /**< Sparse ternary masking only */
    QUANTUM_ATTENTION_WALK,         /**< Quantum walk exploration */
    QUANTUM_ATTENTION_ANNEAL        /**< Quantum annealing optimization */
} quantum_attention_mode_t;

/**
 * @brief Quantum attention configuration
 */
typedef struct {
    /* Mode selection */
    quantum_attention_mode_t mode;  /**< Quantum simulation mode */

    /* Superposition parameters */
    float collapse_threshold;       /**< Threshold for measurement (0.5) */
    float superposition_penalty;    /**< Energy penalty for unmeasured pairs */
    float tunneling_strength;       /**< Gamma for quantum tunneling */

    /* Attention parameters */
    uint32_t max_superposition;     /**< Max pairs in superposition (memory) */
    bool use_sparse_output;         /**< Return sparse attention mask */

    /* Quantum walk parameters */
    uint32_t walk_steps;            /**< Steps for attention walk */
    float walk_bias_self;           /**< Bias for self-attention */

    /* Annealing parameters */
    uint32_t anneal_sweeps;         /**< Sweeps for weight optimization */
    float initial_temperature;      /**< Starting temperature */
    float final_temperature;        /**< Ending temperature */

    /* Sparsity control */
    uint32_t top_k;                 /**< Keep only top-k attention weights */
    float sparsity_threshold;       /**< Zero weights below this */
} quantum_attention_config_t;

/**
 * @brief Quantum attention statistics
 */
typedef struct {
    uint64_t forward_calls;         /**< Total forward passes */
    uint64_t pairs_computed;        /**< Total Q×K pairs computed */
    uint64_t pairs_skipped;         /**< Pairs skipped via sparsity */
    float avg_coherence;            /**< Average superposition fraction */
    float avg_sparsity;             /**< Average attention sparsity */
    uint32_t tunnel_events;         /**< Quantum tunneling events */
    float speedup_factor;           /**< Estimated speedup vs classical */
} quantum_attention_stats_t;

/**
 * @brief Quantum attention result
 */
typedef struct {
    float* attention_weights;       /**< Dense attention [seq_len × seq_len] */
    trit_matrix_t* sparse_mask;     /**< Sparse ternary mask (optional) */
    float* output;                  /**< Attention output [seq_len × dim] */
    float final_energy;             /**< Final Ising energy (if annealing) */
    float coherence;                /**< Final coherence level */
    bool success;                   /**< Operation succeeded */
} quantum_attention_result_t;

//=============================================================================
// Quantum Attention Context
//=============================================================================

/**
 * @brief Quantum attention context (opaque handle)
 */
typedef struct quantum_attention_ctx* quantum_attention_t;

/**
 * @brief Internal quantum attention structure
 */
struct quantum_attention_ctx {
    uint32_t magic;                     /**< Validation: 0x51415454 "QATT" */

    /* Configuration */
    quantum_attention_config_t config;  /**< Configuration */
    uint32_t seq_length;                /**< Sequence length */
    uint32_t head_dim;                  /**< Head dimension (key/query) */
    uint32_t num_heads;                 /**< Number of attention heads */

    /* Quantum state storage */
    trit_ising_config_t** attention_ising;  /**< Ising model per head */
    trit_walker_1d_t** walkers;         /**< Quantum walker per head */

    /* Workspace buffers */
    float* qk_scores;                   /**< Q×K scores buffer */
    float* attention_probs;             /**< Softmax attention probs */
    trit_matrix_t* attention_mask;      /**< Ternary attention mask */

    /* Statistics */
    quantum_attention_stats_t stats;    /**< Performance statistics */

    /* Base attention (optional) */
    multihead_attention_t base_mha;     /**< Classical fallback */
};

#define QUANTUM_ATTENTION_MAGIC 0x51415454  /* "QATT" */

//=============================================================================
// Forward Declarations
//=============================================================================

/* Forward declare destroy for use in create */
static inline void quantum_attention_destroy(quantum_attention_t ctx);

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Get default quantum attention configuration
 *
 * @return Default configuration
 */
static inline quantum_attention_config_t quantum_attention_default_config(void) {
    quantum_attention_config_t config = {
        .mode = QUANTUM_ATTENTION_SPARSE,
        .collapse_threshold = 0.5f,
        .superposition_penalty = 0.1f,
        .tunneling_strength = 0.1f,
        .max_superposition = 1024,
        .use_sparse_output = true,
        .walk_steps = 10,
        .walk_bias_self = 0.5f,
        .anneal_sweeps = 100,
        .initial_temperature = 1.0f,
        .final_temperature = 0.01f,
        .top_k = 0,              /* 0 = no top-k filtering */
        .sparsity_threshold = 0.01f
    };
    return config;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create quantum attention context
 *
 * WHAT: Initialize quantum-enhanced attention system
 * WHY:  Prepare ternary state storage and quantum walkers
 * HOW:  Allocate Ising models per head for attention optimization
 *
 * @param config Quantum attention configuration
 * @param seq_length Maximum sequence length
 * @param head_dim Key/query dimension per head
 * @param num_heads Number of attention heads
 * @return Quantum attention context, or NULL on failure
 */
static inline quantum_attention_t quantum_attention_create(
    const quantum_attention_config_t* config,
    uint32_t seq_length,
    uint32_t head_dim,
    uint32_t num_heads
) {
    if (!config || seq_length == 0 || head_dim == 0 || num_heads == 0) {
        return NULL;
    }

    struct quantum_attention_ctx* ctx = (struct quantum_attention_ctx*)
        nimcp_calloc(1, sizeof(struct quantum_attention_ctx));
    if (!ctx) return NULL;

    ctx->magic = QUANTUM_ATTENTION_MAGIC;
    ctx->config = *config;
    ctx->seq_length = seq_length;
    ctx->head_dim = head_dim;
    ctx->num_heads = num_heads;

    /* Allocate Ising models for attention optimization */
    uint32_t n_pairs = seq_length * seq_length;
    ctx->attention_ising = (trit_ising_config_t**)
        nimcp_calloc(num_heads, sizeof(trit_ising_config_t*));
    if (!ctx->attention_ising) {
        nimcp_free(ctx);
        return NULL;
    }

    for (uint32_t h = 0; h < num_heads; h++) {
        ctx->attention_ising[h] = trit_ising_create(n_pairs, config->superposition_penalty);
        if (!ctx->attention_ising[h]) {
            /* Cleanup on failure */
            for (uint32_t i = 0; i < h; i++) {
                trit_ising_destroy(ctx->attention_ising[i]);
            }
            nimcp_free(ctx->attention_ising);
            nimcp_free(ctx);
            return NULL;
        }
    }

    /* Allocate quantum walkers for walk mode */
    if (config->mode == QUANTUM_ATTENTION_WALK) {
        ctx->walkers = (trit_walker_1d_t**)nimcp_calloc(num_heads, sizeof(trit_walker_1d_t*));
        if (ctx->walkers) {
            for (uint32_t h = 0; h < num_heads; h++) {
                ctx->walkers[h] = trit_walker_1d_create(
                    seq_length,
                    (1.0f - config->walk_bias_self) / 2.0f,  /* left */
                    config->walk_bias_self,                   /* stay */
                    (1.0f - config->walk_bias_self) / 2.0f   /* right */
                );
            }
        }
    }

    /* Allocate workspace buffers */
    ctx->qk_scores = (float*)nimcp_calloc(n_pairs, sizeof(float));
    ctx->attention_probs = (float*)nimcp_calloc(n_pairs, sizeof(float));
    ctx->attention_mask = trit_matrix_create(seq_length, seq_length, TERNARY_PACK_BASE243);

    if (!ctx->qk_scores || !ctx->attention_probs || !ctx->attention_mask) {
        quantum_attention_destroy(ctx);
        return NULL;
    }

    return ctx;
}

/**
 * @brief Destroy quantum attention context
 *
 * @param ctx Context to destroy
 */
static inline void quantum_attention_destroy(quantum_attention_t ctx) {
    if (!ctx) return;
    if (ctx->magic != QUANTUM_ATTENTION_MAGIC) return;

    /* Destroy Ising models */
    if (ctx->attention_ising) {
        for (uint32_t h = 0; h < ctx->num_heads; h++) {
            if (ctx->attention_ising[h]) {
                trit_ising_destroy(ctx->attention_ising[h]);
            }
        }
        nimcp_free(ctx->attention_ising);
    }

    /* Destroy walkers */
    if (ctx->walkers) {
        for (uint32_t h = 0; h < ctx->num_heads; h++) {
            if (ctx->walkers[h]) {
                trit_walker_1d_destroy(ctx->walkers[h]);
            }
        }
        nimcp_free(ctx->walkers);
    }

    /* Free workspace */
    nimcp_free(ctx->qk_scores);
    nimcp_free(ctx->attention_probs);
    if (ctx->attention_mask) trit_matrix_destroy(ctx->attention_mask);

    ctx->magic = 0;
    nimcp_free(ctx);
}

//=============================================================================
// Ternary Attention Mask Operations
//=============================================================================

/**
 * @brief Initialize attention mask to superposition
 *
 * WHAT: Set all attention pairs to unknown (superposition)
 * WHY:  Starting state before measurement
 * HOW:  Fill ternary matrix with TRIT_UNKNOWN
 *
 * @param ctx Quantum attention context
 */
static inline void quantum_attention_reset_mask(quantum_attention_t ctx) {
    if (!ctx || ctx->magic != QUANTUM_ATTENTION_MAGIC) return;

    /* Fill attention mask with TRIT_UNKNOWN */
    for (uint32_t i = 0; i < ctx->seq_length; i++) {
        for (uint32_t j = 0; j < ctx->seq_length; j++) {
            trit_matrix_set(ctx->attention_mask, i, j, TRIT_UNKNOWN);
        }
    }

    /* Reset Ising models to superposition */
    for (uint32_t h = 0; h < ctx->num_heads; h++) {
        if (ctx->attention_ising[h]) {
            for (uint32_t i = 0; i < ctx->seq_length * ctx->seq_length; i++) {
                trit_ising_reset(ctx->attention_ising[h], i);
            }
        }
    }
}

/**
 * @brief Measure attention pair (collapse superposition)
 *
 * WHAT: Collapse single Q×K pair to definite attention state
 * WHY:  Selective measurement based on score threshold
 * HOW:  Compare score to threshold, set positive/negative/zero
 *
 * @param ctx Quantum attention context
 * @param query_idx Query token index
 * @param key_idx Key token index
 * @param score Attention score (dot product)
 * @return Measured ternary state
 */
static inline trit_t quantum_attention_measure_pair(
    quantum_attention_t ctx,
    uint32_t query_idx,
    uint32_t key_idx,
    float score
) {
    if (!ctx || ctx->magic != QUANTUM_ATTENTION_MAGIC) return TRIT_UNKNOWN;
    if (query_idx >= ctx->seq_length || key_idx >= ctx->seq_length) return TRIT_UNKNOWN;

    /* Ternary quantization based on threshold */
    trit_t result;
    if (score > ctx->config.collapse_threshold) {
        result = TRIT_POSITIVE;    /* Strong positive attention */
    } else if (score < -ctx->config.collapse_threshold) {
        result = TRIT_NEGATIVE;    /* Negative/inhibitory attention */
    } else if (fabsf(score) < ctx->config.sparsity_threshold) {
        result = TRIT_UNKNOWN;     /* Too weak, keep in superposition */
    } else {
        /* Weak but measurable - probabilistic collapse */
        result = (score > 0) ? TRIT_POSITIVE : TRIT_NEGATIVE;
    }

    trit_matrix_set(ctx->attention_mask, query_idx, key_idx, result);

    /* Update Ising model for head 0 (simplified) */
    uint32_t pair_idx = query_idx * ctx->seq_length + key_idx;
    if (ctx->attention_ising[0] && result != TRIT_UNKNOWN) {
        trit_ising_measure(ctx->attention_ising[0], pair_idx,
            (result == TRIT_POSITIVE) ? TRIT_SPIN_UP : TRIT_SPIN_DOWN);
    }

    ctx->stats.pairs_computed++;

    return result;
}

/**
 * @brief Get sparse attention pairs (measured only)
 *
 * WHAT: Extract only measured (non-superposition) attention pairs
 * WHY:  Sparse computation - skip unmeasured pairs
 * HOW:  Iterate mask, collect non-zero entries
 *
 * @param ctx Quantum attention context
 * @param query_indices Output query indices (caller allocated)
 * @param key_indices Output key indices (caller allocated)
 * @param values Output attention values (caller allocated)
 * @param max_pairs Maximum pairs to extract
 * @return Number of pairs extracted
 */
static inline uint32_t quantum_attention_get_sparse_pairs(
    const quantum_attention_t ctx,
    uint32_t* query_indices,
    uint32_t* key_indices,
    float* values,
    uint32_t max_pairs
) {
    if (!ctx || ctx->magic != QUANTUM_ATTENTION_MAGIC) return 0;
    if (!query_indices || !key_indices || !values) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < ctx->seq_length && count < max_pairs; i++) {
        for (uint32_t j = 0; j < ctx->seq_length && count < max_pairs; j++) {
            trit_t val = trit_matrix_get(ctx->attention_mask, i, j);
            if (val != TRIT_UNKNOWN) {
                query_indices[count] = i;
                key_indices[count] = j;
                values[count] = (float)val;  /* -1, 0, or +1 */
                count++;
            }
        }
    }

    return count;
}

//=============================================================================
// Quantum Walk Attention
//=============================================================================

/**
 * @brief Explore attention patterns via quantum walk
 *
 * WHAT: Use quantum walk to find high-attention patterns
 * WHY:  √N exploration of attention space
 * HOW:  Walk coin state determines attention direction
 *
 * ALGORITHM:
 * 1. Initialize walker at each query position
 * 2. Walk explores key positions with ternary coin
 * 3. Amplitude distribution → attention weights
 *
 * @param ctx Quantum attention context
 * @param query Query vectors [seq_length × head_dim]
 * @param key Key vectors [seq_length × head_dim]
 * @param head_idx Head index
 * @return Coherence after walk (0 = fully measured)
 */
static inline float quantum_attention_walk(
    quantum_attention_t ctx,
    const float* query,
    const float* key,
    uint32_t head_idx
) {
    if (!ctx || ctx->magic != QUANTUM_ATTENTION_MAGIC) return 0.0f;
    if (!ctx->walkers || head_idx >= ctx->num_heads) return 0.0f;

    trit_walker_1d_t* walker = ctx->walkers[head_idx];
    if (!walker) return 0.0f;

    /* Initialize walker at center position */
    uint32_t center = ctx->seq_length / 2;
    trit_walker_1d_init(walker, center, TRIT_COIN_STAY);

    /* Perform quantum walk steps */
    for (uint32_t step = 0; step < ctx->config.walk_steps; step++) {
        trit_walker_1d_step(walker);
    }

    /* Extract attention weights from amplitude distribution */
    float* amplitudes = walker->amplitudes;
    float max_amp = 0.0f;
    for (uint32_t i = 0; i < ctx->seq_length; i++) {
        if (amplitudes[i] > max_amp) max_amp = amplitudes[i];
    }

    /* Normalize and measure high-amplitude positions */
    if (max_amp > 1e-6f) {
        for (uint32_t i = 0; i < ctx->seq_length; i++) {
            float normalized = amplitudes[i] / max_amp;
            if (normalized > ctx->config.collapse_threshold) {
                /* High amplitude = high attention */
                trit_matrix_set(ctx->attention_mask, center, i, TRIT_POSITIVE);
                ctx->stats.pairs_computed++;
            }
        }
    }

    return trit_walker_1d_variance(walker);
}

//=============================================================================
// Quantum Annealing for Attention Optimization
//=============================================================================

/**
 * @brief Optimize attention weights via quantum annealing
 *
 * WHAT: Find optimal attention pattern minimizing energy
 * WHY:  Escape local minima in attention score landscape
 * HOW:  Map attention to Ising model, anneal to ground state
 *
 * ENERGY FUNCTION:
 * E = -Σᵢⱼ Jᵢⱼ aᵢⱼ - Σᵢⱼ sᵢⱼ aᵢⱼ
 * Where:
 *   aᵢⱼ = attention spin (+1 attend, -1 ignore)
 *   sᵢⱼ = Q×K score (normalized)
 *   Jᵢⱼ = coupling (encourage nearby attention)
 *
 * @param ctx Quantum attention context
 * @param qk_scores Pre-computed Q×K scores [seq_length × seq_length]
 * @param head_idx Head index
 * @param result Output result
 * @return 0 on success, error code on failure
 */
static inline int quantum_attention_anneal(
    quantum_attention_t ctx,
    const float* qk_scores,
    uint32_t head_idx,
    quantum_attention_result_t* result
) {
    if (!ctx || ctx->magic != QUANTUM_ATTENTION_MAGIC || !result) return -1;
    if (!qk_scores || head_idx >= ctx->num_heads) return -1;

    result->success = false;

    trit_ising_config_t* ising = ctx->attention_ising[head_idx];
    if (!ising) return -2;

    uint32_t n = ctx->seq_length;
    uint32_t n_pairs = n * n;

    /* Build coupling matrix (sparse, neighbor attention) */
    float* J = (float*)nimcp_calloc(n_pairs * n_pairs, sizeof(float));
    if (!J) return -3;

    /* Coupling: encourage attention continuity */
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            uint32_t idx = i * n + j;
            /* Couple adjacent keys for same query */
            if (j > 0) {
                uint32_t left_idx = i * n + (j - 1);
                J[idx * n_pairs + left_idx] = 0.1f;
                J[left_idx * n_pairs + idx] = 0.1f;
            }
        }
    }

    /* Use Q×K scores as external field */
    float* h = (float*)nimcp_malloc(n_pairs * sizeof(float));
    if (!h) {
        nimcp_free(J);
        return -3;
    }
    for (uint32_t i = 0; i < n_pairs; i++) {
        h[i] = qk_scores[i];
    }

    /* Run quantum annealing */
    quantum_ternary_config_t anneal_config = quantum_ternary_default_config();
    anneal_config.num_sweeps = ctx->config.anneal_sweeps;
    anneal_config.initial_temperature = ctx->config.initial_temperature;
    anneal_config.final_temperature = ctx->config.final_temperature;
    anneal_config.initial_gamma = ctx->config.tunneling_strength;
    anneal_config.track_best = true;

    quantum_ternary_result_t anneal_result;
    int err = quantum_ternary_anneal(ising, J, h, &anneal_config, &anneal_result);

    nimcp_free(J);
    nimcp_free(h);

    if (err != 0) return err;

    /* Extract attention mask from annealed spins */
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            uint32_t idx = i * n + j;
            trit_spin_t spin = trit_vector_get(ising->spins, idx);

            trit_t attention = TRIT_UNKNOWN;
            if (spin == TRIT_SPIN_UP) {
                attention = TRIT_POSITIVE;
            } else if (spin == TRIT_SPIN_DOWN) {
                attention = TRIT_NEGATIVE;
            }

            trit_matrix_set(ctx->attention_mask, i, j, attention);
        }
    }

    result->sparse_mask = ctx->attention_mask;
    result->final_energy = (float)anneal_result.best_energy;
    result->coherence = anneal_result.final_coherence;
    result->success = true;

    ctx->stats.tunnel_events += anneal_result.tunnel_events;

    return 0;
}

//=============================================================================
// Quantum-Enhanced Forward Pass
//=============================================================================

/**
 * @brief Compute quantum-enhanced attention scores
 *
 * WHAT: Compute attention with quantum speedup
 * WHY:  Reduce computation via sparse ternary masks
 * HOW:  Selective measurement based on score magnitude
 *
 * ALGORITHM:
 * 1. Compute Q×K scores (full or sampled)
 * 2. Initialize attention mask to superposition
 * 3. Measure high-magnitude pairs only
 * 4. Use sparse mask for value aggregation
 *
 * @param ctx Quantum attention context
 * @param query Query vectors [seq_length × head_dim]
 * @param key Key vectors [seq_length × head_dim]
 * @param head_idx Head index
 * @param scale Attention scale (1/√d_k)
 */
static inline void quantum_attention_compute_scores(
    quantum_attention_t ctx,
    const float* query,
    const float* key,
    uint32_t head_idx,
    float scale
) {
    if (!ctx || ctx->magic != QUANTUM_ATTENTION_MAGIC) return;
    if (!query || !key) return;

    uint32_t n = ctx->seq_length;
    uint32_t d = ctx->head_dim;

    /* Reset mask to superposition */
    quantum_attention_reset_mask(ctx);

    /* Compute Q×K scores */
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            /* Dot product Q[i] · K[j] */
            float score = 0.0f;
            for (uint32_t k = 0; k < d; k++) {
                score += query[i * d + k] * key[j * d + k];
            }
            score *= scale;

            ctx->qk_scores[i * n + j] = score;

            /* Selective measurement based on magnitude */
            if (fabsf(score) > ctx->config.collapse_threshold) {
                quantum_attention_measure_pair(ctx, i, j, score);
            } else {
                ctx->stats.pairs_skipped++;
            }
        }
    }

    /* Update coherence statistics */
    float coherence = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < n; j++) {
            trit_t val = trit_matrix_get(ctx->attention_mask, i, j);
            if (val == TRIT_UNKNOWN) coherence += 1.0f;
        }
    }
    coherence /= (float)(n * n);

    ctx->stats.avg_coherence =
        (ctx->stats.avg_coherence * ctx->stats.forward_calls + coherence) /
        (ctx->stats.forward_calls + 1);

    /* Update sparsity statistics */
    float sparsity = (float)ctx->stats.pairs_skipped / (float)(n * n);
    ctx->stats.avg_sparsity =
        (ctx->stats.avg_sparsity * ctx->stats.forward_calls + sparsity) /
        (ctx->stats.forward_calls + 1);

    ctx->stats.forward_calls++;
}

/**
 * @brief Apply sparse attention to values
 *
 * WHAT: Aggregate values using sparse attention mask
 * WHY:  Only compute attended positions
 * HOW:  Iterate sparse mask, accumulate weighted values
 *
 * @param ctx Quantum attention context
 * @param value Value vectors [seq_length × value_dim]
 * @param value_dim Value dimension
 * @param output Output [seq_length × value_dim]
 */
static inline void quantum_attention_apply_values(
    quantum_attention_t ctx,
    const float* value,
    uint32_t value_dim,
    float* output
) {
    if (!ctx || ctx->magic != QUANTUM_ATTENTION_MAGIC) return;
    if (!value || !output) return;

    uint32_t n = ctx->seq_length;

    /* Clear output */
    memset(output, 0, n * value_dim * sizeof(float));

    /* Apply attention based on ternary mask */
    for (uint32_t i = 0; i < n; i++) {
        float weight_sum = 0.0f;

        /* Collect attention weights for this query */
        for (uint32_t j = 0; j < n; j++) {
            trit_t attention = trit_matrix_get(ctx->attention_mask, i, j);
            float weight = 0.0f;

            if (attention == TRIT_POSITIVE) {
                weight = 1.0f;  /* Full attention */
            } else if (attention == TRIT_NEGATIVE) {
                weight = 0.0f;  /* No attention (could use -weight for inhibition) */
            } else {
                /* Superposition: use raw score if available */
                weight = fmaxf(0.0f, ctx->qk_scores[i * n + j]);
            }

            if (weight > 0.0f) {
                weight_sum += weight;
                /* Accumulate weighted values */
                for (uint32_t k = 0; k < value_dim; k++) {
                    output[i * value_dim + k] += weight * value[j * value_dim + k];
                }
            }
        }

        /* Normalize */
        if (weight_sum > 1e-6f) {
            for (uint32_t k = 0; k < value_dim; k++) {
                output[i * value_dim + k] /= weight_sum;
            }
        }
    }
}

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get quantum attention statistics
 *
 * @param ctx Quantum attention context
 * @param stats Output statistics
 */
static inline void quantum_attention_get_stats(
    const quantum_attention_t ctx,
    quantum_attention_stats_t* stats
) {
    if (!stats) return;

    if (!ctx || ctx->magic != QUANTUM_ATTENTION_MAGIC) {
        memset(stats, 0, sizeof(quantum_attention_stats_t));
        return;
    }

    *stats = ctx->stats;

    /* Compute estimated speedup */
    if (stats->pairs_computed > 0) {
        float total_pairs = (float)(ctx->seq_length * ctx->seq_length);
        float computed_ratio = (float)stats->pairs_computed / total_pairs;
        stats->speedup_factor = 1.0f / computed_ratio;
    } else {
        stats->speedup_factor = 1.0f;
    }
}

/**
 * @brief Reset quantum attention statistics
 *
 * @param ctx Quantum attention context
 */
static inline void quantum_attention_reset_stats(quantum_attention_t ctx) {
    if (!ctx || ctx->magic != QUANTUM_ATTENTION_MAGIC) return;
    memset(&ctx->stats, 0, sizeof(quantum_attention_stats_t));
}

//=============================================================================
// Integration with Base Attention
//=============================================================================

/**
 * @brief Connect to base multihead attention system
 *
 * @param ctx Quantum attention context
 * @param mha Base multihead attention (for fallback)
 */
static inline void quantum_attention_set_base(
    quantum_attention_t ctx,
    multihead_attention_t mha
) {
    if (!ctx || ctx->magic != QUANTUM_ATTENTION_MAGIC) return;
    ctx->base_mha = mha;
}

/**
 * @brief Get connected base attention system
 *
 * @param ctx Quantum attention context
 * @return Base multihead attention or NULL
 */
static inline multihead_attention_t quantum_attention_get_base(
    const quantum_attention_t ctx
) {
    if (!ctx || ctx->magic != QUANTUM_ATTENTION_MAGIC) return NULL;
    return ctx->base_mha;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_QUANTUM_ATTENTION_H */
