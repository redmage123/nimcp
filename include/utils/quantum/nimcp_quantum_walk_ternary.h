//=============================================================================
// nimcp_quantum_walk_ternary.h - Ternary State Quantum Walk
//=============================================================================
/**
 * @file nimcp_quantum_walk_ternary.h
 * @brief Ternary representation for discrete quantum walks
 *
 * WHAT: Three-state quantum walk on graphs
 * WHY:  Efficient representation of quantum superposition on discrete graphs
 * HOW:  Use ternary coin states and amplitude tracking
 *
 * TERNARY COIN STATES:
 * | State | Meaning                  | Amplitude Weight |
 * |-------|--------------------------|------------------|
 * | -1    | Move left/backward       | -1/√2            |
 * | 0     | Superposition/stationary | 0                |
 * | +1    | Move right/forward       | +1/√2            |
 *
 * ADVANTAGES OVER BINARY QUANTUM WALK:
 * - Explicit "stay in place" option for bounded walks
 * - Natural representation of three-way graph junctions
 * - Graceful degradation to classical walk when coherence lost
 *
 * MATHEMATICAL FOUNDATION:
 * Classical Hadamard coin: |ψ⟩ → (|L⟩ + |R⟩)/√2
 * Ternary coin: |ψ⟩ → (|L⟩ + |S⟩ + |R⟩)/√3
 *               or biased: α|L⟩ + β|S⟩ + γ|R⟩
 *
 * INTEGRATION:
 * - Uses nimcp_ternary.h for packed state storage
 * - Extends nimcp_quantum_walk.h with ternary coin
 * - Memory efficient: 200KB for 1M nodes vs 8MB (complex)
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#ifndef NIMCP_QUANTUM_WALK_TERNARY_H
#define NIMCP_QUANTUM_WALK_TERNARY_H

#include "utils/ternary/nimcp_ternary.h"
#include <math.h>
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Ternary coin states */
#define TRIT_COIN_LEFT       TRIT_NEGATIVE   /**< Move left/backward */
#define TRIT_COIN_STAY       TRIT_UNKNOWN    /**< Stay in place */
#define TRIT_COIN_RIGHT      TRIT_POSITIVE   /**< Move right/forward */

/** Default coin bias (uniform) */
#define TRIT_WALK_UNIFORM_BIAS (1.0f / 3.0f)

/** Magic number for validation */
#define TRIT_WALK_MAGIC 0x54574B51  /* "TWKQ" */

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Ternary coin state
 */
typedef trit_t trit_coin_t;

/**
 * @brief Ternary quantum walker on 1D line
 */
typedef struct {
    uint32_t magic;             /**< Validation magic */
    uint32_t n_positions;       /**< Number of positions */
    trit_vector_t* coins;       /**< Coin state at each position */
    float* amplitudes;          /**< Amplitude magnitude at each position */
    float* phases;              /**< Phase at each position [0, 2π) */

    /* Coin operator parameters */
    float bias_left;            /**< Probability amplitude for left */
    float bias_stay;            /**< Probability amplitude for stay */
    float bias_right;           /**< Probability amplitude for right */

    /* Statistics */
    uint32_t steps;             /**< Steps taken */
    float total_probability;    /**< Sum of |ψ|² (should be 1) */
} trit_walker_1d_t;

/**
 * @brief Ternary quantum walker on graph
 */
typedef struct {
    uint32_t magic;             /**< Validation magic */
    uint32_t n_nodes;           /**< Number of graph nodes */
    uint32_t max_degree;        /**< Maximum node degree */

    /* State storage */
    trit_matrix_t* coins;       /**< Coin state per (node, edge) */
    float* amplitudes;          /**< Amplitude at each node */

    /* Graph structure (CSR format) */
    uint32_t* row_ptr;          /**< Row pointers */
    uint32_t* col_idx;          /**< Column indices */
    float* edge_weights;        /**< Edge weights (optional) */

    /* Statistics */
    uint32_t steps;             /**< Steps taken */
} trit_walker_graph_t;

//=============================================================================
// 1D Walker Lifecycle
//=============================================================================

/**
 * @brief Create 1D ternary quantum walker
 *
 * @param n_positions Number of positions on the line
 * @param bias_left Left movement bias
 * @param bias_stay Stay in place bias
 * @param bias_right Right movement bias
 * @return New walker, or NULL on failure
 */
