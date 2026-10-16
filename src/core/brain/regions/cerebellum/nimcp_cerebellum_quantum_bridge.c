/**
 * @file nimcp_cerebellum_quantum_bridge.c
 * @brief Quantum Cerebellum Bridge Implementation
 *
 * Integrates quantum algorithms with the Cerebellum for
 * optimized motor timing, trajectory selection, and gain adaptation.
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/cerebellum/nimcp_cerebellum_quantum_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(cerebellum_quantum_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "CEREBELLUM_QUANTUM_BRIDGE"


/*=============================================================================
 * Internal Structure
 *===========================================================================*/

struct cerebellum_quantum_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* cerebellum;                         /**< Cerebellum adapter handle */
    cerebellum_quantum_config_t config;       /**< Configuration */
    qreason_t quantum_reasoner;               /**< Quantum reasoning engine */
    cerebellum_quantum_stats_t stats;         /**< Statistics */

    /* Candidate tracking */
    quantum_timing_candidate_t* timing_candidates;
    quantum_trajectory_candidate_t* trajectory_candidates;
    quantum_gain_candidate_t* gain_candidates;
    uint32_t max_candidates;

    /* RNG state */
    uint32_t rng_state;
};

/*=============================================================================
 * Helper Functions
 *===========================================================================*/

static uint32_t quantum_rand(uint32_t* state) {
    *state = *state * 1103515245 + 12345;
    return (*state >> 16) & 0x7FFF;
}

static float quantum_randf(uint32_t* state) {
    return (float)quantum_rand(state) / 32767.0f;
}

/*=============================================================================
 * Configuration API
 *===========================================================================*/

cerebellum_quantum_config_t cerebellum_quantum_default_config(void) {
    return (cerebellum_quantum_config_t){
        .enabled = true,
        .timing_search_depth = 1000,
        .trajectory_alternatives = 16,
        .max_grover_iterations = 10,
        .min_timing_confidence = 0.5f,
        .enable_interference = true,
        .use_superposition = true,
        .seed = 42
    };
}

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

cerebellum_quantum_bridge_t* cerebellum_quantum_bridge_create(
    void* cerebellum,
    const cerebellum_quantum_config_t* config
) {
    cerebellum_quantum_bridge_t* bridge = nimcp_calloc(1, sizeof(cerebellum_quantum_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    bridge->cerebellum = cerebellum;
    bridge->config = config ? *config : cerebellum_quantum_default_config();

    /* Create quantum reasoner */
    qreason_config_t qconfig = qreason_default_config();
    qconfig.max_grover_iterations = bridge->config.max_grover_iterations;
    qconfig.min_confidence = bridge->config.min_timing_confidence;
    qconfig.enable_interference = bridge->config.enable_interference;
    qconfig.seed = bridge->config.seed;

    bridge->quantum_reasoner = qreason_create(&qconfig);
    if (!bridge->quantum_reasoner) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cerebellum_quantum_bridge_create: bridge->quantum_reasoner is NULL");
        return NULL;
    }

    /* Allocate candidate arrays */
    bridge->max_candidates = bridge->config.trajectory_alternatives;

    bridge->timing_candidates = nimcp_calloc(
        bridge->max_candidates, sizeof(quantum_timing_candidate_t));
    bridge->trajectory_candidates = nimcp_calloc(
        bridge->max_candidates, sizeof(quantum_trajectory_candidate_t));
    bridge->gain_candidates = nimcp_calloc(
        bridge->max_candidates, sizeof(quantum_gain_candidate_t));

    if (!bridge->timing_candidates || !bridge->trajectory_candidates ||
        !bridge->gain_candidates) {
        cerebellum_quantum_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cerebellum_quantum_bridge_create: operation failed");
        return NULL;
    }

    bridge->rng_state = bridge->config.seed;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return bridge;
}

void cerebellum_quantum_bridge_destroy(cerebellum_quantum_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "cerebellum_quantum");

    if (bridge->timing_candidates) nimcp_free(bridge->timing_candidates);
    if (bridge->trajectory_candidates) nimcp_free(bridge->trajectory_candidates);
    if (bridge->gain_candidates) nimcp_free(bridge->gain_candidates);

    if (bridge->quantum_reasoner) {
        qreason_destroy(bridge->quantum_reasoner);
    }

    nimcp_free(bridge);
}

