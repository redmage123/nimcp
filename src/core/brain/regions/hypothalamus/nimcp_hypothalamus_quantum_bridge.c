/**
 * @file nimcp_hypothalamus_quantum_bridge.c
 * @brief Implementation of quantum bridge for hypothalamus integration
 *
 * WHAT: Bridge connecting hypothalamus to quantum reasoning system
 * WHY:  Enable quantum-accelerated homeostatic optimization
 * HOW:  Uses quantum annealing for multi-objective optimization
 *
 * @version Phase H1: Hypothalamus Brain Integration
 * @date 2025-12-30
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_quantum_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define LOG_MODULE "HYPOTHALAMUS_QUANTUM"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_quantum_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * MATHEMATICAL CONSTANTS
 *===========================================================================*/


/*=============================================================================
 * INTERNAL STRUCTURE
 *===========================================================================*/

struct hypothalamus_quantum_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    hypothalamus_quantum_config_t config;

    /* Connected hypothalamus */
    hypothalamus_adapter_t* hypothalamus;

    /* QUBO formulation */
    homeostatic_qubo_t qubo;

    /* Cached optimization results */
    optimization_result_t cached_result;
    bool cache_valid;
    uint64_t cache_timestamp_us;

    /* Timing */
    uint64_t last_update_us;

    /* Statistics */
    hypothalamus_quantum_stats_t stats;

    /* Random state for classical fallback */
    uint32_t rng_state;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Simple pseudo-random number generator
 */
static float random_float(hypothalamus_quantum_bridge_t* bridge) {
    bridge->rng_state = bridge->rng_state * 1103515245 + 12345;
    return (float)(bridge->rng_state & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

/**
 * @brief Initialize QUBO matrix
 */
static int init_qubo_matrix(homeostatic_qubo_t* qubo, uint32_t size) {
    if (!qubo) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qubo is NULL");

        return -1;

    }

    qubo->qubo_size = size;

    /* Allocate upper triangular matrix (size * (size + 1) / 2 elements) */
    size_t matrix_elements = (size * (size + 1)) / 2;
    qubo->qubo_matrix = (float*)nimcp_calloc(matrix_elements, sizeof(float));
    if (!qubo->qubo_matrix) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate QUBO matrix");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_qubo_matrix: qubo->qubo_matrix is NULL");
        return -1;
    }

    /* Allocate linear terms */
    qubo->linear_terms = (float*)nimcp_calloc(size, sizeof(float));
    if (!qubo->linear_terms) {
        nimcp_free(qubo->qubo_matrix);
        qubo->qubo_matrix = NULL;
        LOG_ERROR(LOG_MODULE, "Failed to allocate linear terms");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "init_qubo_matrix: qubo->linear_terms is NULL");
        return -1;
    }

    qubo->offset = 0.0f;
    return 0;
}

/**
 * @brief Free QUBO matrix
 */
static void free_qubo_matrix(homeostatic_qubo_t* qubo) {
    if (!qubo) return;

    if (qubo->qubo_matrix) {
        nimcp_free(qubo->qubo_matrix);
        qubo->qubo_matrix = NULL;
    }

    if (qubo->linear_terms) {
        nimcp_free(qubo->linear_terms);
        qubo->linear_terms = NULL;
    }

    qubo->qubo_size = 0;
}

/**
 * @brief Get QUBO matrix index for (i, j) where i <= j
 */
static size_t qubo_index(uint32_t i, uint32_t j, uint32_t size) {
    if (i > j) {
        uint32_t temp = i;
        i = j;
        j = temp;
    }
    return (size_t)i * size - (i * (i + 1)) / 2 + j;
}

/**
 * @brief Formulate QUBO from current state and objectives
 */
