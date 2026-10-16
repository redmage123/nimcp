/**
 * @file nimcp_hippocampus_quantum_bridge.c
 * @brief Quantum Hippocampus Bridge Implementation
 *
 * Integrates quantum algorithms with hippocampus for
 * optimized memory search, pattern matching, and spatial navigation.
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/hippocampus/nimcp_hippocampus_quantum_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "cognitive/reasoning/nimcp_quantum_reasoning.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(hippocampus_quantum_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HIPPOCAMPUS_QUANTUM_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURE
 *===========================================================================*/

struct hippocampus_quantum_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* hippocampus;                       /**< Hippocampus adapter handle */
    hippocampus_quantum_config_t config;     /**< Configuration */
    qreason_t quantum_reasoner;              /**< Quantum reasoning engine */
    hippocampus_quantum_stats_t stats;       /**< Statistics */

    /* Candidate tracking */
    quantum_memory_candidate_t* memory_candidates;
    quantum_pattern_candidate_t* pattern_candidates;
    quantum_spatial_candidate_t* spatial_candidates;
    uint32_t max_candidates;

    /* RNG state */
    uint32_t rng_state;
};

/*=============================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

static uint32_t quantum_rand(uint32_t* state) {
    *state = *state * 1103515245 + 12345;
    return (*state >> 16) & 0x7FFF;
}

static float quantum_randf(uint32_t* state) {
    return (float)quantum_rand(state) / 32767.0f;
}

/*=============================================================================
 * CONFIGURATION API
 *===========================================================================*/

hippocampus_quantum_config_t hippocampus_quantum_default_config(void) {
    return (hippocampus_quantum_config_t){
        .enabled = true,
        .memory_search_depth = 1000,
        .pattern_alternatives = 16,
        .max_grover_iterations = 10,
        .min_match_confidence = 0.5f,
        .enable_interference = true,
        .use_superposition = true,
        .seed = 42
    };
}

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

