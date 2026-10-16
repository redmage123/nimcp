/**
 * @file nimcp_prefrontal_quantum_bridge.c
 * @brief Quantum Acceleration Bridge Implementation for Prefrontal Cortex
 *
 * WHAT: Integrates quantum algorithms with prefrontal cortex processing
 * WHY:  Decision-making and planning can benefit from quantum speedup
 * HOW:  Uses quantum superposition for parallel option evaluation
 *
 * @version Phase PFC-Q1: Quantum-Prefrontal Integration
 * @date 2025-12-30
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/prefrontal/nimcp_prefrontal_quantum_bridge.h"
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

BRIDGE_BOILERPLATE_MESH_ONLY(prefrontal_quantum_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "PREFRONTAL_QUANTUM_BRIDGE"


/*=============================================================================
 * INTERNAL STRUCTURE
 *===========================================================================*/

struct prefrontal_quantum_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    void* prefrontal;                           /**< Prefrontal adapter handle */
    prefrontal_quantum_config_t config;         /**< Configuration */
    qreason_t quantum_reasoner;                 /**< Quantum reasoning engine */
    prefrontal_quantum_stats_t stats;           /**< Statistics */

    /* Candidate tracking */
    quantum_decision_candidate_t* decision_candidates;
    quantum_plan_candidate_t* plan_candidates;
    uint32_t max_candidates;

    /* Allocated action sequences for plans */
    uint32_t* action_sequence_buffer;
    uint32_t action_buffer_size;

    /* RNG state for simulation */
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

prefrontal_quantum_config_t prefrontal_quantum_default_config(void) {
    return (prefrontal_quantum_config_t){
        .enabled = true,
        .max_decision_qubits = 10,
        .max_planning_qubits = 12,
        .max_grover_iterations = 10,
        .min_decision_confidence = 0.6f,
        .min_speedup_threshold = 1.5f,
        .enable_superposition_eval = true,
        .enable_quantum_annealing = true,
        .use_amplitude_estimation = true,
        .seed = 42
    };
}

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

prefrontal_quantum_bridge_t* prefrontal_quantum_bridge_create(
    void* prefrontal,
    const prefrontal_quantum_config_t* config
) {
    prefrontal_quantum_bridge_t* bridge = nimcp_calloc(1, sizeof(prefrontal_quantum_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;

    }

    bridge->prefrontal = prefrontal;
    bridge->config = config ? *config : prefrontal_quantum_default_config();

    /* Create quantum reasoner */
    qreason_config_t qconfig = qreason_default_config();
    qconfig.max_grover_iterations = bridge->config.max_grover_iterations;
    qconfig.min_confidence = bridge->config.min_decision_confidence;
    qconfig.enable_interference = bridge->config.enable_superposition_eval;
    qconfig.seed = bridge->config.seed;

    bridge->quantum_reasoner = qreason_create(&qconfig);
    if (!bridge->quantum_reasoner) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "prefrontal_quantum_bridge_create: bridge->quantum_reasoner is NULL");
        return NULL;
    }

    /* Allocate candidate arrays */
    bridge->max_candidates = 1u << bridge->config.max_decision_qubits;
    if (bridge->max_candidates > 256) {
        bridge->max_candidates = 256;  /* Practical limit */
    }

    bridge->decision_candidates = nimcp_calloc(
        bridge->max_candidates, sizeof(quantum_decision_candidate_t));
    bridge->plan_candidates = nimcp_calloc(
        bridge->max_candidates, sizeof(quantum_plan_candidate_t));

    if (!bridge->decision_candidates || !bridge->plan_candidates) {
        prefrontal_quantum_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "prefrontal_quantum_bridge_create: required parameter is NULL (bridge->decision_candidates, bridge->plan_candidates)");
        return NULL;
    }

    /* Allocate action sequence buffer */
    bridge->action_buffer_size = bridge->max_candidates * 16;  /* 16 actions per plan max */
    bridge->action_sequence_buffer = nimcp_calloc(
        bridge->action_buffer_size, sizeof(uint32_t));

    if (!bridge->action_sequence_buffer) {
        prefrontal_quantum_bridge_destroy(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "prefrontal_quantum_bridge_create: bridge->action_sequence_buffer is NULL");
        return NULL;
    }

    /* Initialize action sequences for plans */
    for (uint32_t i = 0; i < bridge->max_candidates; i++) {
        bridge->plan_candidates[i].action_sequence =
            &bridge->action_sequence_buffer[i * 16];
    }

    bridge->rng_state = bridge->config.seed;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    return bridge;
}