static int formulate_qubo(hypothalamus_quantum_bridge_t* bridge,
                           const homeostatic_objective_t* objective,
                           const regulatory_constraint_t* constraints) {
    if (!bridge || !objective) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qubo_index: required parameter is NULL (bridge, objective)");
        return -1;
    }

    homeostatic_qubo_t* qubo = &bridge->qubo;
    uint32_t n = qubo->qubo_size;

    /* Clear existing values */
    size_t matrix_elements = (n * (n + 1)) / 2;
    memset(qubo->qubo_matrix, 0, matrix_elements * sizeof(float));
    memset(qubo->linear_terms, 0, n * sizeof(float));

    /* Get current state from hypothalamus */
    hypothalamus_state_t state;
    if (!hypothalamus_get_state(bridge->hypothalamus, &state)) {
        LOG_WARNING(LOG_MODULE, "Could not get hypothalamus state");
    }

    qubo->current_temperature = state.thermoregulation.core_temp.current_value;
    qubo->current_glucose = state.appetite.blood_glucose.current_value;
    qubo->current_osmolality = state.hydration.osmolality.current_value;
    qubo->current_cortisol = state.hpa_axis.cortisol_level;
    qubo->current_sympathetic = state.autonomic.sympathetic_tone;
    qubo->current_parasympathetic = state.autonomic.parasympathetic_tone;

    /* Copy objective and constraints */
    qubo->objective = *objective;
    if (constraints) {
        qubo->constraints = *constraints;
    }

    /*
     * QUBO formulation:
     * We encode regulatory actions as binary variables.
     *
     * Variables (using n=8 qubits):
     * - Bit 0-1: Heat production level (2 bits, 4 levels)
     * - Bit 2-3: Sympathetic adjustment (2 bits, 4 levels)
     * - Bit 4-5: CRH release (2 bits, 4 levels)
     * - Bit 6-7: Circadian shift (2 bits, 4 levels)
     *
     * The QUBO objective minimizes:
     * E(x) = sum_i h_i * x_i + sum_{i<j} Q_{ij} * x_i * x_j
     *
     * Subject to homeostatic deviation penalties.
     */

    /* Linear terms (diagonal): penalties for deviation from targets */

    /* Temperature deviation penalty */
    float temp_error = qubo->current_temperature - objective->temperature_target;
    qubo->linear_terms[0] += objective->temperature_weight * fabsf(temp_error);
    qubo->linear_terms[1] += objective->temperature_weight * fabsf(temp_error) * 0.5f;

    /* Glucose deviation penalty */
    float glucose_error = qubo->current_glucose - objective->glucose_target;
    /* Normalized glucose error */
    float normalized_glucose_error = glucose_error / 100.0f;
    qubo->linear_terms[2] += objective->glucose_weight * fabsf(normalized_glucose_error);
    qubo->linear_terms[3] += objective->glucose_weight * fabsf(normalized_glucose_error) * 0.5f;

    /* Stress minimization */
    qubo->linear_terms[4] += objective->stress_weight * qubo->current_cortisol;
    qubo->linear_terms[5] += objective->stress_weight * qubo->current_cortisol * 0.3f;

    /* Energy expenditure penalty */
    qubo->linear_terms[6] += objective->energy_weight * 0.5f;
    qubo->linear_terms[7] += objective->energy_weight * 0.25f;

    /* Quadratic coupling terms (off-diagonal): interactions between actions */

    /* Heat production and sympathetic tone are positively coupled */
    qubo->qubo_matrix[qubo_index(0, 2, n)] = -0.2f;
    qubo->qubo_matrix[qubo_index(1, 3, n)] = -0.1f;

    /* CRH and sympathetic are positively coupled (stress response) */
    qubo->qubo_matrix[qubo_index(2, 4, n)] = -0.15f;
    qubo->qubo_matrix[qubo_index(3, 5, n)] = -0.1f;

    /* Circadian shift and CRH are negatively coupled (jet lag stress) */
    qubo->qubo_matrix[qubo_index(4, 6, n)] = 0.1f;
    qubo->qubo_matrix[qubo_index(5, 7, n)] = 0.05f;

    /* Apply constraint penalties if provided */
    if (constraints) {
        /* Temperature constraint penalties */
        if (constraints->min_temperature > 0.0f || constraints->max_temperature > 0.0f) {
            float penalty = 10.0f;  /* Large penalty for constraint violation */

            /* Add penalty terms to discourage extreme actions */
            qubo->linear_terms[0] += penalty * 0.1f;
            qubo->linear_terms[1] += penalty * 0.1f;
        }
    }

    /* Calculate offset (constant term) */
    qubo->offset = objective->temperature_weight * temp_error * temp_error * 0.01f +
                   objective->glucose_weight * normalized_glucose_error * normalized_glucose_error;

    LOG_DEBUG(LOG_MODULE, "QUBO formulated: %u variables, offset=%.4f",
              n, qubo->offset);

    return 0;
}

/**
 * @brief Simulated annealing solver (classical fallback)
 */