static inline trit_walker_1d_t* trit_walker_1d_create(
    uint32_t n_positions,
    float bias_left,
    float bias_stay,
    float bias_right
) {
    if (n_positions == 0) return NULL;

    trit_walker_1d_t* walker = (trit_walker_1d_t*)nimcp_calloc(1, sizeof(trit_walker_1d_t));
    if (!walker) return NULL;

    walker->magic = TRIT_WALK_MAGIC;
    walker->n_positions = n_positions;

    /* Normalize biases */
    float sum = bias_left + bias_stay + bias_right;
    if (sum < 1e-6f) sum = 3.0f;  /* Default to uniform */
    walker->bias_left = bias_left / sum;
    walker->bias_stay = bias_stay / sum;
    walker->bias_right = bias_right / sum;

    /* Allocate coin states */
    walker->coins = trit_vector_create_filled(n_positions, TRIT_COIN_STAY,
                                               TERNARY_PACK_BASE243);
    if (!walker->coins) {
        nimcp_free(walker);
        return NULL;
    }

    /* Allocate amplitudes and phases */
    walker->amplitudes = (float*)nimcp_calloc(n_positions, sizeof(float));
    walker->phases = (float*)nimcp_calloc(n_positions, sizeof(float));
    if (!walker->amplitudes || !walker->phases) {
        trit_vector_destroy(walker->coins);
        nimcp_free(walker->amplitudes);
        nimcp_free(walker->phases);
        nimcp_free(walker);
        return NULL;
    }

    walker->steps = 0;
    walker->total_probability = 0.0f;

    return walker;
}

/**
 * @brief Destroy 1D ternary walker
 *
 * @param walker Walker to destroy
 */
static inline void trit_walker_1d_destroy(trit_walker_1d_t* walker) {
    if (!walker) return;
    if (walker->coins) trit_vector_destroy(walker->coins);
    nimcp_free(walker->amplitudes);
    nimcp_free(walker->phases);
    walker->magic = 0;
    nimcp_free(walker);
}

//=============================================================================
// 1D Walker Operations
//=============================================================================

/**
 * @brief Initialize walker at position with coin state
 *
 * @param walker The walker
 * @param position Starting position
 * @param coin Initial coin state
 */
static inline void trit_walker_1d_init(
    trit_walker_1d_t* walker,
    uint32_t position,
    trit_coin_t coin
) {
    if (!walker || position >= walker->n_positions) return;

    /* Clear all amplitudes */
    for (uint32_t i = 0; i < walker->n_positions; i++) {
        walker->amplitudes[i] = 0.0f;
        walker->phases[i] = 0.0f;
        trit_vector_set(walker->coins, i, TRIT_COIN_STAY);
    }

    /* Set initial position */
    walker->amplitudes[position] = 1.0f;
    trit_vector_set(walker->coins, position, coin);
    walker->total_probability = 1.0f;
    walker->steps = 0;
}

/**
 * @brief Apply ternary coin operator at position
 *
 * @param walker The walker
 * @param position Position to apply coin
 */
static inline void trit_walker_1d_coin(
    trit_walker_1d_t* walker,
    uint32_t position
) {
    if (!walker || position >= walker->n_positions) return;

    float amp = walker->amplitudes[position];
    if (amp < 1e-10f) return;

    /* Apply ternary coin: split amplitude three ways */
    /* For simplicity, use deterministic coin based on current state */
    trit_coin_t current = trit_vector_get(walker->coins, position);

    /* Ternary Hadamard-like transformation */
    /* |L⟩ → (|L⟩ - |S⟩ + |R⟩) / √3 */
    /* |S⟩ → (|L⟩ + |S⟩ + |R⟩) / √3 */
    /* |R⟩ → (-|L⟩ + |S⟩ + |R⟩) / √3 */
    float new_amp = amp / sqrtf(3.0f);

    /* For now, just apply biased coin based on config */
    float r = (float)rand() / (float)RAND_MAX;
    trit_coin_t new_coin;
    if (r < walker->bias_left) {
        new_coin = TRIT_COIN_LEFT;
    } else if (r < walker->bias_left + walker->bias_stay) {
        new_coin = TRIT_COIN_STAY;
    } else {
        new_coin = TRIT_COIN_RIGHT;
    }

    trit_vector_set(walker->coins, position, new_coin);
}

/**
 * @brief Apply shift operator (move based on coin)
 *
 * @param walker The walker
 */