bool cerebellum_quantum_bridge_is_enabled(const cerebellum_quantum_bridge_t* bridge) {
    return bridge && bridge->config.enabled;
}

void cerebellum_quantum_bridge_set_enabled(cerebellum_quantum_bridge_t* bridge, bool enabled) {
    if (bridge) bridge->config.enabled = enabled;
}

/*=============================================================================
 * Timing Optimization API
 *===========================================================================*/

int cerebellum_quantum_optimize_timing(
    cerebellum_quantum_bridge_t* bridge,
    float target_timing_ms,
    float timing_range_ms,
    uint32_t num_alternatives,
    quantum_timing_result_t* result
) {
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_quantum_optimize_timing: required parameter is NULL (bridge, result)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_quantum_optimize_timing: bridge->config is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Build CNF for timing optimization problem */
    qreason_cnf_t timing_cnf = {0};
    timing_cnf.n_variables = (num_alternatives < QREASON_MAX_VARIABLES) ?
                             num_alternatives : QREASON_MAX_VARIABLES;

    /* Simple satisfiability: at least one timing must work */
    timing_cnf.n_clauses = 1;
    timing_cnf.clauses[0].n_literals = (timing_cnf.n_variables < QREASON_MAX_LITERALS) ?
                                       timing_cnf.n_variables : QREASON_MAX_LITERALS;

    for (uint32_t i = 0; i < timing_cnf.clauses[0].n_literals; i++) {
        timing_cnf.clauses[0].literals[i].variable = i;
        timing_cnf.clauses[0].literals[i].negated = false;
    }

    /* Solve using quantum search */
    qreason_result_t qresult;
    int ret = qreason_solve_sat(bridge->quantum_reasoner, &timing_cnf, &qresult);
    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cerebellum_quantum_optimize_timing: validation failed");
        return -1;
    }

    /* Generate timing candidates from quantum state */
    uint32_t num_candidates = (bridge->max_candidates < timing_cnf.n_variables) ?
                              bridge->max_candidates : timing_cnf.n_variables;

    quantum_timing_candidate_t* best = NULL;
    float best_score = -1.0f;

    for (uint32_t i = 0; i < num_candidates; i++) {
        quantum_timing_candidate_t* cand = &bridge->timing_candidates[i];

        cand->timing_id = i;
        /* Generate timing value within range */
        float offset = (quantum_randf(&bridge->rng_state) - 0.5f) * timing_range_ms;
        cand->timing_ms = target_timing_ms + offset;
        cand->amplitude = qresult.confidences[i];
        cand->precision_score = 1.0f - fabsf(offset) / timing_range_ms;
        cand->energy_cost = quantum_randf(&bridge->rng_state) * 0.5f;

        /* Combined score: high amplitude, high precision, low energy */
        cand->combined_score = cand->amplitude * 0.4f +
                               cand->precision_score * 0.4f +
                               (1.0f - cand->energy_cost) * 0.2f;

        if (cand->combined_score > best_score) {
            best_score = cand->combined_score;
            best = cand;
        }
    }

    /* Fill result */
    result->best_timing = best;
    result->candidates_evaluated = num_candidates;
    result->satisfaction_probability = qresult.satisfaction_prob;
    result->grover_iterations_used = qresult.grover_iterations;

    /* Estimate speedup (Grover provides quadratic speedup) */
    float classical_cost = (float)num_alternatives;
    float quantum_cost = sqrtf((float)num_alternatives);
    result->search_speedup = classical_cost / (quantum_cost > 0.0f ? quantum_cost : 1.0f);

    /* Update statistics */
    bridge->stats.timing_optimizations++;
    bridge->stats.avg_timing_speedup =
        (bridge->stats.avg_timing_speedup * (bridge->stats.timing_optimizations - 1) +
         result->search_speedup) / bridge->stats.timing_optimizations;
    bridge->stats.avg_satisfaction_prob =
        (bridge->stats.avg_satisfaction_prob * (bridge->stats.timing_optimizations - 1) +
         result->satisfaction_probability) / bridge->stats.timing_optimizations;

    if (best && best->combined_score >= bridge->config.min_timing_confidence) {
        bridge->stats.successful_optimizations++;
    } else {
        bridge->stats.failed_optimizations++;
    }

    return 0;
}