static int solve_qubo_simulated_annealing(
    hypothalamus_quantum_bridge_t* bridge,
    optimization_result_t* result) {

    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "qubo_index: required parameter is NULL (bridge, result)");
        return -1;
    }

    homeostatic_qubo_t* qubo = &bridge->qubo;
    uint32_t n = qubo->qubo_size;

    /* Initialize random binary solution */
    uint8_t* solution = (uint8_t*)nimcp_calloc(n, sizeof(uint8_t));
    uint8_t* best_solution = (uint8_t*)nimcp_calloc(n, sizeof(uint8_t));
    if (!solution || !best_solution) {
        if (solution) nimcp_free(solution);
        if (best_solution) nimcp_free(best_solution);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "solve_qubo_simulated_annealing: allocation failed");
        return -1;
    }

    /* Random initial solution */
    for (uint32_t i = 0; i < n; i++) {
        solution[i] = random_float(bridge) > 0.5f ? 1 : 0;
        best_solution[i] = solution[i];
    }

    /* Calculate initial energy */
    float energy = qubo->offset;
    for (uint32_t i = 0; i < n; i++) {
        if (solution[i]) {
            energy += qubo->linear_terms[i];
            for (uint32_t j = i; j < n; j++) {
                if (solution[j]) {
                    energy += qubo->qubo_matrix[qubo_index(i, j, n)];
                }
            }
        }
    }

    float best_energy = energy;
    float temperature = bridge->config.initial_temperature;
    float cooling_rate = powf(bridge->config.final_temperature /
                              bridge->config.initial_temperature,
                              1.0f / bridge->config.max_iterations);

    /* Simulated annealing loop */
    uint32_t iterations = 0;
    for (; iterations < bridge->config.max_iterations; iterations++) {
        /* Select random bit to flip */
        uint32_t flip_bit = (uint32_t)(random_float(bridge) * n) % n;

        /* Calculate energy change from flip */
        float delta_e = 0.0f;

        /* Linear term change */
        delta_e += solution[flip_bit] ? -qubo->linear_terms[flip_bit]
                                      : qubo->linear_terms[flip_bit];

        /* Quadratic term changes */
        for (uint32_t j = 0; j < n; j++) {
            if (j != flip_bit && solution[j]) {
                float q = qubo->qubo_matrix[qubo_index(flip_bit, j, n)];
                delta_e += solution[flip_bit] ? -q : q;
            }
        }

        /* Accept or reject flip */
        bool accept = false;
        if (delta_e < 0) {
            accept = true;
        } else {
            float p = expf(-delta_e / temperature);
            accept = random_float(bridge) < p;
        }

        if (accept) {
            solution[flip_bit] = 1 - solution[flip_bit];
            energy += delta_e;

            if (energy < best_energy) {
                best_energy = energy;
                memcpy(best_solution, solution, n * sizeof(uint8_t));
            }
        }

        /* Cool down */
        temperature *= cooling_rate;

        /* Check convergence */
        if (temperature < bridge->config.final_temperature) {
            break;
        }
    }

    /* Decode solution to regulatory actions */
    /* Bits 0-1: Heat production (0-3 mapped to 0.25-1.0) */
    uint32_t heat_bits = (best_solution[0] << 1) | best_solution[1];
    result->optimal_heat_production = 0.25f + (float)heat_bits * 0.25f;

    /* Bits 2-3: Sympathetic adjustment (-0.2 to +0.2) */
    uint32_t symp_bits = (best_solution[2] << 1) | best_solution[3];
    result->optimal_sympathetic_change = -0.2f + (float)symp_bits * 0.133f;

    /* Bits 4-5: CRH release (0-0.6) */
    uint32_t crh_bits = (best_solution[4] << 1) | best_solution[5];
    result->optimal_crh_release = (float)crh_bits * 0.2f;

    /* Bits 6-7: Circadian shift (-0.2 to +0.2 radians) */
    uint32_t circ_bits = (best_solution[6] << 1) | best_solution[7];
    result->optimal_circadian_shift = -0.2f + (float)circ_bits * 0.133f;

    /* Reciprocal parasympathetic adjustment */
    result->optimal_parasympathetic_change = -result->optimal_sympathetic_change * 0.5f;

    /* Heat loss is inverse of production (simplified) */
    result->optimal_heat_loss = 1.0f - result->optimal_heat_production * 0.5f;

    /* Fill result metadata */
    result->energy_value = best_energy;
    result->iterations_used = iterations;
    result->converged = iterations < bridge->config.max_iterations;
    result->constraint_violation = 0.0f;  /* TODO: calculate actual violation */
    result->quantum_contribution = 0.0f;  /* Classical solver */

    /* Cleanup */
    nimcp_free(solution);
    nimcp_free(best_solution);

    LOG_DEBUG(LOG_MODULE, "SA solved: energy=%.4f, iterations=%u",
              best_energy, iterations);

    return 0;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

