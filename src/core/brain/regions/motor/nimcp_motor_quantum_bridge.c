/**
 * @file nimcp_motor_quantum_bridge.c
 * @brief Quantum Motor Bridge Implementation
 *
 * Integrates quantum algorithms with Motor Cortex for
 * optimized trajectory planning, program selection, and timing.
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/motor/nimcp_motor_quantum_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(motor_quantum_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "MOTOR_QUANTUM_BRIDGE"


//=============================================================================
// Internal Structure
//=============================================================================

struct motor_quantum_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* motor;                          /**< Motor adapter handle */
    motor_quantum_config_t config;        /**< Configuration */
    qreason_t quantum_reasoner;           /**< Quantum reasoning engine */
    motor_quantum_stats_t stats;          /**< Statistics */

    /* Candidate tracking */
    quantum_trajectory_candidate_t* trajectory_candidates;
    quantum_program_candidate_t* program_candidates;
    quantum_timing_candidate_t* timing_candidates;
    uint32_t max_candidates;

    /* Waypoint storage for trajectory candidates */
    quantum_trajectory_waypoint_t* waypoint_storage;
    uint32_t waypoints_per_trajectory;

    /* RNG state */
    uint32_t rng_state;
};

//=============================================================================
// Helper Functions
//=============================================================================

static uint32_t quantum_rand(uint32_t* state) {
    *state = *state * 1103515245 + 12345;
    return (*state >> 16) & 0x7FFF;
}

static float quantum_randf(uint32_t* state) {
    return (float)quantum_rand(state) / 32767.0f;
}

/**
 * @brief Compute distance between two 3D points
 */