/*=============================================================================
 * Trajectory Optimization API
 *===========================================================================*/

int cerebellum_quantum_optimize_trajectory(
    cerebellum_quantum_bridge_t* bridge,
    const float* start_state,
    const float* end_state,
    uint32_t num_dims,
    float max_duration_ms,
    quantum_trajectory_result_t* result
) {
    if (!bridge || !result || !start_state || !end_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_quantum_optimize_trajectory: required parameter is NULL (bridge, result, start_state, end_state)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_quantum_optimize_trajectory: bridge->config is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Build CNF for trajectory optimization */
    qreason_cnf_t traj_cnf = {0};
    traj_cnf.n_variables = (bridge->config.trajectory_alternatives < QREASON_MAX_VARIABLES) ?
                           bridge->config.trajectory_alternatives : QREASON_MAX_VARIABLES;

    traj_cnf.n_clauses = 1;
    traj_cnf.clauses[0].n_literals = traj_cnf.n_variables;

    for (uint32_t i = 0; i < traj_cnf.n_variables; i++) {
        traj_cnf.clauses[0].literals[i].variable = i;
        traj_cnf.clauses[0].literals[i].negated = false;
    }

    /* Solve using quantum search */
    qreason_result_t qresult;
    int ret = qreason_solve_sat(bridge->quantum_reasoner, &traj_cnf, &qresult);
    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cerebellum_quantum_optimize_trajectory: validation failed");
        return -1;
    }

    /* Generate trajectory candidates */
    quantum_trajectory_candidate_t* best = NULL;
    float best_score = -1.0f;
    uint32_t dims = (num_dims < 8) ? num_dims : 8;

    for (uint32_t i = 0; i < traj_cnf.n_variables && i < bridge->max_candidates; i++) {
        quantum_trajectory_candidate_t* cand = &bridge->trajectory_candidates[i];

        cand->trajectory_id = i;
        cand->amplitude = qresult.confidences[i];

        /* Generate waypoints (interpolation with noise) */
        cand->num_waypoints = 8 + (quantum_rand(&bridge->rng_state) % 8);
        if (cand->num_waypoints > 32) cand->num_waypoints = 32;

        for (uint32_t w = 0; w < cand->num_waypoints; w++) {
            float t = (float)w / (float)(cand->num_waypoints - 1);
            float noise = (quantum_randf(&bridge->rng_state) - 0.5f) * 0.2f;
            /* Linear interpolation plus noise */
            cand->waypoints[w] = start_state[0] + t * (end_state[0] - start_state[0]) + noise;
        }

        /* Compute smoothness (second derivative magnitude) */
        float smoothness = 1.0f;
        for (uint32_t w = 1; w < cand->num_waypoints - 1; w++) {
            float d2 = cand->waypoints[w-1] - 2*cand->waypoints[w] + cand->waypoints[w+1];
            smoothness -= fabsf(d2) * 0.1f;
        }
        cand->smoothness_score = (smoothness < 0.0f) ? 0.0f : smoothness;

        /* Energy efficiency (path length relative to minimum) */
        float path_length = 0.0f;
        for (uint32_t w = 1; w < cand->num_waypoints; w++) {
            path_length += fabsf(cand->waypoints[w] - cand->waypoints[w-1]);
        }
        float min_length = fabsf(end_state[0] - start_state[0]);
        cand->energy_efficiency = (path_length > 0.0f) ? min_length / path_length : 1.0f;
        if (cand->energy_efficiency > 1.0f) cand->energy_efficiency = 1.0f;

        /* Duration proportional to path length */
        cand->duration_ms = path_length * 10.0f;  /* 10ms per unit distance */
        cand->is_feasible = (cand->duration_ms <= max_duration_ms);

        /* Combined score */
        float score = cand->amplitude * 0.3f +
                      cand->smoothness_score * 0.3f +
                      cand->energy_efficiency * 0.2f +
                      (cand->is_feasible ? 0.2f : 0.0f);

        if (score > best_score && cand->is_feasible) {
            best_score = score;
            best = cand;
        }
    }

    /* Fill result */
    result->best_trajectory = best;
    result->trajectories_evaluated = traj_cnf.n_variables;
    result->satisfaction_probability = qresult.satisfaction_prob;
    result->grover_iterations_used = qresult.grover_iterations;

    /* Update statistics */
    bridge->stats.trajectory_optimizations++;

    float speedup = sqrtf((float)result->trajectories_evaluated);
    bridge->stats.avg_trajectory_speedup =
        (bridge->stats.avg_trajectory_speedup * (bridge->stats.trajectory_optimizations - 1) +
         speedup) / bridge->stats.trajectory_optimizations;

    return 0;
}