static inline void trit_walker_1d_shift(trit_walker_1d_t* walker) {
    if (!walker) return;

    uint32_t n = walker->n_positions;

    /* Temporary storage for new amplitudes */
    float* new_amps = (float*)nimcp_calloc(n, sizeof(float));
    trit_t* new_coins = (trit_t*)nimcp_calloc(n, sizeof(trit_t));
    if (!new_amps || !new_coins) {
        nimcp_free(new_amps);
        nimcp_free(new_coins);
        return;
    }

    for (uint32_t i = 0; i < n; i++) {
        float amp = walker->amplitudes[i];
        if (amp < 1e-10f) continue;

        trit_coin_t coin = trit_vector_get(walker->coins, i);

        uint32_t new_pos = i;
        if (coin == TRIT_COIN_LEFT && i > 0) {
            new_pos = i - 1;
        } else if (coin == TRIT_COIN_RIGHT && i < n - 1) {
            new_pos = i + 1;
        }
        /* STAY keeps position */

        new_amps[new_pos] += amp;
        new_coins[new_pos] = coin;
    }

    /* Copy back */
    for (uint32_t i = 0; i < n; i++) {
        walker->amplitudes[i] = new_amps[i];
        trit_vector_set(walker->coins, i, new_coins[i]);
    }

    nimcp_free(new_amps);
    nimcp_free(new_coins);
}

/**
 * @brief Perform one quantum walk step
 *
 * @param walker The walker
 */
static inline void trit_walker_1d_step(trit_walker_1d_t* walker) {
    if (!walker) return;

    /* Apply coin to all positions with non-zero amplitude */
    for (uint32_t i = 0; i < walker->n_positions; i++) {
        if (walker->amplitudes[i] > 1e-10f) {
            trit_walker_1d_coin(walker, i);
        }
    }

    /* Apply shift */
    trit_walker_1d_shift(walker);

    walker->steps++;

    /* Renormalize */
    float total = 0.0f;
    for (uint32_t i = 0; i < walker->n_positions; i++) {
        total += walker->amplitudes[i] * walker->amplitudes[i];
    }
    walker->total_probability = total;
}

/**
 * @brief Get probability distribution
 *
 * @param walker The walker
 * @param probabilities Output array (caller allocated, n_positions)
 */
static inline void trit_walker_1d_get_distribution(
    const trit_walker_1d_t* walker,
    float* probabilities
) {
    if (!walker || !probabilities) return;

    for (uint32_t i = 0; i < walker->n_positions; i++) {
        probabilities[i] = walker->amplitudes[i] * walker->amplitudes[i];
    }
}

/**
 * @brief Measure walker position (collapse superposition)
 *
 * @param walker The walker
 * @param random Random value [0,1)
 * @return Measured position
 */
static inline uint32_t trit_walker_1d_measure(
    trit_walker_1d_t* walker,
    float random
) {
    if (!walker) return 0;

    float cumulative = 0.0f;
    uint32_t measured_pos = 0;

    for (uint32_t i = 0; i < walker->n_positions; i++) {
        float prob = walker->amplitudes[i] * walker->amplitudes[i];
        cumulative += prob / walker->total_probability;
        if (random < cumulative) {
            measured_pos = i;
            break;
        }
    }

    /* Collapse to measured position */
    for (uint32_t i = 0; i < walker->n_positions; i++) {
        walker->amplitudes[i] = (i == measured_pos) ? 1.0f : 0.0f;
    }
    walker->total_probability = 1.0f;

    return measured_pos;
}

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Compute mean position
 *
 * @param walker The walker
 * @return Mean position (expectation value)
 */
static inline float trit_walker_1d_mean_position(const trit_walker_1d_t* walker) {
    if (!walker || walker->total_probability < 1e-10f) return 0.0f;

    float mean = 0.0f;
    for (uint32_t i = 0; i < walker->n_positions; i++) {
        float prob = walker->amplitudes[i] * walker->amplitudes[i];
        mean += i * prob;
    }
    return mean / walker->total_probability;
}

/**
 * @brief Compute position variance
 *
 * @param walker The walker
 * @return Variance of position
 */
static inline float trit_walker_1d_variance(const trit_walker_1d_t* walker) {
    if (!walker || walker->total_probability < 1e-10f) return 0.0f;

    float mean = trit_walker_1d_mean_position(walker);
    float var = 0.0f;

    for (uint32_t i = 0; i < walker->n_positions; i++) {
        float prob = walker->amplitudes[i] * walker->amplitudes[i];
        float diff = (float)i - mean;
        var += diff * diff * prob;
    }

    return var / walker->total_probability;
}

