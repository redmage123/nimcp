//=============================================================================
// nimcp_quantum_ternary.h - Ternary Representation for Quantum Algorithms
//=============================================================================
/**
 * @file nimcp_quantum_ternary.h
 * @brief Ternary spin states for quantum-inspired optimization
 *
 * WHAT: Three-state spins {DOWN, SUPERPOSITION, UP} for quantum annealing
 * WHY:  Model quantum superposition explicitly in classical simulation
 * HOW:  Extend Ising model to include unmeasured/superposition states
 *
 * QUANTUM-TERNARY MAPPING:
 * | Quantum State     | Ternary | Meaning                    |
 * |-------------------|---------|----------------------------|
 * | |↓⟩ (measured)    | -1      | Spin down, definite state  |
 * | Superposition     | 0       | Unmeasured, superposition  |
 * | |↑⟩ (measured)    | +1      | Spin up, definite state    |
 *
 * ISING MODEL EXTENSION:
 * - Traditional Ising: σᵢ ∈ {-1, +1}
 * - Ternary Ising: σᵢ ∈ {-1, 0, +1}
 * - 0 represents "quantum uncertainty" before measurement
 *
 * ENERGY CALCULATION:
 * - Classical: E = -Σ Jᵢⱼ σᵢ σⱼ - Σ hᵢ σᵢ
 * - Ternary: Same formula, but 0 spins contribute 0 to energy
 * - Superposition penalty: Add term to favor definite states
 *
 * MEMORY EFFICIENCY:
 * - 1M spins (float): 4 MB
 * - 1M spins (int8 Ising): 1 MB
 * - 1M spins (ternary packed): 200 KB (5x savings over int8)
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#ifndef NIMCP_QUANTUM_TERNARY_H
#define NIMCP_QUANTUM_TERNARY_H

#include "utils/ternary/nimcp_ternary.h"
#include <math.h>
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Quantum Ternary Types
//=============================================================================

/**
 * @brief Ternary spin value
 */
typedef trit_t trit_spin_t;

#define TRIT_SPIN_DOWN         TRIT_NEGATIVE  /**< |↓⟩ measured spin */
#define TRIT_SPIN_SUPERPOSITION TRIT_UNKNOWN  /**< Unmeasured superposition */
#define TRIT_SPIN_UP           TRIT_POSITIVE  /**< |↑⟩ measured spin */

/**
 * @brief Ternary spin configuration for Ising model
 */
typedef struct {
    uint32_t magic;             /**< Validation magic */
    uint32_t n_spins;           /**< Number of spins */
    trit_vector_t* spins;       /**< Spin states */

    /* Coupling matrix (optional, for dense problems) */
    trit_matrix_t* J;           /**< Ternary couplings (optional) */

    /* Superposition tracking */
    uint32_t n_measured;        /**< Count of measured spins */
    uint32_t n_superposition;   /**< Count of superposition spins */

    /* Energy tracking */
    double energy;              /**< Current energy */
    double superposition_penalty; /**< Penalty per superposition spin */
} trit_ising_config_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create ternary Ising configuration
 *
 * @param n_spins Number of spins
 * @param superposition_penalty Energy penalty per superposition spin
 * @return New configuration, or NULL on failure
 */
static inline trit_ising_config_t* trit_ising_create(
    uint32_t n_spins,
    double superposition_penalty
) {
    trit_ising_config_t* config = (trit_ising_config_t*)
        nimcp_calloc(1, sizeof(trit_ising_config_t));
    if (!config) return NULL;

    config->magic = TERNARY_MAGIC;
    config->n_spins = n_spins;
    config->superposition_penalty = superposition_penalty;

    /* Initialize all spins to superposition (unmeasured) */
    config->spins = trit_vector_create_filled(n_spins, TRIT_SPIN_SUPERPOSITION,
                                               TERNARY_PACK_BASE243);
    if (!config->spins) {
        nimcp_free(config);
        return NULL;
    }

    config->n_measured = 0;
    config->n_superposition = n_spins;
    config->energy = n_spins * superposition_penalty;

    return config;
}

/**
 * @brief Destroy ternary Ising configuration
 *
 * @param config Configuration to destroy
 */
static inline void trit_ising_destroy(trit_ising_config_t* config) {
    if (!config) return;
    if (config->spins) trit_vector_destroy(config->spins);
    if (config->J) trit_matrix_destroy(config->J);
    config->magic = 0;
    nimcp_free(config);
}

//=============================================================================
// Spin Operations
//=============================================================================

/**
 * @brief Measure a spin (collapse superposition)
 *
 * WHAT: Collapse spin from superposition to definite state
 * WHY:  Model quantum measurement
 * HOW:  Use probability or random choice
 *
 * @param config Ising configuration
 * @param spin_idx Spin index
 * @param result Measurement result (+1 or -1)
 * @return Previous spin state
 */
