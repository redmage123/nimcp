//=============================================================================
// nimcp_quantum_annealing_ternary.h - Ternary Ising Quantum Annealing Bridge
//=============================================================================
/**
 * @file nimcp_quantum_annealing_ternary.h
 * @brief Integration of ternary Ising model with quantum annealing optimizer
 *
 * WHAT: Bridge between ternary spins and quantum annealing
 * WHY:  Memory-efficient quantum optimization using packed ternary states
 * HOW:  Convert between continuous and ternary, use ternary energy functions
 *
 * MEMORY COMPARISON:
 * | Representation | 1M spins | Notes                    |
 * |----------------|----------|--------------------------|
 * | float32        | 4 MB     | Standard representation  |
 * | Ternary packed | 200 KB   | 20x reduction            |
 *
 * INTEGRATION:
 * - Uses nimcp_quantum_ternary.h for ternary Ising model
 * - Uses nimcp_quantum_annealing.h for optimization engine
 * - Provides energy functions compatible with quantum_annealer_optimize()
 *
 * TYPICAL WORKFLOW:
 * ```c
 * // Create ternary Ising configuration
 * trit_ising_config_t* ising = trit_ising_create(1000, 0.1);
 *
 * // Create quantum annealer
 * quantum_annealing_config_t config = quantum_annealing_default_config();
 * quantum_annealer_t annealer = quantum_annealer_create(&config);
 *
 * // Run ternary quantum annealing
 * quantum_ternary_result_t result;
 * quantum_ternary_anneal(annealer, ising, J, h, &result);
 *
 * // Best configuration is now in ising->spins
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#ifndef NIMCP_QUANTUM_ANNEALING_TERNARY_H
#define NIMCP_QUANTUM_ANNEALING_TERNARY_H

#include "nimcp_quantum_annealing.h"
#include "nimcp_quantum_ternary.h"
#include "utils/ternary/nimcp_ternary.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Ternary quantum annealing configuration
 */
typedef struct {
    /* Annealing parameters */
    float initial_temperature;      /**< Starting temperature */
    float final_temperature;        /**< Ending temperature */
    uint32_t num_sweeps;            /**< Number of full sweeps */
    uint32_t spins_per_sweep;       /**< Spins to update per sweep (0 = all) */

    /* Quantum parameters */
    float initial_gamma;            /**< Initial transverse field strength */
    float final_gamma;              /**< Final transverse field (typically 0) */
    bool enable_tunneling;          /**< Enable quantum tunneling */

    /* Ternary parameters */
    float collapse_threshold;       /**< Coherence below which to collapse all */
    bool track_best;                /**< Track best configuration seen */

    /* Random seed */
    uint32_t seed;                  /**< RNG seed (0 = use time) */
} quantum_ternary_config_t;

/**
 * @brief Result of ternary quantum annealing
 */
typedef struct {
    double best_energy;             /**< Lowest energy found */
    double final_energy;            /**< Final configuration energy */
    uint32_t best_iteration;        /**< Iteration with best energy */
    uint32_t total_flips;           /**< Total spin flips accepted */
    uint32_t tunnel_events;         /**< Quantum tunneling events */
    float final_coherence;          /**< Final superposition fraction */
    bool converged;                 /**< Whether optimization converged */
} quantum_ternary_result_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Get default ternary quantum annealing configuration
 *
 * @return Default configuration
 */
static inline quantum_ternary_config_t quantum_ternary_default_config(void) {
    quantum_ternary_config_t config = {
        .initial_temperature = 5.0f,
        .final_temperature = 0.01f,
        .num_sweeps = 1000,
        .spins_per_sweep = 0,           /* All spins */
        .initial_gamma = 1.0f,
        .final_gamma = 0.0f,
        .enable_tunneling = true,
        .collapse_threshold = 0.01f,
        .track_best = true,
        .seed = 0
    };
    return config;
}

//=============================================================================
// Core Annealing Functions
//=============================================================================