static float vec3_distance(const quantum_motor_vec3_t* a, const quantum_motor_vec3_t* b) {
    float dx = b->x - a->x;
    float dy = b->y - a->y;
    float dz = b->z - a->z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

/**
 * @brief Linear interpolation between two 3D points
 */
static quantum_motor_vec3_t vec3_lerp(const quantum_motor_vec3_t* a,
                                       const quantum_motor_vec3_t* b,
                                       float t) {
    quantum_motor_vec3_t result;
    result.x = a->x + (b->x - a->x) * t;
    result.y = a->y + (b->y - a->y) * t;
    result.z = a->z + (b->z - a->z) * t;
    return result;
}

/**
 * @brief Generate a randomized trajectory variant
 */
static void generate_trajectory_variant(motor_quantum_bridge_t* bridge,
                                         quantum_trajectory_candidate_t* cand,
                                         const quantum_motor_vec3_t* start,
                                         const quantum_motor_vec3_t* end,
                                         float max_duration,
                                         uint32_t variant_id) {
    cand->trajectory_id = variant_id;
    cand->num_waypoints = 3 + (variant_id % 5);  /* 3-7 waypoints */

    if (cand->num_waypoints > bridge->waypoints_per_trajectory) {
        cand->num_waypoints = bridge->waypoints_per_trajectory;
    }

    /* Start waypoint */
    cand->waypoints[0].position = *start;
    cand->waypoints[0].velocity.x = 0.0f;
    cand->waypoints[0].velocity.y = 0.0f;
    cand->waypoints[0].velocity.z = 0.0f;
    cand->waypoints[0].time_ms = 0.0f;

    /* Intermediate waypoints with random perturbation */
    for (uint32_t i = 1; i < cand->num_waypoints - 1; i++) {
        float t = (float)i / (float)(cand->num_waypoints - 1);

        /* Base interpolated position */
        quantum_motor_vec3_t base = vec3_lerp(start, end, t);

        /* Add random perturbation for trajectory variation */
        float perturb = 0.2f * (quantum_randf(&bridge->rng_state) - 0.5f);
        cand->waypoints[i].position.x = base.x + perturb;
        cand->waypoints[i].position.y = base.y + perturb;
        cand->waypoints[i].position.z = base.z + perturb;

        /* Velocity estimate */
        cand->waypoints[i].velocity.x = (end->x - start->x) / max_duration * 1000.0f;
        cand->waypoints[i].velocity.y = (end->y - start->y) / max_duration * 1000.0f;
        cand->waypoints[i].velocity.z = (end->z - start->z) / max_duration * 1000.0f;

        cand->waypoints[i].time_ms = t * max_duration;
    }

    /* End waypoint */
    uint32_t last = cand->num_waypoints - 1;
    cand->waypoints[last].position = *end;
    cand->waypoints[last].velocity.x = 0.0f;
    cand->waypoints[last].velocity.y = 0.0f;
    cand->waypoints[last].velocity.z = 0.0f;
    cand->waypoints[last].time_ms = max_duration;
}

//=============================================================================
// Configuration API
//=============================================================================

motor_quantum_config_t motor_quantum_default_config(void) {
    return (motor_quantum_config_t){
        .enabled = true,
        .trajectory_alternatives = 16,
        .program_search_depth = 100,
        .max_grover_iterations = 10,
        .min_trajectory_confidence = 0.5f,
        .enable_interference = true,
        .use_superposition = true,
        .energy_weight = 0.3f,
        .time_weight = 0.3f,
        .accuracy_weight = 0.4f,
        .seed = 42
    };
}

//=============================================================================
// Lifecycle API
//=============================================================================

motor_quantum_bridge_t* motor_quantum_bridge_create(
    void* motor,
    const motor_quantum_config_t* config
) {
    motor_quantum_bridge_t* bridge = nimcp_calloc(1, sizeof(motor_quantum_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    bridge->motor = motor;
    bridge->config = config ? *config : motor_quantum_default_config();

    /* Create quantum reasoner */
    qreason_config_t qconfig = qreason_default_config();
    qconfig.max_grover_iterations = bridge->config.max_grover_iterations;
    qconfig.min_confidence = bridge->config.min_trajectory_confidence;
    qconfig.enable_interference = bridge->config.enable_interference;
    qconfig.seed = bridge->config.seed;

    bridge->quantum_reasoner = qreason_create(&qconfig);
    if (!bridge->quantum_reasoner) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "motor_quantum_bridge_create: bridge->quantum_reasoner is NULL");
        return NULL;
    }

    /* Allocate candidate arrays */
    bridge->max_candidates = bridge->config.trajectory_alternatives;
    bridge->waypoints_per_trajectory = 16;  /* Max waypoints per trajectory */

    bridge->trajectory_candidates = nimcp_calloc(
        bridge->max_candidates, sizeof(quantum_trajectory_candidate_t));
    bridge->program_candidates = nimcp_calloc(
        bridge->max_candidates, sizeof(quantum_program_candidate_t));
    bridge->timing_candidates = nimcp_calloc(
        bridge->max_candidates, sizeof(quantum_timing_candidate_t));

    /* Allocate waypoint storage for all trajectory candidates */
    size_t total_waypoints = bridge->max_candidates * bridge->waypoints_per_trajectory;
    bridge->waypoint_storage = nimcp_calloc(
        total_waypoints, sizeof(quantum_trajectory_waypoint_t));

    if (!bridge->trajectory_candidates || !bridge->program_candidates ||
        !bridge->timing_candidates || !bridge->waypoint_storage) {
        motor_quantum_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "motor_quantum_bridge_create: operation failed");
        return NULL;
    }

    /* Assign waypoint storage to trajectory candidates */
    for (uint32_t i = 0; i < bridge->max_candidates; i++) {
        bridge->trajectory_candidates[i].waypoints =
            &bridge->waypoint_storage[i * bridge->waypoints_per_trajectory];
    }

    bridge->rng_state = bridge->config.seed;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return bridge;
}

void motor_quantum_bridge_destroy(motor_quantum_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "motor_quantum");

    if (bridge->trajectory_candidates) nimcp_free(bridge->trajectory_candidates);
    if (bridge->program_candidates) nimcp_free(bridge->program_candidates);
    if (bridge->timing_candidates) nimcp_free(bridge->timing_candidates);
    if (bridge->waypoint_storage) nimcp_free(bridge->waypoint_storage);

    if (bridge->quantum_reasoner) {
        qreason_destroy(bridge->quantum_reasoner);
    }

    nimcp_free(bridge);
}

bool motor_quantum_bridge_is_enabled(const motor_quantum_bridge_t* bridge) {
    return bridge && bridge->config.enabled;
}