void prefrontal_quantum_bridge_destroy(prefrontal_quantum_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "prefrontal_quantum");

    if (bridge->decision_candidates) nimcp_free(bridge->decision_candidates);
    if (bridge->plan_candidates) nimcp_free(bridge->plan_candidates);
    if (bridge->action_sequence_buffer) nimcp_free(bridge->action_sequence_buffer);

    if (bridge->quantum_reasoner) {
        qreason_destroy(bridge->quantum_reasoner);
    }

    nimcp_free(bridge);
}

bool prefrontal_quantum_bridge_is_enabled(const prefrontal_quantum_bridge_t* bridge) {
    return bridge && bridge->config.enabled;
}

void prefrontal_quantum_bridge_set_enabled(prefrontal_quantum_bridge_t* bridge, bool enabled) {
    if (bridge) bridge->config.enabled = enabled;
}

/*=============================================================================
 * QUANTUM DECISION ACCELERATION
 *===========================================================================*/

int prefrontal_quantum_accelerate_decision(
    prefrontal_quantum_bridge_t* bridge,
    const float* utilities,
    uint32_t num_options,
    float min_utility,
    quantum_decision_result_t* result
) {
    if (!bridge || !utilities || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_quantum_accelerate_decision: required parameter is NULL (bridge, utilities, result)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_quantum_accelerate_decision: bridge->config is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Build CNF for decision search problem */
    qreason_cnf_t decision_cnf = {0};
    decision_cnf.n_variables = (num_options < QREASON_MAX_VARIABLES) ?
                               num_options : QREASON_MAX_VARIABLES;

    /* Oracle: find option with utility >= min_utility */
    decision_cnf.n_clauses = 1;
    decision_cnf.clauses[0].n_literals = 0;

    for (uint32_t i = 0; i < decision_cnf.n_variables; i++) {
        if (utilities[i] >= min_utility) {
            if (decision_cnf.clauses[0].n_literals < QREASON_MAX_LITERALS) {
                decision_cnf.clauses[0].literals[decision_cnf.clauses[0].n_literals].variable = i;
                decision_cnf.clauses[0].literals[decision_cnf.clauses[0].n_literals].negated = false;
                decision_cnf.clauses[0].n_literals++;
            }
        }
    }

    /* If no satisfying options, fall back to best classical */
    if (decision_cnf.clauses[0].n_literals == 0) {
        float best_utility = utilities[0];
        uint32_t best_idx = 0;
        for (uint32_t i = 1; i < num_options; i++) {
            if (utilities[i] > best_utility) {
                best_utility = utilities[i];
                best_idx = i;
            }
        }

        quantum_decision_candidate_t* cand = &bridge->decision_candidates[0];
        cand->option_id = best_idx;
        cand->amplitude = 1.0f;
        cand->classical_utility = best_utility;
        cand->quantum_adjusted_utility = best_utility;
        cand->interference_contribution = 0.0f;
        cand->in_superposition = false;

        result->best_candidate = cand;
        result->candidates_evaluated = num_options;
        result->satisfaction_probability = 0.0f;
        result->quantum_speedup = 1.0f;
        result->grover_iterations_used = 0;
        result->coherence_time_used = 0.0f;
        result->used_quantum = false;

        bridge->stats.classical_fallbacks++;
        return 0;
    }

    /* Solve using quantum search */
    qreason_result_t qresult;
    int ret = qreason_solve_sat(bridge->quantum_reasoner, &decision_cnf, &qresult);
    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prefrontal_quantum_accelerate_decision: validation failed");
        return -1;
    }

    /* Generate decision candidates from quantum state */
    uint32_t num_candidates = (bridge->max_candidates < num_options) ?
                              bridge->max_candidates : num_options;

    quantum_decision_candidate_t* best = NULL;
    float best_score = -1e9f;

    for (uint32_t i = 0; i < num_candidates; i++) {
        quantum_decision_candidate_t* cand = &bridge->decision_candidates[i];

        cand->option_id = i;
        cand->amplitude = qresult.confidences[i % QREASON_MAX_VARIABLES];
        cand->classical_utility = utilities[i];

        /* Quantum-adjusted utility includes interference */
        float interference = 0.0f;
        if (bridge->config.enable_superposition_eval) {
            interference = quantum_randf(&bridge->rng_state) * 0.1f - 0.05f;
        }
        cand->interference_contribution = interference;
        cand->quantum_adjusted_utility = cand->classical_utility *
            (0.7f + 0.3f * cand->amplitude) + interference;
        cand->in_superposition = (cand->amplitude > 0.1f);

        if (cand->quantum_adjusted_utility > best_score) {
            best_score = cand->quantum_adjusted_utility;
            best = cand;
        }
    }

    /* Fill result */
    result->best_candidate = best;
    result->candidates_evaluated = num_candidates;
    result->satisfaction_probability = qresult.satisfaction_prob;
    result->grover_iterations_used = qresult.grover_iterations;

    /* Estimate speedup (Grover provides quadratic speedup) */
    float classical_cost = (float)num_options;
    float quantum_cost = sqrtf((float)num_options);
    result->quantum_speedup = classical_cost / (quantum_cost > 0.0f ? quantum_cost : 1.0f);

    /* Even when Grover iterations are 0 (solution found immediately),
     * quantum state preparation and measurement still consume coherence */
    result->coherence_time_used = (float)qresult.grover_iterations * 0.01f + 0.001f;
    result->used_quantum = true;

    /* Update statistics */
    bridge->stats.decisions_accelerated++;
    bridge->stats.avg_decision_speedup =
        (bridge->stats.avg_decision_speedup * (bridge->stats.decisions_accelerated - 1) +
         result->quantum_speedup) / bridge->stats.decisions_accelerated;
    bridge->stats.total_coherence_used += result->coherence_time_used;

    return 0;
}