/**
 * @brief Run ternary quantum annealing
 *
 * WHAT: Optimize Ising model using quantum-inspired annealing
 * WHY:  Find ground state of ternary spin system
 * HOW:  Metropolis updates + quantum tunneling + gradual collapse
 *
 * ALGORITHM:
 * 1. Initialize all spins to superposition
 * 2. For each sweep:
 *    a. Compute temperature T(t) and gamma(t)
 *    b. For selected spins:
 *       - If superposition: probabilistically measure
 *       - If measured: attempt flip with Metropolis + tunneling
 *    c. Track best configuration
 * 3. Collapse remaining superpositions
 * 4. Return best configuration
 *
 * @param ising Ternary Ising configuration (modified in place)
 * @param J Coupling matrix (symmetric, n_spins × n_spins)
 * @param h External field array (n_spins)
 * @param config Annealing configuration
 * @param result Output result statistics
 * @return 0 on success, error code on failure
 */
int quantum_ternary_anneal(
    trit_ising_config_t* ising,
    const float* J,
    const float* h,
    const quantum_ternary_config_t* config,
    quantum_ternary_result_t* result
);

/**
 * @brief Single sweep of ternary quantum annealing
 *
 * @param ising Ising configuration
 * @param J Coupling matrix
 * @param h External field
 * @param temperature Current temperature
 * @param gamma Current transverse field strength
 * @param rng Random state (modified)
 * @return Number of accepted moves
 */
uint32_t quantum_ternary_sweep(
    trit_ising_config_t* ising,
    const float* J,
    const float* h,
    float temperature,
    float gamma,
    uint64_t* rng
);

//=============================================================================
// Measurement and Collapse
//=============================================================================

/**
 * @brief Measure spin from superposition
 *
 * WHAT: Collapse superposition spin to definite state
 * WHY:  Model quantum measurement
 * HOW:  Use local field to bias measurement probability
 *
 * @param ising Ising configuration
 * @param J Coupling matrix
 * @param h External field
 * @param spin_idx Spin to measure
 * @param temperature Current temperature
 * @param random Random value [0,1)
 * @return Measurement result (+1 or -1)
 */
static inline trit_spin_t quantum_ternary_measure(
    trit_ising_config_t* ising,
    const float* J,
    const float* h,
    uint32_t spin_idx,
    float temperature,
    float random
) {
    if (!ising || spin_idx >= ising->n_spins) return TRIT_SPIN_DOWN;

    /* Compute local field */
    double field = trit_ising_local_field(ising, J, h, spin_idx);

    /* Boltzmann probability for spin up */
    float p_up = 0.5f;
    if (temperature > 1e-6f) {
        p_up = 1.0f / (1.0f + expf(-2.0f * (float)field / temperature));
    } else {
        p_up = (field >= 0) ? 1.0f : 0.0f;
    }

    trit_spin_t result = (random < p_up) ? TRIT_SPIN_UP : TRIT_SPIN_DOWN;
    trit_ising_measure(ising, spin_idx, result);

    return result;
}

/**
 * @brief Attempt Metropolis flip with quantum tunneling
 *
 * @param ising Ising configuration
 * @param J Coupling matrix
 * @param h External field
 * @param spin_idx Spin to flip
 * @param temperature Current temperature
 * @param gamma Transverse field strength
 * @param random Random value [0,1)
 * @return true if flip accepted
 */
static inline bool quantum_ternary_metropolis(
    trit_ising_config_t* ising,
    const float* J,
    const float* h,
    uint32_t spin_idx,
    float temperature,
    float gamma,
    float random
) {
    if (!ising || spin_idx >= ising->n_spins) return false;

    trit_spin_t current = trit_vector_get(ising->spins, spin_idx);
    if (current == TRIT_SPIN_SUPERPOSITION) return false;

    /* Compute energy change */
    double delta_E = trit_ising_delta_energy(ising, J, h, spin_idx);

    /* Classical acceptance probability */
    float p_accept = 1.0f;
    if (delta_E > 0 && temperature > 1e-6f) {
        p_accept = expf(-(float)delta_E / temperature);
    }

    /* Add quantum tunneling probability */
    if (gamma > 0.0f) {
        float p_tunnel = gamma / (1.0f + temperature);
        p_accept = p_accept + (1.0f - p_accept) * p_tunnel;
    }

    if (random < p_accept) {
        trit_ising_flip(ising, spin_idx);
        return true;
    }

    return false;
}

