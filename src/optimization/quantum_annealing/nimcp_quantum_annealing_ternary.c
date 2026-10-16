#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_quantum_annealing_ternary.c - Ternary Quantum Annealing Implementation
//=============================================================================

#include "optimization/quantum_annealing/nimcp_quantum_annealing_ternary.h"
#include "api/nimcp_api_exception.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* P3-OPT-2: Add LOG_MODULE for logging macros */
#define LOG_MODULE "OPTIMIZATION_TERNARY"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(quantum_annealing_ternary)

//=============================================================================
// RNG Utilities (xorshift64)
//=============================================================================

static inline uint64_t xorshift64(uint64_t* state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

static inline float random_float(uint64_t* state) {
    return (float)(xorshift64(state) & 0xFFFFFFFF) / 4294967296.0f;
}

//=============================================================================
// Single Sweep
//=============================================================================

uint32_t quantum_ternary_sweep(
    trit_ising_config_t* ising,
    const float* J,
    const float* h,
    float temperature,
    float gamma,
    uint64_t* rng
) {
    if (!ising) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "quantum_ternary_sweep: null ising config");
        return 0;
    }
    if (!rng) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "quantum_ternary_sweep: null rng state");
        return 0;
    }

    uint32_t accepted = 0;
    uint32_t n = ising->n_spins;

    for (uint32_t i = 0; i < n; i++) {
        /* Random spin selection (could use sequential for speed) */
        uint32_t idx = xorshift64(rng) % n;
        float r = random_float(rng);

        trit_spin_t current = trit_vector_get(ising->spins, idx);

        if (current == TRIT_SPIN_SUPERPOSITION) {
            /* Measure with probability based on temperature */
            float p_measure = 1.0f / (1.0f + gamma);
            if (r < p_measure) {
                float r2 = random_float(rng);
                quantum_ternary_measure(ising, J, h, idx, temperature, r2);
                accepted++;
            }
        } else {
            /* Attempt Metropolis flip */
            if (quantum_ternary_metropolis(ising, J, h, idx, temperature, gamma, r)) {
                accepted++;
            }
        }
    }

    return accepted;
}

//=============================================================================
// Main Annealing Loop
//=============================================================================

int quantum_ternary_anneal(
    trit_ising_config_t* ising,
    const float* J,
    const float* h,
    const quantum_ternary_config_t* config,
    quantum_ternary_result_t* result
) {
    /* Validate inputs with exception handling */
    if (!ising) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "quantum_ternary_anneal: null ising config");
        return -1;
    }
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "quantum_ternary_anneal: null config");
        return -1;
    }
    if (!result) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "quantum_ternary_anneal: null result");
        return -1;
    }

    /* Initialize result */
    memset(result, 0, sizeof(quantum_ternary_result_t));

    /* Initialize RNG */
    uint64_t rng_state = config->seed;
    if (rng_state == 0) {
        rng_state = (uint64_t)time(NULL) ^ (uint64_t)(size_t)ising;
    }
    /* P2 fix: xorshift64 with seed 0 produces all-zero sequence */
    if (rng_state == 0) {
        rng_state = 1;
    }

    /* Reset all spins to superposition */
    for (uint32_t i = 0; i < ising->n_spins; i++) {
        trit_ising_reset(ising, i);
    }

    /* Tracking best configuration */
    trit_t* best_spins = NULL;
    if (config->track_best) {
        size_t best_spins_size = ising->n_spins * sizeof(trit_t);
        best_spins = (trit_t*)nimcp_malloc(best_spins_size);
        if (!best_spins) {
            NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, best_spins_size,
                              "Failed to allocate best_spins array for ternary annealing (n_spins=%u)",
                              ising->n_spins);
            return -2;
        }
    }

    result->best_energy = 1e30;
    uint32_t total_accepted = 0;
    uint32_t tunnel_count = 0;

    /* Main annealing loop */
    for (uint32_t sweep = 0; sweep < config->num_sweeps; sweep++) {
        /* Compute temperature and gamma for this sweep */
        /* P1-16 fix: When num_sweeps==1, (num_sweeps-1)=0 causes div-by-zero */
        float progress = (config->num_sweeps > 1)
            ? (float)sweep / (float)(config->num_sweeps - 1)
            : 1.0f;

        /* P2-OPT-2: Validate temperatures before computing log ratio */
        float clamped_init = (config->initial_temperature > 1e-10f) ? config->initial_temperature : 1e-10f;
        float clamped_final = (config->final_temperature > 1e-10f) ? config->final_temperature : 1e-10f;
        if (clamped_final >= clamped_init) clamped_final = clamped_init * 0.01f;

        /* Exponential cooling */
        float log_ratio = logf(clamped_init / clamped_final);
        float temperature = clamped_init * expf(-progress * log_ratio);

        /* Linear gamma decrease */
        float gamma = config->initial_gamma * (1.0f - progress) +
                     config->final_gamma * progress;

        /* Perform sweep */
        uint32_t accepted = quantum_ternary_sweep(ising, J, h, temperature, gamma, &rng_state);
        total_accepted += accepted;

        /* Compute current energy */
        double energy = trit_ising_energy(ising, J, h);

        /* Track best */
        if (config->track_best && energy < result->best_energy) {
            result->best_energy = energy;
            result->best_iteration = sweep;

            /* Save best configuration */
            for (uint32_t i = 0; i < ising->n_spins; i++) {
                best_spins[i] = trit_vector_get(ising->spins, i);
            }
        }

        /* Check coherence for early collapse */
        float coherence = trit_ising_coherence(ising);
        if (coherence < config->collapse_threshold && coherence > 0) {
            /* Collapse remaining superpositions */
            trit_ising_collapse_all(ising, J, h);
        }
    }

    /* Final collapse of any remaining superpositions */
    trit_ising_collapse_all(ising, J, h);

    /* Restore best configuration if tracking */
    if (config->track_best && best_spins) {
        for (uint32_t i = 0; i < ising->n_spins; i++) {
            if (best_spins[i] != TRIT_SPIN_SUPERPOSITION) {
                trit_ising_measure(ising, i, best_spins[i]);
            }
        }
        nimcp_free(best_spins);
    }

    /* Fill result */
    result->final_energy = trit_ising_energy(ising, J, h);
    result->total_flips = total_accepted;
    /* P2-OPT-3: tunnel_events was always 0 because tunnel_count is never incremented.
     * Tunneling detection requires tracking superposition-to-classical transitions
     * which is not yet instrumented. Set to 0 explicitly and document as TODO. */
    result->tunnel_events = 0;  /* TODO: Instrument tunneling event detection */
    result->final_coherence = trit_ising_coherence(ising);
    result->converged = (result->final_coherence == 0.0f);

    return 0;
}