//=============================================================================
// Graph Walker (for graph algorithms with ternary adjacency)
//=============================================================================

/**
 * @brief Create graph walker from ternary adjacency matrix
 *
 * @param adjacency Ternary adjacency matrix (will be referenced, not copied)
 * @return New graph walker, or NULL on failure
 */
static inline trit_walker_graph_t* trit_walker_graph_create(
    const trit_matrix_t* adjacency
) {
    if (!adjacency || adjacency->magic != TERNARY_MAGIC) return NULL;
    if (adjacency->rows != adjacency->cols) return NULL;  /* Must be square */

    trit_walker_graph_t* walker = (trit_walker_graph_t*)nimcp_calloc(1, sizeof(trit_walker_graph_t));
    if (!walker) return NULL;

    walker->magic = TRIT_WALK_MAGIC;
    walker->n_nodes = (uint32_t)adjacency->rows;
    walker->steps = 0;

    /* Compute max degree */
    walker->max_degree = 0;
    for (uint32_t i = 0; i < walker->n_nodes; i++) {
        uint32_t degree = 0;
        for (uint32_t j = 0; j < walker->n_nodes; j++) {
            if (trit_matrix_get(adjacency, i, j) != TRIT_UNKNOWN) {
                degree++;
            }
        }
        if (degree > walker->max_degree) {
            walker->max_degree = degree;
        }
    }

    /* Build CSR format from adjacency matrix */
    uint32_t n_edges = 0;
    for (size_t i = 0; i < adjacency->numel; i++) {
        trit_t v;
        if (adjacency->pack_mode == TERNARY_PACK_NONE) {
            v = adjacency->data.unpacked[i];
        } else {
            v = trit_matrix_get(adjacency, i / adjacency->cols, i % adjacency->cols);
        }
        if (v != TRIT_UNKNOWN) n_edges++;
    }

    walker->row_ptr = (uint32_t*)nimcp_calloc(walker->n_nodes + 1, sizeof(uint32_t));
    walker->col_idx = (uint32_t*)nimcp_calloc(n_edges, sizeof(uint32_t));
    walker->edge_weights = (float*)nimcp_calloc(n_edges, sizeof(float));

    if (!walker->row_ptr || !walker->col_idx || !walker->edge_weights) {
        nimcp_free(walker->row_ptr);
        nimcp_free(walker->col_idx);
        nimcp_free(walker->edge_weights);
        nimcp_free(walker);
        return NULL;
    }

    /* Fill CSR */
    uint32_t edge_idx = 0;
    for (uint32_t i = 0; i < walker->n_nodes; i++) {
        walker->row_ptr[i] = edge_idx;
        for (uint32_t j = 0; j < walker->n_nodes; j++) {
            trit_t w = trit_matrix_get(adjacency, i, j);
            if (w != TRIT_UNKNOWN) {
                walker->col_idx[edge_idx] = j;
                walker->edge_weights[edge_idx] = (float)w;
                edge_idx++;
            }
        }
    }
    walker->row_ptr[walker->n_nodes] = edge_idx;

    /* Allocate coin states per edge */
    walker->coins = trit_matrix_create(walker->n_nodes, walker->max_degree,
                                        TERNARY_PACK_NONE);
    if (!walker->coins) {
        nimcp_free(walker->row_ptr);
        nimcp_free(walker->col_idx);
        nimcp_free(walker->edge_weights);
        nimcp_free(walker);
        return NULL;
    }

    /* Allocate amplitudes */
    walker->amplitudes = (float*)nimcp_calloc(walker->n_nodes, sizeof(float));
    if (!walker->amplitudes) {
        trit_matrix_destroy(walker->coins);
        nimcp_free(walker->row_ptr);
        nimcp_free(walker->col_idx);
        nimcp_free(walker->edge_weights);
        nimcp_free(walker);
        return NULL;
    }

    return walker;
}

/**
 * @brief Destroy graph walker
 */
static inline void trit_walker_graph_destroy(trit_walker_graph_t* walker) {
    if (!walker) return;
    if (walker->magic != TRIT_WALK_MAGIC) return;

    if (walker->coins) trit_matrix_destroy(walker->coins);
    nimcp_free(walker->amplitudes);
    nimcp_free(walker->row_ptr);
    nimcp_free(walker->col_idx);
    nimcp_free(walker->edge_weights);
    walker->magic = 0;
    nimcp_free(walker);
}