static inline trit_spin_t trit_ising_measure(
    trit_ising_config_t* config,
    uint32_t spin_idx,
    trit_spin_t result
) {
    if (!config || spin_idx >= config->n_spins) return TRIT_SPIN_SUPERPOSITION;

    trit_spin_t old = trit_vector_get(config->spins, spin_idx);

    /* Only valid results are UP or DOWN */
    if (result != TRIT_SPIN_UP && result != TRIT_SPIN_DOWN) {
        result = TRIT_SPIN_UP;  /* Default to up if invalid */
    }

    trit_vector_set(config->spins, spin_idx, result);

    /* Update counters */
    if (old == TRIT_SPIN_SUPERPOSITION) {
        config->n_superposition--;
        config->n_measured++;
    }

    return old;
}

/**
 * @brief Reset spin to superposition
 *
 * @param config Ising configuration
 * @param spin_idx Spin index
 * @return Previous spin state
 */
static inline trit_spin_t trit_ising_reset(
    trit_ising_config_t* config,
    uint32_t spin_idx
) {
    if (!config || spin_idx >= config->n_spins) return TRIT_SPIN_SUPERPOSITION;

    trit_spin_t old = trit_vector_get(config->spins, spin_idx);
    trit_vector_set(config->spins, spin_idx, TRIT_SPIN_SUPERPOSITION);

    /* Update counters */
    if (old != TRIT_SPIN_SUPERPOSITION) {
        config->n_superposition++;
        config->n_measured--;
    }

    return old;
}

/**
 * @brief Flip a measured spin
 *
 * @param config Ising configuration
 * @param spin_idx Spin index
 */
static inline void trit_ising_flip(
    trit_ising_config_t* config,
    uint32_t spin_idx
) {
    if (!config || spin_idx >= config->n_spins) return;

    trit_spin_t current = trit_vector_get(config->spins, spin_idx);
    if (current == TRIT_SPIN_UP) {
        trit_vector_set(config->spins, spin_idx, TRIT_SPIN_DOWN);
    } else if (current == TRIT_SPIN_DOWN) {
        trit_vector_set(config->spins, spin_idx, TRIT_SPIN_UP);
    }
    /* Superposition spins don't flip */
}

//=============================================================================
// Energy Calculation
//=============================================================================

/**
 * @brief Compute local field on a spin
 *
 * @param config Ising configuration
 * @param J Coupling matrix (n_spins x n_spins, float)
 * @param h External field array (n_spins floats)
 * @param spin_idx Spin index
 * @return Local field value
 */
static inline double trit_ising_local_field(
    const trit_ising_config_t* config,
    const float* J,
    const float* h,
    uint32_t spin_idx
) {
    if (!config || spin_idx >= config->n_spins) return 0.0;

    double field = (h) ? h[spin_idx] : 0.0;

    if (!J) return field;

    for (uint32_t j = 0; j < config->n_spins; j++) {
        if (j == spin_idx) continue;
        trit_spin_t sj = trit_vector_get(config->spins, j);
        if (sj != TRIT_SPIN_SUPERPOSITION) {
            field += J[spin_idx * config->n_spins + j] * (double)sj;
        }
    }

    return field;
}

/**
 * @brief Compute total energy of configuration
 *
 * @param config Ising configuration
 * @param J Coupling matrix (n_spins x n_spins, float, symmetric)
 * @param h External field array (n_spins floats)
 * @return Total energy
 */
static inline double trit_ising_energy(
    const trit_ising_config_t* config,
    const float* J,
    const float* h
) {
    if (!config) return 0.0;

    double E = 0.0;

    /* Coupling energy: -Σᵢⱼ Jᵢⱼ σᵢ σⱼ (only for i < j to avoid double counting) */
    if (J) {
        for (uint32_t i = 0; i < config->n_spins; i++) {
            trit_spin_t si = trit_vector_get(config->spins, i);
            if (si == TRIT_SPIN_SUPERPOSITION) continue;

            for (uint32_t j = i + 1; j < config->n_spins; j++) {
                trit_spin_t sj = trit_vector_get(config->spins, j);
                if (sj == TRIT_SPIN_SUPERPOSITION) continue;

                E -= J[i * config->n_spins + j] * (double)si * (double)sj;
            }
        }
    }

    /* Field energy: -Σᵢ hᵢ σᵢ */
    if (h) {
        for (uint32_t i = 0; i < config->n_spins; i++) {
            trit_spin_t si = trit_vector_get(config->spins, i);
            if (si != TRIT_SPIN_SUPERPOSITION) {
                E -= h[i] * (double)si;
            }
        }
    }

    /* Superposition penalty */
    E += config->n_superposition * config->superposition_penalty;

    return E;
}

/**
 * @brief Compute energy change from flipping a spin
 *
 * @param config Ising configuration
 * @param J Coupling matrix
 * @param h External field
 * @param spin_idx Spin to flip
 * @return Energy change (ΔE)
 */