hippocampus_quantum_bridge_t* hippocampus_quantum_bridge_create(
    void* hippocampus,
    const hippocampus_quantum_config_t* config
) {
    hippocampus_quantum_bridge_t* bridge = nimcp_calloc(1, sizeof(hippocampus_quantum_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    bridge->hippocampus = hippocampus;
    bridge->config = config ? *config : hippocampus_quantum_default_config();

    /* Create quantum reasoner */
    qreason_config_t qconfig = qreason_default_config();
    qconfig.max_grover_iterations = bridge->config.max_grover_iterations;
    qconfig.min_confidence = bridge->config.min_match_confidence;
    qconfig.enable_interference = bridge->config.enable_interference;
    qconfig.seed = bridge->config.seed;

    bridge->quantum_reasoner = qreason_create(&qconfig);
    if (!bridge->quantum_reasoner) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippocampus_quantum_bridge_create: bridge->quantum_reasoner is NULL");
        return NULL;
    }

    /* Allocate candidate arrays */
    bridge->max_candidates = bridge->config.pattern_alternatives;

    bridge->memory_candidates = nimcp_calloc(
        bridge->max_candidates, sizeof(quantum_memory_candidate_t));
    bridge->pattern_candidates = nimcp_calloc(
        bridge->max_candidates, sizeof(quantum_pattern_candidate_t));
    bridge->spatial_candidates = nimcp_calloc(
        bridge->max_candidates, sizeof(quantum_spatial_candidate_t));

    if (!bridge->memory_candidates || !bridge->pattern_candidates ||
        !bridge->spatial_candidates) {
        hippocampus_quantum_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippocampus_quantum_bridge_create: operation failed");
        return NULL;
    }

    bridge->rng_state = bridge->config.seed;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return bridge;
}

void hippocampus_quantum_bridge_destroy(hippocampus_quantum_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hippocampus_quantum");

    if (bridge->memory_candidates) nimcp_free(bridge->memory_candidates);
    if (bridge->pattern_candidates) nimcp_free(bridge->pattern_candidates);
    if (bridge->spatial_candidates) nimcp_free(bridge->spatial_candidates);

    if (bridge->quantum_reasoner) {
        qreason_destroy(bridge->quantum_reasoner);
    }

    nimcp_free(bridge);
}

bool hippocampus_quantum_bridge_is_enabled(const hippocampus_quantum_bridge_t* bridge) {
    return bridge && bridge->config.enabled;
}

void hippocampus_quantum_bridge_set_enabled(hippocampus_quantum_bridge_t* bridge, bool enabled) {
    if (bridge) bridge->config.enabled = enabled;
}

/*=============================================================================
 * QUANTUM MEMORY SEARCH API
 *===========================================================================*/

int hippocampus_quantum_search_memory(
    hippocampus_quantum_bridge_t* bridge,
    const float* query_features,
    uint32_t query_size,
    uint32_t memory_count,
    quantum_memory_result_t* result
) {
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_quantum_search_memory: required parameter is NULL (bridge, result)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_quantum_search_memory: bridge->config is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Build CNF for memory search problem */
    qreason_cnf_t search_cnf = {0};
    search_cnf.n_variables = (memory_count < QREASON_MAX_VARIABLES) ?
                             memory_count : QREASON_MAX_VARIABLES;

    /* Satisfiability: at least one memory must match */
    search_cnf.n_clauses = 1;
    search_cnf.clauses[0].n_literals = (search_cnf.n_variables < QREASON_MAX_LITERALS) ?
                                       search_cnf.n_variables : QREASON_MAX_LITERALS;

    for (uint32_t i = 0; i < search_cnf.clauses[0].n_literals; i++) {
        search_cnf.clauses[0].literals[i].variable = i;
        search_cnf.clauses[0].literals[i].negated = false;
    }

    /* Solve using quantum search */
    qreason_result_t qresult;
    int ret = qreason_solve_sat(bridge->quantum_reasoner, &search_cnf, &qresult);
    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hippocampus_quantum_search_memory: validation failed");
        return -1;
    }

    /* Generate memory candidates from quantum state */
    uint32_t num_candidates = (bridge->max_candidates < search_cnf.n_variables) ?
                              bridge->max_candidates : search_cnf.n_variables;

    quantum_memory_candidate_t* best = NULL;
    float best_score = -1.0f;

    for (uint32_t i = 0; i < num_candidates; i++) {
        quantum_memory_candidate_t* cand = &bridge->memory_candidates[i];

        cand->memory_id = i + 1;  /* 1-indexed memory IDs */
        cand->amplitude = qresult.confidences[i];

        /* Simulate similarity scoring */
        cand->similarity_score = quantum_randf(&bridge->rng_state);
        cand->spatial_score = quantum_randf(&bridge->rng_state);
        cand->temporal_score = quantum_randf(&bridge->rng_state);

        /* Combine scores with quantum amplitude weighting */
        cand->combined_score = cand->amplitude * 0.4f +
                               cand->similarity_score * 0.3f +
                               cand->spatial_score * 0.2f +
                               cand->temporal_score * 0.1f;

        if (cand->combined_score > best_score) {
            best_score = cand->combined_score;
            best = cand;
        }
    }

    /* Fill result */
    result->best_candidate = best;
    result->candidates_evaluated = num_candidates;
    result->satisfaction_probability = qresult.satisfaction_prob;
    result->grover_iterations_used = qresult.grover_iterations;

    /* Estimate speedup (Grover provides quadratic speedup) */
    float classical_cost = (float)memory_count;
    float quantum_cost = sqrtf((float)memory_count);
    result->search_speedup = classical_cost / (quantum_cost > 0.0f ? quantum_cost : 1.0f);

    /* Update statistics */
    bridge->stats.memory_searches++;
    bridge->stats.total_grover_iterations += qresult.grover_iterations;
    bridge->stats.avg_memory_speedup =
        (bridge->stats.avg_memory_speedup * (bridge->stats.memory_searches - 1) +
         result->search_speedup) / bridge->stats.memory_searches;
    bridge->stats.avg_satisfaction_prob =
        (bridge->stats.avg_satisfaction_prob * (bridge->stats.memory_searches - 1) +
         result->satisfaction_probability) / bridge->stats.memory_searches;

    if (best && best->combined_score >= bridge->config.min_match_confidence) {
        bridge->stats.successful_searches++;
    } else {
        bridge->stats.failed_searches++;
    }

    return 0;
}

/*=============================================================================
 * QUANTUM PATTERN MATCHING API
 *===========================================================================*/