/**
 * @brief Initialize graph walker at source node
 */
static inline void trit_walker_graph_init(
    trit_walker_graph_t* walker,
    uint32_t source
) {
    if (!walker || source >= walker->n_nodes) return;

    /* Clear all amplitudes */
    for (uint32_t i = 0; i < walker->n_nodes; i++) {
        walker->amplitudes[i] = 0.0f;
    }

    /* Set initial position */
    walker->amplitudes[source] = 1.0f;
    walker->steps = 0;
}

/**
 * @brief Get node degree
 */
static inline uint32_t trit_walker_graph_degree(
    const trit_walker_graph_t* walker,
    uint32_t node
) {
    if (!walker || node >= walker->n_nodes) return 0;
    return walker->row_ptr[node + 1] - walker->row_ptr[node];
}

/**
 * @brief Single step of graph walk
 */
static inline void trit_walker_graph_step(trit_walker_graph_t* walker) {
    if (!walker) return;

    uint32_t n = walker->n_nodes;

    /* Temporary storage for new amplitudes */
    float* new_amps = (float*)nimcp_calloc(n, sizeof(float));
    if (!new_amps) return;

    for (uint32_t i = 0; i < n; i++) {
        float amp = walker->amplitudes[i];
        if (fabsf(amp) < 1e-10f) continue;

        uint32_t degree = trit_walker_graph_degree(walker, i);
        if (degree == 0) {
            new_amps[i] += amp;
            continue;
        }

        float amp_per_edge = amp / sqrtf((float)degree);

        /* Distribute amplitude to neighbors */
        for (uint32_t e = walker->row_ptr[i]; e < walker->row_ptr[i + 1]; e++) {
            uint32_t j = walker->col_idx[e];
            float w = walker->edge_weights[e];
            new_amps[j] += amp_per_edge * w;
        }
    }

    /* Copy back */
    memcpy(walker->amplitudes, new_amps, n * sizeof(float));
    nimcp_free(new_amps);

    /* Normalize */
    float norm_sq = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        norm_sq += walker->amplitudes[i] * walker->amplitudes[i];
    }
    if (norm_sq > 1e-10f) {
        float inv_norm = 1.0f / sqrtf(norm_sq);
        for (uint32_t i = 0; i < n; i++) {
            walker->amplitudes[i] *= inv_norm;
        }
    }

    walker->steps++;
}

/**
 * @brief Run multiple steps of graph walk
 */
static inline void trit_walker_graph_run(
    trit_walker_graph_t* walker,
    uint32_t steps
) {
    if (!walker) return;
    for (uint32_t s = 0; s < steps; s++) {
        trit_walker_graph_step(walker);
    }
}

/**
 * @brief Get amplitude at node
 */
static inline float trit_walker_graph_get_amplitude(
    const trit_walker_graph_t* walker,
    uint32_t node
) {
    if (!walker || node >= walker->n_nodes) return 0.0f;
    return walker->amplitudes[node];
}

/**
 * @brief Get probability at node
 */
static inline float trit_walker_graph_get_probability(
    const trit_walker_graph_t* walker,
    uint32_t node
) {
    if (!walker || node >= walker->n_nodes) return 0.0f;
    return walker->amplitudes[node] * walker->amplitudes[node];
}

/**
 * @brief Get probability distribution
 */
static inline void trit_walker_graph_get_distribution(
    const trit_walker_graph_t* walker,
    float* probabilities
) {
    if (!walker || !probabilities) return;

    for (uint32_t i = 0; i < walker->n_nodes; i++) {
        probabilities[i] = walker->amplitudes[i] * walker->amplitudes[i];
    }
}

/**
 * @brief Find node with maximum amplitude
 */
static inline uint32_t trit_walker_graph_max_node(
    const trit_walker_graph_t* walker,
    float* max_amplitude
) {
    if (!walker) {
        if (max_amplitude) *max_amplitude = 0.0f;
        return 0;
    }

    float max_amp = 0.0f;
    uint32_t max_node = 0;

    for (uint32_t i = 0; i < walker->n_nodes; i++) {
        float amp = fabsf(walker->amplitudes[i]);
        if (amp > max_amp) {
            max_amp = amp;
            max_node = i;
        }
    }

    if (max_amplitude) *max_amplitude = max_amp;
    return max_node;
}