hypothalamus_quantum_config_t hypothalamus_quantum_default_config(void) {
    hypothalamus_quantum_config_t config;
    memset(&config, 0, sizeof(config));

    config.mode = HYPOTHALAMUS_QUANTUM_MODE_FULL;
    config.num_qubits = HYPOTHALAMUS_QUANTUM_DEFAULT_QUBITS;
    config.circuit_depth = HYPOTHALAMUS_QUANTUM_DEFAULT_DEPTH;
    config.max_iterations = HYPOTHALAMUS_QUANTUM_DEFAULT_ITERATIONS;

    config.annealing_time_us = HYPOTHALAMUS_QUANTUM_DEFAULT_ANNEALING_TIME;
    config.initial_temperature = 10.0f;
    config.final_temperature = 0.01f;

    config.quantum_mixing_ratio = HYPOTHALAMUS_QUANTUM_DEFAULT_MIXING_RATIO;
    config.enable_error_mitigation = true;

    config.optimize_temperature = true;
    config.optimize_glucose = true;
    config.optimize_hydration = true;
    config.optimize_stress = true;
    config.optimize_circadian = true;

    config.update_interval_us = 100000;  /* 100ms */
    config.enable_caching = true;
    config.cache_validity_us = 500000;   /* 500ms */

    return config;
}

hypothalamus_quantum_bridge_t* hypothalamus_quantum_bridge_create(
    hypothalamus_adapter_t* hypothalamus,
    const hypothalamus_quantum_config_t* config) {

    LOG_INFO(LOG_MODULE, "Creating hypothalamus quantum bridge");

    if (!hypothalamus) {
        LOG_ERROR(LOG_MODULE, "Hypothalamus adapter is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus is NULL");


        return NULL;
    }

    hypothalamus_quantum_bridge_t* bridge =
        (hypothalamus_quantum_bridge_t*)nimcp_calloc(
            1, sizeof(hypothalamus_quantum_bridge_t));
    if (!bridge) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate bridge memory");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypothalamus_quantum_bridge_create: allocation failed");

        return NULL;
    }

    /* Set configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = hypothalamus_quantum_default_config();
    }

    bridge->hypothalamus = hypothalamus;

    /* Initialize bridge base (creates mutex) */
    if (bridge_base_init(&bridge->base, 0, "hypothalamus_quantum") != 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "hypothalamus_quantum_bridge_create: bridge_base_init failed");
        return NULL;
    }

    /* Initialize QUBO matrix */
    if (init_qubo_matrix(&bridge->qubo, bridge->config.num_qubits) != 0) {
        bridge_base_cleanup(&bridge->base);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "hypothalamus_quantum_bridge_create: init_qubo_matrix failed");
        return NULL;
    }

    /* Initialize cache */
    bridge->cache_valid = false;
    bridge->cache_timestamp_us = 0;

    /* Initialize RNG */
    bridge->rng_state = 12345;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    LOG_INFO(LOG_MODULE, "Quantum bridge created: %u qubits, mode=%d",
             bridge->config.num_qubits, bridge->config.mode);

    return bridge;
}