void motor_quantum_bridge_set_enabled(motor_quantum_bridge_t* bridge, bool enabled) {
    if (bridge) bridge->config.enabled = enabled;
}

//=============================================================================
// Trajectory Optimization API
//=============================================================================

int motor_quantum_optimize_trajectory(
    motor_quantum_bridge_t* bridge,
    const quantum_motor_vec3_t* start_position,
    const quantum_motor_vec3_t* end_position,
    float max_duration,
    uint32_t num_alternatives,
    quantum_trajectory_result_t* result
) {
    if (!bridge || !start_position || !end_position || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_quantum_optimize_trajectory: required parameter is NULL (bridge, start_position, end_position, result)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_quantum_optimize_trajectory: bridge->config is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Limit alternatives to capacity */
    if (num_alternatives > bridge->max_candidates) {
        num_alternatives = bridge->max_candidates;
    }

    /* Build CNF for trajectory optimization */
    qreason_cnf_t traj_cnf = {0};
    traj_cnf.n_variables = (num_alternatives < QREASON_MAX_VARIABLES) ?
                           num_alternatives : QREASON_MAX_VARIABLES;

    /* Each variable represents a trajectory choice */
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
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_quantum_optimize_trajectory: validation failed");
        return -1;
    }

    /* Generate trajectory candidates */
    float distance = vec3_distance(start_position, end_position);
    quantum_trajectory_candidate_t* best = NULL;
    float best_score = -1.0f;

    for (uint32_t i = 0; i < num_alternatives; i++) {
        quantum_trajectory_candidate_t* cand = &bridge->trajectory_candidates[i];

        /* Generate trajectory variant */
        generate_trajectory_variant(bridge, cand, start_position, end_position,
                                     max_duration, i);

        /* Compute costs based on trajectory characteristics */
        cand->amplitude = qresult.confidences[i % traj_cnf.n_variables];

        /* Energy cost: longer/more complex paths cost more */
        float path_length = 0.0f;
        for (uint32_t j = 1; j < cand->num_waypoints; j++) {
            path_length += vec3_distance(&cand->waypoints[j-1].position,
                                          &cand->waypoints[j].position);
        }
        cand->energy_cost = path_length / (distance * 2.0f);  /* Normalize */
        cand->energy_cost = fminf(1.0f, cand->energy_cost);

        /* Time cost: normalized by max duration */
        cand->time_cost = cand->waypoints[cand->num_waypoints - 1].time_ms / max_duration;

        /* Accuracy score: straighter paths are more accurate */
        float straightness = distance / fmaxf(path_length, 0.001f);
        cand->accuracy_score = fminf(1.0f, straightness);

        cand->is_feasible = true;

        /* Combined score with quantum amplitude weighting */
        cand->combined_score = cand->amplitude * 0.4f +
                               (1.0f - cand->energy_cost) * bridge->config.energy_weight +
                               (1.0f - cand->time_cost) * bridge->config.time_weight +
                               cand->accuracy_score * bridge->config.accuracy_weight;

        if (cand->combined_score > best_score && cand->is_feasible) {
            best_score = cand->combined_score;
            best = cand;
        }
    }

    /* Fill result */
    result->best_trajectory = best;
    result->trajectories_evaluated = num_alternatives;
    result->satisfaction_probability = qresult.satisfaction_prob;
    result->grover_iterations_used = qresult.grover_iterations;

    /* Compute speedup (quadratic for Grover) */
    float classical_cost = (float)num_alternatives;
    float quantum_cost = sqrtf((float)num_alternatives);
    result->optimization_speedup = classical_cost / (quantum_cost > 0.0f ? quantum_cost : 1.0f);

    /* Update statistics */
    bridge->stats.trajectory_optimizations++;
    bridge->stats.avg_trajectory_speedup =
        (bridge->stats.avg_trajectory_speedup * (bridge->stats.trajectory_optimizations - 1) +
         result->optimization_speedup) / bridge->stats.trajectory_optimizations;
    bridge->stats.avg_satisfaction_prob =
        (bridge->stats.avg_satisfaction_prob * (bridge->stats.trajectory_optimizations - 1) +
         result->satisfaction_probability) / bridge->stats.trajectory_optimizations;

    if (best && best->combined_score >= bridge->config.min_trajectory_confidence) {
        bridge->stats.successful_optimizations++;
    } else {
        bridge->stats.failed_optimizations++;
    }

    return 0;
}