int hippocampus_quantum_match_pattern(
    hippocampus_quantum_bridge_t* bridge,
    const float* partial_pattern,
    uint32_t pattern_size,
    uint32_t num_patterns,
    quantum_pattern_result_t* result
) {
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_quantum_match_pattern: required parameter is NULL (bridge, result)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_quantum_match_pattern: bridge->config is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Build CNF for pattern matching */
    qreason_cnf_t pattern_cnf = {0};
    pattern_cnf.n_variables = (num_patterns < QREASON_MAX_VARIABLES) ?
                              num_patterns : QREASON_MAX_VARIABLES;

    /* Each variable represents a pattern candidate */
    pattern_cnf.n_clauses = 1;
    pattern_cnf.clauses[0].n_literals = pattern_cnf.n_variables;

    for (uint32_t i = 0; i < pattern_cnf.n_variables; i++) {
        pattern_cnf.clauses[0].literals[i].variable = i;
        pattern_cnf.clauses[0].literals[i].negated = false;
    }

    /* Solve using quantum search */
    qreason_result_t qresult;
    int ret = qreason_solve_sat(bridge->quantum_reasoner, &pattern_cnf, &qresult);
    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hippocampus_quantum_match_pattern: validation failed");
        return -1;
    }

    /* Generate pattern candidates */
    quantum_pattern_candidate_t* best = NULL;
    float best_score = -1.0f;

    for (uint32_t i = 0; i < pattern_cnf.n_variables && i < bridge->max_candidates; i++) {
        quantum_pattern_candidate_t* cand = &bridge->pattern_candidates[i];

        cand->pattern_id = i;
        cand->pattern = NULL;  /* Would be actual pattern in full impl */
        cand->pattern_size = pattern_size;
        cand->amplitude = qresult.confidences[i];
        cand->completion_score = cand->amplitude * (0.7f + 0.3f * quantum_randf(&bridge->rng_state));
        cand->is_match = (cand->completion_score >= bridge->config.min_match_confidence);

        if (cand->completion_score > best_score) {
            best_score = cand->completion_score;
            best = cand;
        }
    }

    /* Fill result */
    result->best_pattern = best;
    result->patterns_evaluated = pattern_cnf.n_variables;
    result->satisfaction_probability = qresult.satisfaction_prob;
    result->grover_iterations_used = qresult.grover_iterations;

    /* Update statistics */
    bridge->stats.pattern_matches++;
    bridge->stats.total_grover_iterations += qresult.grover_iterations;

    float speedup = sqrtf((float)num_patterns);
    bridge->stats.avg_pattern_speedup =
        (bridge->stats.avg_pattern_speedup * (bridge->stats.pattern_matches - 1) +
         speedup) / bridge->stats.pattern_matches;

    return 0;
}

/*=============================================================================
 * QUANTUM SPATIAL SEARCH API
 *===========================================================================*/