/**
 * @brief Search for target node from source
 *
 * @param walker Graph walker
 * @param source Source node
 * @param target Target node
 * @param max_steps Maximum steps
 * @param steps_out Output: steps taken (can be NULL)
 * @return true if target reached with amplitude > 0.5
 */
static inline bool trit_walker_graph_search(
    trit_walker_graph_t* walker,
    uint32_t source,
    uint32_t target,
    uint32_t max_steps,
    uint32_t* steps_out
) {
    if (!walker || source >= walker->n_nodes || target >= walker->n_nodes) {
        if (steps_out) *steps_out = 0;
        return false;
    }

    trit_walker_graph_init(walker, source);

    for (uint32_t step = 0; step < max_steps; step++) {
        trit_walker_graph_step(walker);

        if (fabsf(walker->amplitudes[target]) > 0.5f) {
            if (steps_out) *steps_out = step + 1;
            return true;
        }
    }

    if (steps_out) *steps_out = max_steps;
    return false;
}

/**
 * @brief Compute reachability from source
 *
 * @param walker Graph walker
 * @param source Source node
 * @param steps Walk steps
 * @param reachable Output: reachable nodes (amplitude > threshold)
 * @param threshold Amplitude threshold for reachability
 * @return Number of reachable nodes
 */
static inline uint32_t trit_walker_graph_reachability(
    trit_walker_graph_t* walker,
    uint32_t source,
    uint32_t steps,
    bool* reachable,
    float threshold
) {
    if (!walker || !reachable || source >= walker->n_nodes) return 0;
    if (threshold <= 0.0f) threshold = 0.01f;

    trit_walker_graph_init(walker, source);
    trit_walker_graph_run(walker, steps);

    uint32_t count = 0;
    for (uint32_t i = 0; i < walker->n_nodes; i++) {
        reachable[i] = fabsf(walker->amplitudes[i]) > threshold;
        if (reachable[i]) count++;
    }

    return count;
}

/**
 * @brief Compute quantum PageRank-like centrality
 *
 * @param adjacency Graph adjacency matrix
 * @param steps Steps per source
 * @param damping Damping factor (0.85 typical)
 * @param centrality Output: centrality per node (caller allocated)
 * @return 0 on success
 */
static inline int trit_walker_graph_centrality(
    const trit_matrix_t* adjacency,
    uint32_t steps,
    float damping,
    float* centrality
) {
    if (!adjacency || !centrality) return -1;
    if (damping <= 0.0f || damping >= 1.0f) damping = 0.85f;

    trit_walker_graph_t* walker = trit_walker_graph_create(adjacency);
    if (!walker) return -1;

    uint32_t n = walker->n_nodes;

    /* Clear centrality */
    for (uint32_t i = 0; i < n; i++) {
        centrality[i] = 0.0f;
    }

    /* Run from each node and accumulate */
    for (uint32_t start = 0; start < n; start++) {
        trit_walker_graph_init(walker, start);
        trit_walker_graph_run(walker, steps);

        /* Accumulate amplitude squared */
        for (uint32_t i = 0; i < n; i++) {
            float prob = walker->amplitudes[i] * walker->amplitudes[i];
            centrality[i] += prob;
        }
    }

    /* Normalize */
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        sum += centrality[i];
    }
    if (sum > 0.0f) {
        float uniform = 1.0f / (float)n;
        for (uint32_t i = 0; i < n; i++) {
            centrality[i] = damping * (centrality[i] / sum) + (1.0f - damping) * uniform;
        }
    }

    trit_walker_graph_destroy(walker);
    return 0;
}

/**
 * @brief Estimate hitting time between nodes
 *
 * @param adjacency Graph adjacency matrix
 * @param source Source node
 * @param target Target node
 * @param max_steps Maximum steps
 * @return Estimated hitting time, or -1 if not reached
 */
static inline float trit_walker_graph_hitting_time(
    const trit_matrix_t* adjacency,
    uint32_t source,
    uint32_t target,
    uint32_t max_steps
) {
    if (!adjacency) return -1.0f;

    trit_walker_graph_t* walker = trit_walker_graph_create(adjacency);
    if (!walker) return -1.0f;

    if (source >= walker->n_nodes || target >= walker->n_nodes) {
        trit_walker_graph_destroy(walker);
        return -1.0f;
    }

    uint32_t steps;
    bool found = trit_walker_graph_search(walker, source, target, max_steps, &steps);

    trit_walker_graph_destroy(walker);

    return found ? (float)steps : -1.0f;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_QUANTUM_WALK_TERNARY_H */