void hypothalamus_quantum_bridge_destroy(hypothalamus_quantum_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hypothalamus_quantum");

    LOG_INFO(LOG_MODULE, "Destroying quantum bridge");

    free_qubo_matrix(&bridge->qubo);

    /* Cleanup bridge base (destroys mutex) */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

int hypothalamus_quantum_bridge_reset(hypothalamus_quantum_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    LOG_DEBUG(LOG_MODULE, "Resetting quantum bridge");

    /* Clear cache */
    bridge->cache_valid = false;
    bridge->cache_timestamp_us = 0;

    /* Clear QUBO values but keep allocated memory */
    if (bridge->qubo.qubo_matrix) {
        size_t matrix_elements =
            (bridge->qubo.qubo_size * (bridge->qubo.qubo_size + 1)) / 2;
        memset(bridge->qubo.qubo_matrix, 0, matrix_elements * sizeof(float));
    }
    if (bridge->qubo.linear_terms) {
        memset(bridge->qubo.linear_terms, 0,
               bridge->qubo.qubo_size * sizeof(float));
    }

    /* Reset statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return 0;
}

/*=============================================================================
 * HOMEOSTATIC OPTIMIZATION
 *===========================================================================*/

int hypothalamus_quantum_optimize_homeostasis(
    hypothalamus_quantum_bridge_t* bridge,
    const homeostatic_objective_t* objective,
    const regulatory_constraint_t* constraints,
    optimization_result_t* result) {

    if (!bridge || !objective || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_quantum_optimize_homeostasis: bridge, objective, or result is NULL");
        return -1;
    }

    if (bridge->config.mode == HYPOTHALAMUS_QUANTUM_MODE_DISABLED) {
        LOG_DEBUG(LOG_MODULE, "Quantum mode disabled");
        return -1;
    }

    /* Check cache */
    hypothalamus_state_t current_state;
    hypothalamus_get_state(bridge->hypothalamus, &current_state);

    if (bridge->config.enable_caching && bridge->cache_valid) {
        uint64_t cache_age = current_state.current_time_us - bridge->cache_timestamp_us;
        if (cache_age < bridge->config.cache_validity_us) {
            *result = bridge->cached_result;
            bridge->stats.cache_hits++;
            LOG_DEBUG(LOG_MODULE, "Using cached optimization result");
            return 0;
        }
    }

    /* Formulate QUBO */
    if (formulate_qubo(bridge, objective, constraints) != 0) {
        LOG_ERROR(LOG_MODULE, "Failed to formulate QUBO");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypothalamus_quantum_bridge_reset: validation failed");
        return -1;
    }

    /* Solve QUBO */
    /* NOTE: For now, use simulated annealing as classical fallback.
     * Actual quantum implementation would use quantum annealer here. */
    int ret = solve_qubo_simulated_annealing(bridge, result);

    if (ret != 0) {
        LOG_ERROR(LOG_MODULE, "QUBO solving failed");
        bridge->stats.classical_fallbacks++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypothalamus_quantum_bridge_reset: validation failed");
        return -1;
    }

    /* Cache result */
    if (bridge->config.enable_caching) {
        bridge->cached_result = *result;
        bridge->cache_valid = true;
        bridge->cache_timestamp_us = current_state.current_time_us;
    }

    bridge->stats.homeostatic_optimizations++;

    LOG_DEBUG(LOG_MODULE, "Homeostatic optimization complete: energy=%.4f",
              result->energy_value);

    return 0;
}

int hypothalamus_quantum_apply_optimization(
    hypothalamus_quantum_bridge_t* bridge,
    const optimization_result_t* result) {

    if (!bridge || !result || !bridge->hypothalamus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_quantum_apply_optimization: bridge, result, or hypothalamus is NULL");
        return -1;
    }

    /* Apply optimized circadian shift */
    if (fabsf(result->optimal_circadian_shift) > 0.01f) {
        hypothalamus_state_t state;
        if (hypothalamus_get_state(bridge->hypothalamus, &state)) {
            /* Direct phase adjustment through light exposure simulation */
            float light_intensity = fabsf(result->optimal_circadian_shift);
            float duration = 30.0f * 60.0f * 1000.0f;  /* 30 minutes */

            if (result->optimal_circadian_shift > 0) {
                /* Advance phase with morning light */
                hypothalamus_apply_light(bridge->hypothalamus, light_intensity, duration);
            }
        }
    }

    /* Apply optimized stress via HPA axis */
    if (result->optimal_crh_release > 0.1f) {
        /* This would normally be handled internally by the hypothalamus update */
        LOG_DEBUG(LOG_MODULE, "Optimal CRH release: %.2f", result->optimal_crh_release);
    }

    LOG_DEBUG(LOG_MODULE, "Applied optimization: heat=%.2f, symp=%.2f",
              result->optimal_heat_production, result->optimal_sympathetic_change);

    return 0;
}

int hypothalamus_quantum_get_qubo(
    const hypothalamus_quantum_bridge_t* bridge,
    homeostatic_qubo_t* qubo) {

    if (!bridge || !qubo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_quantum_get_qubo: bridge or qubo is NULL");
        return -1;
    }

    /* Copy structure (but not pointers) */
    qubo->objective = bridge->qubo.objective;
    qubo->constraints = bridge->qubo.constraints;
    qubo->current_temperature = bridge->qubo.current_temperature;
    qubo->current_glucose = bridge->qubo.current_glucose;
    qubo->current_osmolality = bridge->qubo.current_osmolality;
    qubo->current_cortisol = bridge->qubo.current_cortisol;
    qubo->current_sympathetic = bridge->qubo.current_sympathetic;
    qubo->current_parasympathetic = bridge->qubo.current_parasympathetic;
    qubo->qubo_size = bridge->qubo.qubo_size;
    qubo->offset = bridge->qubo.offset;

    /* Caller must allocate matrix memory if they want it */
    qubo->qubo_matrix = NULL;
    qubo->linear_terms = NULL;

    return 0;
}

/*=============================================================================
 * PARALLEL AUTONOMIC EVALUATION
 *===========================================================================*/

int hypothalamus_quantum_evaluate_autonomic(
    hypothalamus_quantum_bridge_t* bridge,
    const autonomic_evaluation_input_t* input,
    autonomic_evaluation_result_t* result) {

    if (!bridge || !input || !result || !input->candidates) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_quantum_evaluate_autonomic: bridge, input, result, or candidates is NULL");
        return -1;
    }

    if (input->num_candidates == 0) {
        LOG_WARNING(LOG_MODULE, "No candidates to evaluate");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypothalamus_quantum_bridge_reset: input->num_candidates is zero");
        return -1;
    }

    uint64_t start_time = 0;
    hypothalamus_state_t state;
    if (hypothalamus_get_state(bridge->hypothalamus, &state)) {
        start_time = state.current_time_us;
    }

    /* Allocate scores array */
    result->candidate_scores = (float*)nimcp_calloc(
        input->num_candidates, sizeof(float));
    if (!result->candidate_scores) {
        LOG_ERROR(LOG_MODULE, "Failed to allocate score array");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypothalamus_quantum_bridge_reset: result->candidate_scores is NULL");
        return -1;
    }

    /* Evaluate each candidate (classical approach for now) */
    /* Quantum approach would use amplitude amplification */
    float best_score = -1e10f;
    uint32_t best_index = 0;

    for (uint32_t i = 0; i < input->num_candidates; i++) {
        const autonomic_candidate_t* c = &input->candidates[i];

        /* Calculate weighted score */
        float score = 0.0f;

        /* Stability: prefer balanced autonomic state */
        float balance = fabsf(c->sympathetic_tone - c->parasympathetic_tone);
        float stability_score = 1.0f - balance;
        score += input->stability_weight * stability_score;

        /* Energy efficiency: lower energy cost is better */
        float efficiency_score = 1.0f - c->energy_cost;
        score += input->energy_efficiency_weight * efficiency_score;

        /* Responsiveness: quicker responses are better */
        float responsiveness_score = (c->sympathetic_tone + c->parasympathetic_tone) / 2.0f;
        score += input->responsiveness_weight * responsiveness_score;

        /* Smoothness: minimize extreme changes */
        float smoothness_score = 1.0f - fabsf(c->heart_rate_mod - 1.0f) * 2.0f;
        smoothness_score = nimcp_clamp01(smoothness_score);
        score += input->smoothness_weight * smoothness_score;

        result->candidate_scores[i] = score;

        if (score > best_score) {
            best_score = score;
            best_index = i;
        }
    }

    result->best_candidate_index = best_index;

    /* Calculate timing */
    if (hypothalamus_get_state(bridge->hypothalamus, &state)) {
        result->evaluation_time_us = state.current_time_us - start_time;
    } else {
        result->evaluation_time_us = 0;
    }

    /* Estimate quantum speedup (theoretical for N candidates: sqrt(N)) */
    result->quantum_speedup = sqrtf((float)input->num_candidates);

    bridge->stats.autonomic_evaluations++;

    LOG_DEBUG(LOG_MODULE, "Autonomic evaluation: %u candidates, best=%u, score=%.3f",
              input->num_candidates, best_index, best_score);

    return 0;
}

float hypothalamus_quantum_estimate_speedup(
    const hypothalamus_quantum_bridge_t* bridge,
    uint32_t num_candidates) {

    if (!bridge || num_candidates == 0) return 1.0f;

    /* Grover's algorithm provides sqrt(N) speedup for unstructured search */
    float grover_speedup = sqrtf((float)num_candidates);

    /* Apply quantum mixing ratio */
    float effective_speedup = 1.0f + (grover_speedup - 1.0f) *
                              bridge->config.quantum_mixing_ratio;

    return effective_speedup;
}

/*=============================================================================
 * CIRCADIAN OPTIMIZATION
 *===========================================================================*/

int hypothalamus_quantum_optimize_circadian(
    hypothalamus_quantum_bridge_t* bridge,
    const circadian_optimization_input_t* input,
    circadian_optimization_result_t* result) {

    if (!bridge || !input || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_quantum_optimize_circadian: bridge, input, or result is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Calculate phase difference */
    float phase_diff = input->target_phase - input->current_phase;

    /* Normalize to [-PI, PI] */
    while (phase_diff > (float)M_PI) phase_diff -= 2.0f * (float)M_PI;
    while (phase_diff < -(float)M_PI) phase_diff += 2.0f * (float)M_PI;

    bool need_advance = phase_diff > 0;
    float shift_magnitude = fabsf(phase_diff);

    /* Simple heuristic schedule (would use quantum optimization in full impl) */
    if (shift_magnitude < 0.1f) {
        /* Already close enough */
        result->num_light_exposures = 0;
        result->predicted_adaptation_time = 0.0f;
        result->predicted_jet_lag_severity = 0.0f;
        result->phase_shift_confidence = 0.95f;
    } else {
        /* Calculate number of exposures needed */
        /* Typical phase shift per exposure: ~0.5-1 hour = 0.1-0.25 radians */
        float shift_per_exposure = 0.15f * input->light_intensity;
        uint32_t num_exposures = (uint32_t)(shift_magnitude / shift_per_exposure) + 1;
        if (num_exposures > 8) num_exposures = 8;

        result->num_light_exposures = num_exposures;

        /* Schedule light exposures */
        for (uint32_t i = 0; i < num_exposures; i++) {
            if (need_advance) {
                /* Advance: light in late night / early morning */
                result->optimal_light_times[i] = 5.0f + (float)i * 0.5f;
            } else {
                /* Delay: light in evening */
                result->optimal_light_times[i] = 20.0f + (float)i * 0.5f;
            }
            result->optimal_light_durations[i] = 0.5f;  /* 30 minutes */
            result->optimal_light_intensities[i] = input->light_intensity;
        }

        /* Optimal sleep/wake times */
        if (need_advance) {
            result->optimal_wake_time = 5.5f;
            result->optimal_sleep_time = 21.0f;
        } else {
            result->optimal_wake_time = 8.0f;
            result->optimal_sleep_time = 24.0f;
        }
        result->optimal_exercise_time = result->optimal_wake_time + 4.0f;

        /* Predictions */
        result->predicted_adaptation_time = shift_magnitude / shift_per_exposure * 24.0f;
        result->predicted_jet_lag_severity = nimcp_clamp01(shift_magnitude / (float)M_PI);
        result->phase_shift_confidence = 0.7f;  /* Moderate confidence */
    }

    bridge->stats.circadian_optimizations++;

    LOG_DEBUG(LOG_MODULE, "Circadian optimization: phase_diff=%.3f, exposures=%u",
              phase_diff, result->num_light_exposures);

    return 0;
}

int hypothalamus_quantum_predict_circadian(
    hypothalamus_quantum_bridge_t* bridge,
    float initial_phase,
    const float* light_schedule,
    uint32_t schedule_length,
    float* predicted_phases) {

    if (!bridge || !light_schedule || !predicted_phases || schedule_length == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypothalamus_quantum_predict_circadian: invalid parameters");
        return -1;
    }

    /* Simple phase oscillator model (would use quantum simulation in full impl) */
    float phase = initial_phase;
    float period_hours = 24.0f;
    float hour_per_step = 1.0f;  /* Assume 1 hour per schedule step */

    for (uint32_t i = 0; i < schedule_length; i++) {
        /* Natural phase progression */
        phase += (2.0f * (float)M_PI / period_hours) * hour_per_step;

        /* Light-induced phase shift (phase response curve) */
        float prc = 0.0f;
        float hour_of_day = fmodf(phase * 12.0f / (float)M_PI, 24.0f);

        if (hour_of_day > 21.0f || hour_of_day < 6.0f) {
            /* Night: light advances phase */
            prc = 0.1f;
        } else if (hour_of_day > 18.0f) {
            /* Evening: light delays phase */
            prc = -0.1f;
        } else {
            /* Daytime: minimal effect */
            prc = 0.02f;
        }

        phase += prc * light_schedule[i];

        /* Normalize phase */
        while (phase >= 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
        while (phase < 0.0f) phase += 2.0f * (float)M_PI;

        predicted_phases[i] = phase;
    }

    LOG_DEBUG(LOG_MODULE, "Circadian prediction: %u steps, final_phase=%.3f",
              schedule_length, predicted_phases[schedule_length - 1]);

    return 0;
}

/*=============================================================================
 * HPA AXIS OPTIMIZATION
 *===========================================================================*/

int hypothalamus_quantum_optimize_hpa(
    hypothalamus_quantum_bridge_t* bridge,
    const hpa_axis_state_t* current_hpa,
    const float* stress_history,
    uint32_t history_length,
    float* optimal_sensitivity,
    float* optimal_feedback) {

    if (!bridge || !current_hpa || !optimal_sensitivity || !optimal_feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_quantum_optimize_hpa: required parameters are NULL");
        return -1;
    }

    /* Calculate average and variance of stress history */
    float avg_stress = 0.0f;
    float var_stress = 0.0f;

    if (stress_history && history_length > 0) {
        for (uint32_t i = 0; i < history_length; i++) {
            avg_stress += stress_history[i];
        }
        avg_stress /= (float)history_length;

        for (uint32_t i = 0; i < history_length; i++) {
            float diff = stress_history[i] - avg_stress;
            var_stress += diff * diff;
        }
        var_stress /= (float)history_length;
    }

    /* Optimal HPA sensitivity depends on stress environment */
    /* High stress variability -> higher sensitivity for quick response */
    /* Chronic high stress -> lower sensitivity to prevent exhaustion */

    float stress_variability = sqrtf(var_stress);

    if (current_hpa->chronic_stress) {
        /* Reduce sensitivity to prevent HPA exhaustion */
        *optimal_sensitivity = nimcp_clamp01(0.5f - avg_stress * 0.3f);
    } else if (stress_variability > 0.3f) {
        /* High variability needs responsive HPA */
        *optimal_sensitivity = nimcp_clamp01(0.8f + stress_variability * 0.2f);
    } else {
        /* Normal conditions */
        *optimal_sensitivity = 1.0f;
    }

    /* Optimal negative feedback */
    /* Higher feedback with chronic stress to limit cortisol */
    /* Lower feedback with acute stress for full response */

    if (current_hpa->chronic_stress) {
        *optimal_feedback = nimcp_clamp01(0.7f + avg_stress * 0.2f);
    } else if (avg_stress > 0.5f) {
        *optimal_feedback = 0.4f;  /* Allow stronger response */
    } else {
        *optimal_feedback = 0.5f;  /* Normal feedback */
    }

    bridge->stats.hpa_optimizations++;

    LOG_DEBUG(LOG_MODULE, "HPA optimization: sensitivity=%.2f, feedback=%.2f",
              *optimal_sensitivity, *optimal_feedback);

    return 0;
}

/*=============================================================================
 * INTEGRATED UPDATE
 *===========================================================================*/

int hypothalamus_quantum_update(hypothalamus_quantum_bridge_t* bridge,
                                 uint64_t delta_time_us) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    if (bridge->config.mode == HYPOTHALAMUS_QUANTUM_MODE_DISABLED) {
        return 0;
    }

    /* Rate limiting */
    hypothalamus_state_t state;
    if (!hypothalamus_get_state(bridge->hypothalamus, &state)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: hypothalamus_get_state is NULL");
        return -1;
    }

    uint64_t elapsed = state.current_time_us - bridge->last_update_us;
    if (elapsed < bridge->config.update_interval_us) {
        return 0;  /* Too soon for another update */
    }

    bridge->last_update_us = state.current_time_us;

    /* Run optimizations based on mode */
    optimization_result_t result;
    memset(&result, 0, sizeof(result));

    if (bridge->config.mode == HYPOTHALAMUS_QUANTUM_MODE_FULL ||
        bridge->config.mode == HYPOTHALAMUS_QUANTUM_MODE_HOMEOSTATIC) {

        homeostatic_objective_t objective = {
            .temperature_weight = 1.0f,
            .glucose_weight = 0.8f,
            .hydration_weight = 0.6f,
            .stress_weight = 0.5f,
            .energy_weight = 0.3f,
            .temperature_target = 37.0f,
            .glucose_target = 90.0f,
            .osmolality_target = 290.0f,
            .cortisol_target = 0.3f
        };

        regulatory_constraint_t constraints = {
            .min_temperature = 35.0f,
            .max_temperature = 39.0f,
            .min_glucose = 70.0f,
            .max_glucose = 140.0f,
            .max_cortisol = 0.9f,
            .max_autonomic_change_rate = 0.1f
        };

        if (hypothalamus_quantum_optimize_homeostasis(bridge, &objective,
                                                       &constraints, &result) == 0) {
            hypothalamus_quantum_apply_optimization(bridge, &result);
        }
    }

    return 0;
}

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

int hypothalamus_quantum_get_stats(
    const hypothalamus_quantum_bridge_t* bridge,
    hypothalamus_quantum_stats_t* stats) {

    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_quantum_get_stats: bridge or stats is NULL");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

bool hypothalamus_quantum_is_available(
    const hypothalamus_quantum_bridge_t* bridge) {

    if (!bridge) {
        return false;
    }
    return bridge->config.mode != HYPOTHALAMUS_QUANTUM_MODE_DISABLED;
}

hypothalamus_quantum_mode_t hypothalamus_quantum_get_mode(
    const hypothalamus_quantum_bridge_t* bridge) {

    if (!bridge) return HYPOTHALAMUS_QUANTUM_MODE_DISABLED;
    return bridge->config.mode;
}

int hypothalamus_quantum_set_mode(
    hypothalamus_quantum_bridge_t* bridge,
    hypothalamus_quantum_mode_t mode) {

    if (!bridge) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");


        return -1;


    }
    bridge->config.mode = mode;

    LOG_INFO(LOG_MODULE, "Quantum mode set to: %d", mode);
    return 0;
}

int hypothalamus_quantum_get_config(
    const hypothalamus_quantum_bridge_t* bridge,
    hypothalamus_quantum_config_t* config) {

    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypothalamus_quantum_get_config: bridge or config is NULL");
        return -1;
    }
    *config = bridge->config;
    return 0;
}