/*=============================================================================
 * Gain Optimization API
 *===========================================================================*/

int cerebellum_quantum_optimize_gains(
    cerebellum_quantum_bridge_t* bridge,
    const float* current_gains,
    uint32_t num_gains,
    float error_signal,
    quantum_gain_result_t* result
) {
    if (!bridge || !result || !current_gains) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_quantum_optimize_gains: required parameter is NULL (bridge, result, current_gains)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_quantum_optimize_gains: bridge->config is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Build CNF for gain optimization */
    qreason_cnf_t gain_cnf = {0};
    gain_cnf.n_variables = (num_gains * 4 < QREASON_MAX_VARIABLES) ?
                           num_gains * 4 : QREASON_MAX_VARIABLES;  /* 4 alternatives per gain */

    gain_cnf.n_clauses = 1;
    gain_cnf.clauses[0].n_literals = gain_cnf.n_variables;

    for (uint32_t i = 0; i < gain_cnf.n_variables; i++) {
        gain_cnf.clauses[0].literals[i].variable = i;
        gain_cnf.clauses[0].literals[i].negated = false;
    }

    /* Solve using quantum search */
    qreason_result_t qresult;
    int ret = qreason_solve_sat(bridge->quantum_reasoner, &gain_cnf, &qresult);
    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cerebellum_quantum_optimize_gains: validation failed");
        return -1;
    }

    /* Generate gain candidates */
    uint32_t num_candidates = (bridge->max_candidates < 8) ? bridge->max_candidates : 8;
    quantum_gain_candidate_t* best = NULL;
    float best_score = -1.0f;
    uint32_t dims = (num_gains < 8) ? num_gains : 8;

    for (uint32_t i = 0; i < num_candidates; i++) {
        quantum_gain_candidate_t* cand = &bridge->gain_candidates[i];

        cand->gain_set_id = i;
        cand->num_gains = dims;
        cand->amplitude = qresult.confidences[i % gain_cnf.n_variables];

        /* Generate gain variations */
        for (uint32_t g = 0; g < dims; g++) {
            float variation = (quantum_randf(&bridge->rng_state) - 0.5f) * 0.4f;
            cand->gains[g] = current_gains[g] + variation * fabsf(error_signal);

            /* Clamp gains to reasonable range */
            if (cand->gains[g] < 0.1f) cand->gains[g] = 0.1f;
            if (cand->gains[g] > 2.0f) cand->gains[g] = 2.0f;
        }

        /* Stability score (gains not too high) */
        float max_gain = 0.0f;
        for (uint32_t g = 0; g < dims; g++) {
            if (cand->gains[g] > max_gain) max_gain = cand->gains[g];
        }
        cand->stability_score = 1.0f - (max_gain - 1.0f);
        if (cand->stability_score < 0.0f) cand->stability_score = 0.0f;

        /* Responsiveness (gains close to original) */
        float diff_sum = 0.0f;
        for (uint32_t g = 0; g < dims; g++) {
            diff_sum += fabsf(cand->gains[g] - current_gains[g]);
        }
        cand->responsiveness = 1.0f - diff_sum / dims;

        /* Combined score */
        float score = cand->amplitude * 0.4f +
                      cand->stability_score * 0.3f +
                      cand->responsiveness * 0.3f;

        if (score > best_score) {
            best_score = score;
            best = cand;
        }
    }

    /* Fill result */
    result->best_gains = best;
    result->candidates_evaluated = num_candidates;
    result->optimization_score = best_score;

    /* Update statistics */
    bridge->stats.gain_optimizations++;

    return 0;
}