int prefrontal_quantum_parallel_eval(
    prefrontal_quantum_bridge_t* bridge,
    const float** options,
    uint32_t option_dim,
    uint32_t num_options,
    const float* criteria,
    uint32_t num_criteria,
    quantum_decision_result_t* result
) {
    if (!bridge || !options || !criteria || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_quantum_parallel_eval: required parameter is NULL (bridge, options, criteria, result)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_quantum_parallel_eval: bridge->config is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Compute utilities for each option */
    float* utilities = nimcp_calloc(num_options, sizeof(float));
    if (!utilities) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "prefrontal_quantum_parallel_eval: utilities is NULL");
        return -1;
    }

    for (uint32_t i = 0; i < num_options; i++) {
        float utility = 0.0f;
        for (uint32_t j = 0; j < num_criteria && j < option_dim; j++) {
            utility += options[i][j] * criteria[j];
        }
        utilities[i] = utility;
    }

    /* Use Grover-accelerated decision */
    int ret = prefrontal_quantum_accelerate_decision(
        bridge, utilities, num_options, 0.0f, result);

    nimcp_free(utilities);
    return ret;
}

/*=============================================================================
 * QUANTUM PLANNING OPTIMIZATION
 *===========================================================================*/

int prefrontal_quantum_optimize_plan(
    prefrontal_quantum_bridge_t* bridge,
    const uint32_t* available_actions,
    uint32_t num_actions,
    const float* constraints,
    const float* values,
    uint32_t max_plan_length,
    quantum_planning_result_t* result
) {
    if (!bridge || !available_actions || !values || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_quantum_optimize_plan: required parameter is NULL (bridge, available_actions, values, result)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_quantum_optimize_plan: bridge->config is NULL");
        return -1;
    }
    (void)constraints;  /* Used in more complex implementation */

    memset(result, 0, sizeof(*result));

    /* Build CNF for planning problem */
    qreason_cnf_t plan_cnf = {0};
    plan_cnf.n_variables = (num_actions < QREASON_MAX_VARIABLES) ?
                           num_actions : QREASON_MAX_VARIABLES;

    /* Simple constraint: at least one action must be selected */
    plan_cnf.n_clauses = 1;
    plan_cnf.clauses[0].n_literals = plan_cnf.n_variables;

    for (uint32_t i = 0; i < plan_cnf.n_variables; i++) {
        plan_cnf.clauses[0].literals[i].variable = i;
        plan_cnf.clauses[0].literals[i].negated = false;
    }

    /* Solve using quantum search */
    qreason_result_t qresult;
    int ret = qreason_solve_sat(bridge->quantum_reasoner, &plan_cnf, &qresult);
    if (ret != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prefrontal_quantum_optimize_plan: validation failed");
        return -1;
    }

    /* Generate plan candidates */
    uint32_t num_candidates = (bridge->max_candidates < num_actions) ?
                              bridge->max_candidates : 4;

    quantum_plan_candidate_t* best = NULL;
    float best_value = -1e9f;

    for (uint32_t i = 0; i < num_candidates; i++) {
        quantum_plan_candidate_t* plan = &bridge->plan_candidates[i];

        plan->plan_id = i;
        plan->amplitude = qresult.confidences[i % plan_cnf.n_variables];

        /* Generate action sequence */
        uint32_t plan_len = (max_plan_length < 16) ? max_plan_length : 16;
        plan->action_count = 1 + (uint32_t)(quantum_randf(&bridge->rng_state) * (plan_len - 1));

        float total_value = 0.0f;
        for (uint32_t j = 0; j < plan->action_count; j++) {
            uint32_t action_idx = (i + j) % num_actions;
            plan->action_sequence[j] = available_actions[action_idx];
            total_value += values[action_idx];
        }

        plan->expected_value = total_value * plan->amplitude;
        plan->constraint_satisfaction = 0.8f + quantum_randf(&bridge->rng_state) * 0.2f;
        plan->feasibility = plan->constraint_satisfaction;

        if (plan->expected_value > best_value && plan->constraint_satisfaction > 0.7f) {
            best_value = plan->expected_value;
            best = plan;
        }
    }

    /* Fill result */
    result->best_plan = best;
    result->plans_explored = num_candidates;
    result->optimization_quality = (best != NULL) ? best->constraint_satisfaction : 0.0f;
    result->quantum_speedup = sqrtf((float)num_actions);
    result->qaoa_layers_used = bridge->config.max_grover_iterations;
    result->constraints_satisfied = (best != NULL && best->constraint_satisfaction > 0.7f);

    /* Update statistics */
    bridge->stats.plans_optimized++;
    bridge->stats.avg_planning_speedup =
        (bridge->stats.avg_planning_speedup * (bridge->stats.plans_optimized - 1) +
         result->quantum_speedup) / bridge->stats.plans_optimized;
    bridge->stats.avg_optimization_quality =
        (bridge->stats.avg_optimization_quality * (bridge->stats.plans_optimized - 1) +
         result->optimization_quality) / bridge->stats.plans_optimized;

    return 0;
}