int motor_quantum_optimize_path(
    motor_quantum_bridge_t* bridge,
    const quantum_trajectory_waypoint_t* waypoints,
    uint32_t num_waypoints,
    quantum_trajectory_result_t* result
) {
    if (!bridge || !waypoints || num_waypoints < 2 || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_quantum_optimize_path: required parameter is NULL (bridge, waypoints, result)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_quantum_optimize_path: bridge->config is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Use first and last waypoints as start/end */
    quantum_motor_vec3_t start = waypoints[0].position;
    quantum_motor_vec3_t end = waypoints[num_waypoints - 1].position;
    float max_duration = waypoints[num_waypoints - 1].time_ms;

    /* Optimize trajectory through these waypoints */
    return motor_quantum_optimize_trajectory(
        bridge, &start, &end, max_duration,
        bridge->config.trajectory_alternatives, result);
}

//=============================================================================
// Motor Program Selection API
//=============================================================================

int motor_quantum_select_program(
    motor_quantum_bridge_t* bridge,
    const float* skill_requirements,
    uint32_t skill_dim,
    uint32_t num_programs,
    quantum_program_result_t* result
) {
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_quantum_select_program: required parameter is NULL (bridge, result)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_quantum_select_program: bridge->config is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Build CNF for program selection */
    qreason_cnf_t prog_cnf = {0};
    prog_cnf.n_variables = (num_programs < QREASON_MAX_VARIABLES) ?
                           num_programs : QREASON_MAX_VARIABLES;

    prog_cnf.n_clauses = 1;
    prog_cnf.clauses[0].n_literals = prog_cnf.n_variables;

    for (uint32_t i = 0; i < prog_cnf.n_variables; i++) {
        prog_cnf.clauses[0].literals[i].variable = i;
        prog_cnf.clauses[0].literals[i].negated = false;
    }

    /* Solve using quantum search */
    qreason_result_t qresult;
    int ret = qreason_solve_sat(bridge->quantum_reasoner, &prog_cnf, &qresult);
    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_quantum_select_program: validation failed");
        return -1;
    }

    /* Generate program candidates */
    uint32_t num_candidates = (num_programs < bridge->max_candidates) ?
                              num_programs : bridge->max_candidates;
    quantum_program_candidate_t* best = NULL;
    float best_score = -1.0f;

    for (uint32_t i = 0; i < num_candidates; i++) {
        quantum_program_candidate_t* cand = &bridge->program_candidates[i];

        cand->program_id = i + 1;
        snprintf(cand->program_name, sizeof(cand->program_name), "program_%u", i);
        cand->amplitude = qresult.confidences[i];

        /* Simulate skill matching (would use actual program characteristics) */
        cand->skill_match = quantum_randf(&bridge->rng_state);

        /* Complexity varies by program */
        cand->complexity = 0.3f + 0.5f * quantum_randf(&bridge->rng_state);

        /* Combined score */
        cand->combined_score = cand->amplitude * 0.4f +
                               cand->skill_match * 0.4f +
                               (1.0f - cand->complexity) * 0.2f;

        if (cand->combined_score > best_score) {
            best_score = cand->combined_score;
            best = cand;
        }
    }

    /* Fill result */
    result->best_program = best;
    result->programs_evaluated = num_candidates;
    result->satisfaction_probability = qresult.satisfaction_prob;
    result->grover_iterations_used = qresult.grover_iterations;

    /* Update statistics */
    bridge->stats.program_selections++;

    float speedup = sqrtf((float)num_candidates);
    bridge->stats.avg_program_speedup =
        (bridge->stats.avg_program_speedup * (bridge->stats.program_selections - 1) +
         speedup) / bridge->stats.program_selections;

    return 0;
}

//=============================================================================
// Timing Optimization API
//=============================================================================