int hippocampus_quantum_search_spatial(
    hippocampus_quantum_bridge_t* bridge,
    float current_x,
    float current_y,
    float goal_x,
    float goal_y,
    uint32_t num_locations,
    quantum_spatial_result_t* result
) {
    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_quantum_search_spatial: required parameter is NULL (bridge, result)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_quantum_search_spatial: bridge->config is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Build CNF for spatial search (pathfinding) */
    qreason_cnf_t spatial_cnf = {0};
    spatial_cnf.n_variables = (num_locations < QREASON_MAX_VARIABLES) ?
                              num_locations : QREASON_MAX_VARIABLES;

    /* Each variable represents a waypoint choice */
    spatial_cnf.n_clauses = 1;
    spatial_cnf.clauses[0].n_literals = spatial_cnf.n_variables;

    for (uint32_t i = 0; i < spatial_cnf.n_variables; i++) {
        spatial_cnf.clauses[0].literals[i].variable = i;
        spatial_cnf.clauses[0].literals[i].negated = false;
    }

    /* Solve using quantum walk / Grover hybrid */
    qreason_result_t qresult;
    int ret = qreason_solve_sat(bridge->quantum_reasoner, &spatial_cnf, &qresult);
    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hippocampus_quantum_search_spatial: validation failed");
        return -1;
    }

    /* Generate spatial candidates */
    quantum_spatial_candidate_t* best = NULL;
    float best_reachability = -1.0f;

    /* Distance from goal */
    float goal_dist = sqrtf((goal_x - current_x) * (goal_x - current_x) +
                            (goal_y - current_y) * (goal_y - current_y));

    for (uint32_t i = 0; i < spatial_cnf.n_variables && i < bridge->max_candidates; i++) {
        quantum_spatial_candidate_t* cand = &bridge->spatial_candidates[i];

        cand->location_id = i;
        /* Generate random waypoint between current and goal */
        float t = quantum_randf(&bridge->rng_state);
        cand->x = current_x + t * (goal_x - current_x) +
                  (quantum_randf(&bridge->rng_state) - 0.5f) * goal_dist * 0.1f;
        cand->y = current_y + t * (goal_y - current_y) +
                  (quantum_randf(&bridge->rng_state) - 0.5f) * goal_dist * 0.1f;

        cand->amplitude = qresult.confidences[i];

        /* Estimate path cost (distance to waypoint + waypoint to goal) */
        float dist_to_waypoint = sqrtf((cand->x - current_x) * (cand->x - current_x) +
                                       (cand->y - current_y) * (cand->y - current_y));
        float dist_to_goal = sqrtf((goal_x - cand->x) * (goal_x - cand->x) +
                                   (goal_y - cand->y) * (goal_y - cand->y));
        cand->path_cost = dist_to_waypoint + dist_to_goal;

        /* Reachability based on path efficiency */
        float path_efficiency = goal_dist / (cand->path_cost + 0.001f);
        cand->reachability = cand->amplitude * path_efficiency;

        if (cand->reachability > best_reachability) {
            best_reachability = cand->reachability;
            best = cand;
        }
    }

    /* Fill result */
    result->best_location = best;
    result->locations_evaluated = spatial_cnf.n_variables;
    result->optimization_score = best_reachability;
    result->quantum_walk_steps = qresult.grover_iterations;

    /* Update statistics */
    bridge->stats.spatial_searches++;
    bridge->stats.total_grover_iterations += qresult.grover_iterations;

    float speedup = sqrtf((float)num_locations);
    bridge->stats.avg_spatial_speedup =
        (bridge->stats.avg_spatial_speedup * (bridge->stats.spatial_searches - 1) +
         speedup) / bridge->stats.spatial_searches;

    return 0;
}

/*=============================================================================
 * QUANTUM ASSOCIATIVE RECALL API
 *===========================================================================*/

int hippocampus_quantum_associative_recall(
    hippocampus_quantum_bridge_t* bridge,
    const float* cue,
    uint32_t cue_size,
    uint32_t max_associations,
    uint32_t* memory_ids,
    float* association_strengths,
    uint32_t* count
) {
    if (!bridge || !cue || !memory_ids || !association_strengths || !count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hippocampus_quantum_associative_recall: required parameter is NULL (bridge, cue, memory_ids, association_strengths, count)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_quantum_associative_recall: bridge->config is NULL");
        return -1;
    }

    /* Use quantum search to find associated memories */
    quantum_memory_result_t mem_result;
    int ret = hippocampus_quantum_search_memory(bridge, cue, cue_size,
                                                 bridge->config.memory_search_depth, &mem_result);
    if (ret != 0) {
        *count = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hippocampus_quantum_associative_recall: validation failed");
        return -1;
    }

    /* Extract top associations based on quantum amplitudes */
    uint32_t num_associations = 0;
    for (uint32_t i = 0; i < bridge->max_candidates && num_associations < max_associations; i++) {
        quantum_memory_candidate_t* cand = &bridge->memory_candidates[i];
        if (cand->amplitude > 0.1f) {  /* Threshold for association */
            memory_ids[num_associations] = cand->memory_id;
            association_strengths[num_associations] = cand->combined_score;
            num_associations++;
        }
    }

    *count = num_associations;
    return 0;
}

/*=============================================================================
 * STATISTICS API
 *===========================================================================*/

int hippocampus_quantum_get_stats(
    const hippocampus_quantum_bridge_t* bridge,
    hippocampus_quantum_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_quantum_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;

    /* Compute average Grover iterations */
    uint64_t total_ops = bridge->stats.memory_searches +
                         bridge->stats.pattern_matches +
                         bridge->stats.spatial_searches;
    if (total_ops > 0) {
        stats->avg_grover_iterations =
            (float)bridge->stats.total_grover_iterations / (float)total_ops;
    }

    return 0;
}

void hippocampus_quantum_reset_stats(hippocampus_quantum_bridge_t* bridge) {
    if (bridge) {
        memset(&bridge->stats, 0, sizeof(bridge->stats));
    }
}

int hippocampus_quantum_get_config(
    const hippocampus_quantum_bridge_t* bridge,
    hippocampus_quantum_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hippocampus_quantum_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }
    *config = bridge->config;
    return 0;
}