/*=============================================================================
 * Parallel Motor Program Evaluation API
 *===========================================================================*/

int cerebellum_quantum_evaluate_programs(
    cerebellum_quantum_bridge_t* bridge,
    const float** programs,
    uint32_t num_programs,
    uint32_t program_dims,
    float* scores
) {
    if (!bridge || !programs || !scores || num_programs == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_quantum_evaluate_programs: required parameter is NULL (bridge, programs, scores)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_quantum_evaluate_programs: bridge->config is NULL");
        return -1;
    }

    /* Evaluate all programs using quantum parallelism simulation */
    for (uint32_t i = 0; i < num_programs; i++) {
        float score = 0.0f;

        if (programs[i]) {
            /* Score based on magnitude and smoothness */
            float magnitude = 0.0f;
            float variance = 0.0f;
            float mean = 0.0f;

            for (uint32_t d = 0; d < program_dims; d++) {
                magnitude += programs[i][d] * programs[i][d];
                mean += programs[i][d];
            }
            magnitude = sqrtf(magnitude);
            mean /= program_dims;

            for (uint32_t d = 0; d < program_dims; d++) {
                float diff = programs[i][d] - mean;
                variance += diff * diff;
            }
            variance /= program_dims;

            /* Score: moderate magnitude, low variance (consistency) */
            float mag_score = 1.0f / (1.0f + fabsf(magnitude - 1.0f));
            float var_score = 1.0f / (1.0f + variance);

            score = mag_score * 0.5f + var_score * 0.5f;
        }

        scores[i] = score;
    }

    return 0;
}

int cerebellum_quantum_select_program(
    cerebellum_quantum_bridge_t* bridge,
    const float** programs,
    uint32_t num_programs,
    uint32_t program_dims,
    uint32_t* best_program_idx,
    float* confidence
) {
    if (!bridge || !programs || !best_program_idx || num_programs == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_quantum_select_program: required parameter is NULL (bridge, programs, best_program_idx)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_quantum_select_program: bridge->config is NULL");
        return -1;
    }

    /* Evaluate all programs */
    float* scores = nimcp_calloc(num_programs, sizeof(float));
    if (!scores) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cerebellum_quantum_select_program: scores is NULL");
        return -1;
    }

    int ret = cerebellum_quantum_evaluate_programs(bridge, programs, num_programs,
                                                    program_dims, scores);
    if (ret != 0) {
        nimcp_free(scores);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cerebellum_quantum_select_program: validation failed");
        return -1;
    }

    /* Find best program */
    uint32_t best_idx = 0;
    float best_score = scores[0];

    for (uint32_t i = 1; i < num_programs; i++) {
        if (scores[i] > best_score) {
            best_score = scores[i];
            best_idx = i;
        }
    }

    *best_program_idx = best_idx;

    if (confidence) {
        /* Confidence based on margin over second best */
        float second_best = 0.0f;
        for (uint32_t i = 0; i < num_programs; i++) {
            if (i != best_idx && scores[i] > second_best) {
                second_best = scores[i];
            }
        }
        *confidence = (best_score - second_best) / (best_score + 0.001f);
        if (*confidence > 1.0f) *confidence = 1.0f;
    }

    nimcp_free(scores);
    return 0;
}

/*=============================================================================
 * Statistics API
 *===========================================================================*/

int cerebellum_quantum_get_stats(
    const cerebellum_quantum_bridge_t* bridge,
    cerebellum_quantum_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_quantum_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void cerebellum_quantum_reset_stats(cerebellum_quantum_bridge_t* bridge) {
    if (bridge) {
        memset(&bridge->stats, 0, sizeof(bridge->stats));
    }
}

int cerebellum_quantum_get_config(
    const cerebellum_quantum_bridge_t* bridge,
    cerebellum_quantum_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cerebellum_quantum_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }
    *config = bridge->config;
    return 0;
}