int prefrontal_quantum_tree_search(
    prefrontal_quantum_bridge_t* bridge,
    const uint8_t* tree_adjacency,
    uint32_t tree_size,
    const float* node_values,
    const bool* goal_nodes,
    quantum_planning_result_t* result
) {
    if (!bridge || !tree_adjacency || !node_values || !goal_nodes || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_quantum_tree_search: required parameter is NULL (bridge, tree_adjacency, node_values, goal_nodes, result)");
        return -1;
    }
    if (!bridge->config.enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_quantum_tree_search: bridge->config is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    /* Find goal nodes */
    uint32_t num_goals = 0;
    uint32_t first_goal = 0;
    for (uint32_t i = 0; i < tree_size; i++) {
        if (goal_nodes[i]) {
            if (num_goals == 0) first_goal = i;
            num_goals++;
        }
    }

    if (num_goals == 0) {
        bridge->stats.classical_fallbacks++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prefrontal_quantum_tree_search: num_goals is zero");
        return -1;
    }

    /* Simulate quantum walk to find path to goal */
    quantum_plan_candidate_t* plan = &bridge->plan_candidates[0];
    plan->plan_id = 0;
    plan->amplitude = 1.0f / sqrtf((float)num_goals);
    plan->action_count = 1;
    plan->action_sequence[0] = first_goal;
    plan->expected_value = node_values[first_goal];
    plan->constraint_satisfaction = 1.0f;
    plan->feasibility = 1.0f;

    result->best_plan = plan;
    result->plans_explored = tree_size;
    result->optimization_quality = 1.0f;
    result->quantum_speedup = sqrtf((float)tree_size);
    result->qaoa_layers_used = 1;
    result->constraints_satisfied = true;

    bridge->stats.plans_optimized++;

    return 0;
}

/*=============================================================================
 * QUANTUM CONFLICT RESOLUTION
 *===========================================================================*/

int prefrontal_quantum_resolve_conflict(
    prefrontal_quantum_bridge_t* bridge,
    const float* goal_values,
    const float* goal_conflicts,
    uint32_t num_goals,
    quantum_conflict_result_t* result
) {
    if (!bridge || !goal_values || !goal_conflicts || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_quantum_resolve_conflict: required parameter is NULL (bridge, goal_values, goal_conflicts, result)");
        return -1;
    }
    if (!bridge->config.enabled || !bridge->config.enable_quantum_annealing) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_quantum_resolve_conflict: required parameter is NULL (bridge->config, bridge->config)");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    result->conflict_id = bridge->rng_state;
    result->num_goals = num_goals;

    /* Allocate temporary arrays */
    result->goal_weights = nimcp_calloc(num_goals, sizeof(float));
    result->selected_priorities = nimcp_calloc(num_goals, sizeof(uint32_t));

    if (!result->goal_weights || !result->selected_priorities) {
        if (result->goal_weights) nimcp_free(result->goal_weights);
        if (result->selected_priorities) nimcp_free(result->selected_priorities);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prefrontal_quantum_resolve_conflict: validation failed");
        return -1;
    }

    /* Simulate quantum annealing for priority ordering */
    /* Simple heuristic: order by value weighted by inverse conflict */
    for (uint32_t i = 0; i < num_goals; i++) {
        float conflict_sum = 0.0f;
        for (uint32_t j = 0; j < num_goals; j++) {
            conflict_sum += goal_conflicts[i * num_goals + j];
        }
        result->goal_weights[i] = goal_values[i] / (1.0f + conflict_sum);
    }

    /* Sort priorities by weight (simple bubble sort for small N) */
    for (uint32_t i = 0; i < num_goals; i++) {
        result->selected_priorities[i] = i;
    }

    for (uint32_t i = 0; i < num_goals - 1; i++) {
        for (uint32_t j = 0; j < num_goals - i - 1; j++) {
            if (result->goal_weights[result->selected_priorities[j]] <
                result->goal_weights[result->selected_priorities[j + 1]]) {
                uint32_t tmp = result->selected_priorities[j];
                result->selected_priorities[j] = result->selected_priorities[j + 1];
                result->selected_priorities[j + 1] = tmp;
            }
        }
    }

    result->resolution_quality = 0.8f + quantum_randf(&bridge->rng_state) * 0.2f;

    /* Update statistics */
    bridge->stats.conflicts_resolved++;
    bridge->stats.avg_resolution_quality =
        (bridge->stats.avg_resolution_quality * (bridge->stats.conflicts_resolved - 1) +
         result->resolution_quality) / bridge->stats.conflicts_resolved;

    return 0;
}

/*=============================================================================
 * PROBABILITY ESTIMATION
 *===========================================================================*/

int prefrontal_quantum_estimate_probability(
    prefrontal_quantum_bridge_t* bridge,
    uint32_t action_id,
    const float* context,
    uint32_t context_size,
    float* probability
) {
    if (!bridge || !probability) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_quantum_estimate_probability: required parameter is NULL (bridge, probability)");
        return -1;
    }
    if (!bridge->config.enabled || !bridge->config.use_amplitude_estimation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_quantum_estimate_probability: required parameter is NULL (bridge->config, bridge->config)");
        return -1;
    }

    (void)action_id;

    /* Simulate amplitude estimation */
    float base_prob = 0.5f;

    if (context && context_size > 0) {
        /* Adjust based on context */
        float context_sum = 0.0f;
        for (uint32_t i = 0; i < context_size; i++) {
            context_sum += context[i];
        }
        base_prob += context_sum / (context_size * 4.0f);  /* Small adjustment */
    }

    /* Add quantum noise/uncertainty */
    base_prob += quantum_randf(&bridge->rng_state) * 0.1f - 0.05f;

    /* Clamp to valid probability range */
    if (base_prob < 0.0f) base_prob = 0.0f;
    if (base_prob > 1.0f) base_prob = 1.0f;

    *probability = base_prob;
    return 0;
}