//=============================================================================
// Conversion Utilities
//=============================================================================

/**
 * @brief Convert continuous weights to ternary Ising
 *
 * @param weights Continuous weight array
 * @param n_weights Number of weights
 * @param threshold Quantization threshold
 * @param superposition_penalty Penalty for superposition spins
 * @return Ternary Ising configuration
 */
static inline trit_ising_config_t* quantum_ternary_from_weights(
    const float* weights,
    uint32_t n_weights,
    float threshold,
    float superposition_penalty
) {
    trit_ising_config_t* ising = trit_ising_create(n_weights, superposition_penalty);
    if (!ising || !weights) return ising;

    for (uint32_t i = 0; i < n_weights; i++) {
        trit_spin_t spin;
        if (weights[i] > threshold) {
            spin = TRIT_SPIN_UP;
        } else if (weights[i] < -threshold) {
            spin = TRIT_SPIN_DOWN;
        } else {
            spin = TRIT_SPIN_SUPERPOSITION;
        }
        trit_ising_measure(ising, i, spin);
    }

    return ising;
}

/**
 * @brief Convert ternary Ising to continuous weights
 *
 * @param ising Ising configuration
 * @param weights Output weight array (caller allocated)
 * @param scale Scaling factor for weights
 */
static inline void quantum_ternary_to_weights(
    const trit_ising_config_t* ising,
    float* weights,
    float scale
) {
    if (!ising || !weights) return;

    for (uint32_t i = 0; i < ising->n_spins; i++) {
        trit_spin_t spin = trit_vector_get(ising->spins, i);
        weights[i] = (float)spin * scale;
    }
}

//=============================================================================
// Energy Function Wrapper
//=============================================================================

/**
 * @brief Context for ternary Ising energy function
 */
typedef struct {
    trit_ising_config_t* ising;
    const float* J;
    const float* h;
    float threshold;
} ternary_energy_context_t;

/**
 * @brief Energy function for quantum_annealer_optimize()
 *
 * WHAT: Wraps ternary Ising energy for continuous optimizer
 * WHY:  Allow using ternary model with standard annealer
 * HOW:  Convert continuous state to ternary, compute energy
 *
 * @param state Continuous state vector
 * @param dim State dimension
 * @param user_data ternary_energy_context_t pointer
 * @return Energy value
 */
static inline float ternary_ising_energy_wrapper(
    const float* state,
    uint32_t dim,
    void* user_data
) {
    ternary_energy_context_t* ctx = (ternary_energy_context_t*)user_data;
    if (!ctx || !ctx->ising || dim != ctx->ising->n_spins) {
        return 1e30f;  /* Invalid */
    }

    /* Convert continuous to ternary */
    for (uint32_t i = 0; i < dim; i++) {
        trit_spin_t spin;
        if (state[i] > ctx->threshold) {
            spin = TRIT_SPIN_UP;
        } else if (state[i] < -ctx->threshold) {
            spin = TRIT_SPIN_DOWN;
        } else {
            spin = TRIT_SPIN_SUPERPOSITION;
        }
        trit_vector_set(ctx->ising->spins, i, spin);

        /* Update counters */
        if (spin == TRIT_SPIN_SUPERPOSITION) {
            ctx->ising->n_superposition++;
        } else {
            ctx->ising->n_measured++;
        }
    }

    return (float)trit_ising_energy(ctx->ising, ctx->J, ctx->h);
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_QUANTUM_ANNEALING_TERNARY_H */