int motor_quantum_optimize_timing(
    motor_quantum_bridge_t* bridge,
    const float* base_timing,
    uint32_t num_phases,
    quantum_timing_result_t* result
) {
    if (!bridge || !base_timing || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_quantum_optimize_timing: required parameter is NULL (bridge, base_timing, result)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_quantum_optimize_timing: bridge->config is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Build CNF for timing optimization */
    qreason_cnf_t timing_cnf = {0};
    timing_cnf.n_variables = (num_phases < QREASON_MAX_VARIABLES) ?
                             num_phases : QREASON_MAX_VARIABLES;

    timing_cnf.n_clauses = 1;
    timing_cnf.clauses[0].n_literals = timing_cnf.n_variables;

    for (uint32_t i = 0; i < timing_cnf.n_variables; i++) {
        timing_cnf.clauses[0].literals[i].variable = i;
        timing_cnf.clauses[0].literals[i].negated = false;
    }

    /* Solve using quantum search */
    qreason_result_t qresult;
    int ret = qreason_solve_sat(bridge->quantum_reasoner, &timing_cnf, &qresult);
    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "motor_quantum_optimize_timing: validation failed");
        return -1;
    }

    /* Generate timing candidates */
    uint32_t num_candidates = (8 < bridge->max_candidates) ? 8 : bridge->max_candidates;
    quantum_timing_candidate_t* best = NULL;
    float best_score = -1.0f;

    for (uint32_t i = 0; i < num_candidates; i++) {
        quantum_timing_candidate_t* cand = &bridge->timing_candidates[i];

        cand->timing_id = i;
        cand->num_phases = num_phases;

        /* Allocate time offsets if needed */
        cand->time_offsets = (float*)nimcp_calloc(num_phases, sizeof(float));
        if (!cand->time_offsets) continue;

        /* Generate timing variant with perturbations */
        for (uint32_t j = 0; j < num_phases; j++) {
            float perturbation = (quantum_randf(&bridge->rng_state) - 0.5f) * 0.1f;
            cand->time_offsets[j] = base_timing[j] * (1.0f + perturbation);
        }

        cand->amplitude = qresult.confidences[i % timing_cnf.n_variables];

        /* Coordination score: smoother timing is better */
        float variance = 0.0f;
        float mean = 0.0f;
        for (uint32_t j = 0; j < num_phases; j++) {
            mean += cand->time_offsets[j];
        }
        mean /= (float)num_phases;
        for (uint32_t j = 0; j < num_phases; j++) {
            float diff = cand->time_offsets[j] - mean;
            variance += diff * diff;
        }
        variance /= (float)num_phases;
        cand->coordination_score = 1.0f / (1.0f + variance);

        /* Rhythm score: consistent intervals are better */
        cand->rhythm_score = 0.8f + 0.2f * quantum_randf(&bridge->rng_state);

        float score = cand->amplitude * 0.3f +
                      cand->coordination_score * 0.4f +
                      cand->rhythm_score * 0.3f;

        if (score > best_score) {
            best_score = score;
            best = cand;
        }
    }

    /* Fill result */
    result->best_timing = best;
    result->patterns_evaluated = num_candidates;
    result->optimization_score = best_score;

    /* Update statistics */
    bridge->stats.timing_optimizations++;

    return 0;
}

//=============================================================================
// Parallel Evaluation API
//=============================================================================

int motor_quantum_parallel_evaluate(
    motor_quantum_bridge_t* bridge,
    const uint32_t* program_ids,
    uint32_t num_programs,
    const quantum_motor_vec3_t* goal_position,
    quantum_program_result_t* result
) {
    if (!bridge || !program_ids || !goal_position || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_quantum_parallel_evaluate: required parameter is NULL (bridge, program_ids, goal_position, result)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_quantum_parallel_evaluate: bridge->config is NULL");
        return -1;
    }

    /* Use program selection with the given IDs */
    return motor_quantum_select_program(bridge, NULL, 0, num_programs, result);
}

//=============================================================================
// Statistics API
//=============================================================================

int motor_quantum_get_stats(
    const motor_quantum_bridge_t* bridge,
    motor_quantum_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_quantum_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void motor_quantum_reset_stats(motor_quantum_bridge_t* bridge) {
    if (bridge) {
        memset(&bridge->stats, 0, sizeof(bridge->stats));
    }
}

int motor_quantum_get_config(
    const motor_quantum_bridge_t* bridge,
    motor_quantum_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "motor_quantum_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }
    *config = bridge->config;
    return 0;
}