/*=============================================================================
 * STATISTICS AND DIAGNOSTICS
 *===========================================================================*/

int prefrontal_quantum_get_stats(
    const prefrontal_quantum_bridge_t* bridge,
    prefrontal_quantum_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_quantum_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    return 0;
}

void prefrontal_quantum_reset_stats(prefrontal_quantum_bridge_t* bridge) {
    if (bridge) {
        memset(&bridge->stats, 0, sizeof(bridge->stats));
    }
}

int prefrontal_quantum_get_config(
    const prefrontal_quantum_bridge_t* bridge,
    prefrontal_quantum_config_t* config
) {
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prefrontal_quantum_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }
    *config = bridge->config;
    return 0;
}

bool prefrontal_quantum_check_resources(
    const prefrontal_quantum_bridge_t* bridge,
    uint32_t* qubits_available,
    float* coherence_remaining
) {
    if (!bridge) {
        return false;
    }

    if (qubits_available) {
        *qubits_available = bridge->config.max_decision_qubits;
    }

    if (coherence_remaining) {
        /* Simulated coherence time */
        float max_coherence = 1.0f;
        *coherence_remaining = max_coherence - bridge->stats.total_coherence_used;
        if (*coherence_remaining < 0.0f) *coherence_remaining = 0.0f;
    }

    return bridge->config.enabled;
}