static inline double trit_ising_delta_energy(
    const trit_ising_config_t* config,
    const float* J,
    const float* h,
    uint32_t spin_idx
) {
    if (!config || spin_idx >= config->n_spins) return 0.0;

    trit_spin_t current = trit_vector_get(config->spins, spin_idx);
    if (current == TRIT_SPIN_SUPERPOSITION) {
        return 0.0;  /* Can't compute delta for superposition */
    }

    /* ΔE = 2 σᵢ (Σⱼ Jᵢⱼ σⱼ + hᵢ) */
    double local = trit_ising_local_field(config, J, h, spin_idx);
    return 2.0 * (double)current * local;
}

//=============================================================================
// Quantum-Inspired Operations
//=============================================================================

/**
 * @brief Compute superposition fraction (quantum coherence proxy)
 *
 * @param config Ising configuration
 * @return Fraction of spins in superposition [0,1]
 */
static inline float trit_ising_coherence(const trit_ising_config_t* config) {
    if (!config || config->n_spins == 0) return 0.0f;
    return (float)config->n_superposition / (float)config->n_spins;
}

/**
 * @brief Collapse all spins to definite states
 *
 * WHAT: Measure all superposition spins
 * WHY:  End of annealing, get final configuration
 * HOW:  Use local field to decide spin direction
 *
 * @param config Ising configuration
 * @param J Coupling matrix
 * @param h External field
 */
static inline void trit_ising_collapse_all(
    trit_ising_config_t* config,
    const float* J,
    const float* h
) {
    if (!config) return;

    for (uint32_t i = 0; i < config->n_spins; i++) {
        if (trit_vector_get(config->spins, i) == TRIT_SPIN_SUPERPOSITION) {
            double field = trit_ising_local_field(config, J, h, i);
            trit_spin_t result = (field >= 0) ? TRIT_SPIN_UP : TRIT_SPIN_DOWN;
            trit_ising_measure(config, i, result);
        }
    }
}

/**
 * @brief Apply tunneling probability to spin
 *
 * WHAT: Quantum tunneling effect for escaping local minima
 * WHY:  Core of quantum annealing
 * HOW:  Probabilistic flip based on barrier height
 *
 * @param config Ising configuration
 * @param spin_idx Spin to potentially tunnel
 * @param gamma Tunneling strength (quantum field)
 * @param temperature Current temperature
 * @param random Random value [0,1)
 * @return true if tunneling occurred
 */
static inline bool trit_ising_tunnel(
    trit_ising_config_t* config,
    uint32_t spin_idx,
    float gamma,
    float temperature,
    float random
) {
    if (!config || spin_idx >= config->n_spins) return false;

    trit_spin_t current = trit_vector_get(config->spins, spin_idx);
    if (current == TRIT_SPIN_SUPERPOSITION) return false;

    /* Tunneling probability: P = gamma * exp(-barrier/T) */
    /* For simplicity, use gamma directly as probability */
    float tunnel_prob = gamma / (1.0f + temperature);

    if (random < tunnel_prob) {
        trit_ising_flip(config, spin_idx);
        return true;
    }

    return false;
}

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Ising configuration statistics
 */
typedef struct {
    uint32_t n_up;          /**< Count of UP spins */
    uint32_t n_down;        /**< Count of DOWN spins */
    uint32_t n_superposition;/**< Count of superposition spins */
    float magnetization;    /**< Average spin (m = Σσᵢ/N) */
    float coherence;        /**< Superposition fraction */
    double energy;          /**< Current energy */
} trit_ising_stats_t;

/**
 * @brief Get Ising configuration statistics
 *
 * @param config Ising configuration
 * @param stats Output statistics
 */
static inline void trit_ising_get_stats(
    const trit_ising_config_t* config,
    trit_ising_stats_t* stats
) {
    if (!stats) return;

    stats->n_up = 0;
    stats->n_down = 0;
    stats->n_superposition = 0;
    stats->magnetization = 0.0f;
    stats->coherence = 0.0f;
    stats->energy = 0.0;

    if (!config || !config->spins) return;

    int sum = 0;
    for (uint32_t i = 0; i < config->n_spins; i++) {
        trit_spin_t s = trit_vector_get(config->spins, i);
        if (s == TRIT_SPIN_UP) {
            stats->n_up++;
            sum += 1;
        } else if (s == TRIT_SPIN_DOWN) {
            stats->n_down++;
            sum -= 1;
        } else {
            stats->n_superposition++;
        }
    }

    uint32_t n_measured = stats->n_up + stats->n_down;
    if (n_measured > 0) {
        stats->magnetization = (float)sum / (float)config->n_spins;
    }

    stats->coherence = (float)stats->n_superposition / (float)config->n_spins;
    stats->energy = config->energy;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_QUANTUM_TERNARY_H */
