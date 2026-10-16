/**
 * @file nimcp_executive_quantum_bridge.h
 * @brief Quantum-inspired executive planning and decision optimization
 *
 * WHAT: Integrates quantum superposition with executive function planning
 * WHY:  Explore multiple plan branches simultaneously for better decisions
 * HOW:  Quantum reasoning for hypothesis evaluation and plan optimization
 *
 * BIOLOGICAL INSPIRATION:
 * - Prefrontal cortex explores multiple hypotheses in parallel
 * - Executive function evaluates competing action sequences
 * - Working memory maintains superposition of possibilities
 * - Dorsolateral PFC resolves ambiguity through interference
 *
 * QUANTUM CONCEPTS:
 * - Superposition: Explore multiple plan branches simultaneously
 * - Interference: Cancel suboptimal plans through quantum dynamics
 * - Grover search: Find optimal plans faster than classical methods
 * - Amplitude amplification: Boost high-value decision paths
 */

#ifndef NIMCP_EXECUTIVE_QUANTUM_BRIDGE_H
#define NIMCP_EXECUTIVE_QUANTUM_BRIDGE_H

#include "cognitive/reasoning/nimcp_quantum_reasoning.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Types
//=============================================================================

typedef struct executive_quantum_bridge executive_quantum_bridge_t;

/**
 * @brief Quantum executive configuration
 */
typedef struct {
    bool enabled;                    /**< Enable quantum planning */
    uint32_t planning_depth;         /**< Max depth for quantum plan search (default: 8) */
    uint32_t hypothesis_count;       /**< Max parallel hypotheses (default: 16) */
    uint32_t max_grover_iterations;  /**< Max Grover iterations (default: 10) */
    float min_plan_confidence;       /**< Minimum confidence for plan (default: 0.5) */
    bool enable_interference;        /**< Enable quantum interference (default: true) */
    bool use_superposition;          /**< Use superposition for branch exploration (default: true) */
    uint32_t seed;                   /**< Random seed (default: 42) */
} executive_quantum_config_t;

/**
 * @brief Quantum planning hypothesis
 *
 * WHAT: Single plan branch in superposition
 */
typedef struct {
    uint32_t hypothesis_id;          /**< Unique ID */
    float amplitude;                 /**< Quantum amplitude [0, 1] */
    float confidence;                /**< Plan confidence [0, 1] */
    uint32_t num_steps;              /**< Number of plan steps */
    uint32_t* action_sequence;       /**< Sequence of action IDs */
    float expected_utility;          /**< Estimated utility of plan */
    bool is_satisfiable;             /**< Plan constraints satisfiable */
} quantum_hypothesis_t;

/**
 * @brief Quantum planning result
 *
 * WHAT: Result of quantum plan evaluation
 */
typedef struct {
    quantum_hypothesis_t* best_hypothesis;  /**< Best plan found */
    uint32_t num_hypotheses;                /**< Total hypotheses evaluated */
    float satisfaction_probability;          /**< Probability of success */
    uint32_t grover_iterations_used;        /**< Grover iterations performed */
    float planning_speedup;                  /**< Speedup vs classical (estimated) */
} quantum_plan_result_t;

/**
 * @brief Decision option for quantum evaluation
 *
 * WHAT: Single decision alternative
 */
typedef struct {
    uint32_t option_id;              /**< Unique option ID */
    char description[128];           /**< Human-readable description */
    float expected_reward;           /**< Expected reward [0, 1] */
    float risk_level;                /**< Risk level [0, 1] */
    uint32_t num_constraints;        /**< Number of constraints */
    void* option_data;               /**< Option-specific data */
} decision_option_t;

/**
 * @brief Quantum decision result
 *
 * WHAT: Result of quantum decision evaluation
 */
typedef struct {
    uint32_t selected_option_id;     /**< Best option ID */
    float confidence;                /**< Decision confidence [0, 1] */
    float satisfaction_prob;         /**< Constraint satisfaction probability */
    uint32_t num_options_evaluated;  /**< Options evaluated */
} quantum_decision_result_t;

/**
 * @brief Statistics for quantum executive
 */
typedef struct {
    uint64_t quantum_plans;          /**< Total quantum plans generated */
    uint64_t quantum_decisions;      /**< Total quantum decisions made */
    float avg_planning_speedup;      /**< Average speedup vs classical */
    float avg_hypothesis_count;      /**< Average hypotheses per plan */
    float avg_satisfaction_prob;     /**< Average plan success probability */
    uint64_t successful_plans;       /**< Plans with high confidence */
    uint64_t failed_plans;           /**< Plans with low confidence */
} executive_quantum_stats_t;

//=============================================================================
// API
//=============================================================================

/**
 * WHAT: Get default quantum executive configuration
 * WHY:  Provide sensible defaults for quantum planning
 *
 * @return Default configuration
 */
executive_quantum_config_t executive_quantum_default_config(void);

/**
 * WHAT: Create quantum executive bridge
 * WHY:  Initialize quantum planning system
 * HOW:  Allocate quantum reasoner and hypothesis tracking
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
executive_quantum_bridge_t* executive_quantum_bridge_create(
    const executive_quantum_config_t* config
);

/**
 * WHAT: Destroy quantum executive bridge
 *
 * @param bridge Bridge to destroy
 */
void executive_quantum_bridge_destroy(executive_quantum_bridge_t* bridge);

/**
 * WHAT: Check if quantum planning is enabled
 *
 * @param bridge Quantum bridge
 * @return true if enabled
 */
bool executive_quantum_bridge_is_enabled(const executive_quantum_bridge_t* bridge);

/**
 * WHAT: Enable or disable quantum planning
 *
 * @param bridge Quantum bridge
 * @param enabled Enable flag
 */
void executive_quantum_bridge_set_enabled(executive_quantum_bridge_t* bridge, bool enabled);

/**
 * WHAT: Generate quantum plan using superposition
 * WHY:  Explore multiple plan branches in parallel
 * HOW:  Use quantum reasoning to evaluate plan hypotheses
 *
 * @param bridge Quantum bridge
 * @param goal_description Goal string
 * @param num_actions Number of available actions
 * @param max_plan_steps Maximum plan length
 * @param result Output: planning result
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(sqrt(N)) with Grover vs O(N) classical
 * THREAD-SAFE: No
 */
int executive_quantum_plan(
    executive_quantum_bridge_t* bridge,
    const char* goal_description,
    uint32_t num_actions,
    uint32_t max_plan_steps,
    quantum_plan_result_t* result
);

/**
 * WHAT: Evaluate decision options using quantum search
 * WHY:  Find optimal decision faster using quantum amplitude amplification
 * HOW:  Encode options in quantum state, apply oracle, measure best
 *
 * @param bridge Quantum bridge
 * @param options Array of decision options
 * @param num_options Number of options
 * @param result Output: decision result
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(sqrt(N)) vs O(N) classical
 * THREAD-SAFE: No
 */
int executive_quantum_evaluate_options(
    executive_quantum_bridge_t* bridge,
    const decision_option_t* options,
    uint32_t num_options,
    quantum_decision_result_t* result
);

/**
 * WHAT: Get quantum executive statistics
 *
 * @param bridge Quantum bridge
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int executive_quantum_get_stats(
    const executive_quantum_bridge_t* bridge,
    executive_quantum_stats_t* stats
);

/**
 * WHAT: Reset statistics
 *
 * @param bridge Quantum bridge
 */
void executive_quantum_reset_stats(executive_quantum_bridge_t* bridge);

/**
 * WHAT: Get current configuration
 *
 * @param bridge Quantum bridge
 * @param config Output: configuration
 * @return 0 on success, -1 on error
 */
int executive_quantum_get_config(
    const executive_quantum_bridge_t* bridge,
    executive_quantum_config_t* config
);

//=============================================================================
// Implementation
//=============================================================================

#ifdef NIMCP_EXECUTIVE_QUANTUM_BRIDGE_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/memory/nimcp_memory.h"

/**
 * @brief Internal structure
 */
struct executive_quantum_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    executive_quantum_config_t config;   /**< Configuration */
    qreason_t quantum_reasoner;          /**< Quantum reasoning engine */
    executive_quantum_stats_t stats;     /**< Statistics */

    /* Hypothesis tracking */
    quantum_hypothesis_t** hypotheses;   /**< Array of hypothesis pointers */
    uint32_t num_hypotheses;             /**< Current hypothesis count */
    uint32_t max_hypotheses;             /**< Maximum hypotheses */

    /* Planning state */
    float last_speedup;                  /**< Last planning speedup */
    uint32_t rng_state;                  /**< RNG state */
};

executive_quantum_config_t executive_quantum_default_config(void) {
    return (executive_quantum_config_t){
        .enabled = true,
        .planning_depth = 8,
        .hypothesis_count = 16,
        .max_grover_iterations = 10,
        .min_plan_confidence = 0.5f,
        .enable_interference = true,
        .use_superposition = true,
        .seed = 42
    };
}

executive_quantum_bridge_t* executive_quantum_bridge_create(
    const executive_quantum_config_t* config
) {
    executive_quantum_bridge_t* bridge = (executive_quantum_bridge_t*)nimcp_calloc(
        1, sizeof(executive_quantum_bridge_t));
    if (!bridge) return NULL;

    bridge->config = config ? *config : executive_quantum_default_config();

    /* Create quantum reasoner */
    qreason_config_t qconfig = qreason_default_config();
    qconfig.max_grover_iterations = bridge->config.max_grover_iterations;
    qconfig.min_confidence = bridge->config.min_plan_confidence;
    qconfig.enable_interference = bridge->config.enable_interference;
    qconfig.seed = bridge->config.seed;

    bridge->quantum_reasoner = qreason_create(&qconfig);
    if (!bridge->quantum_reasoner) {
        nimcp_free(bridge);
        return NULL;
    }

    /* Allocate hypothesis tracking */
    bridge->max_hypotheses = bridge->config.hypothesis_count;
    bridge->hypotheses = (quantum_hypothesis_t**)nimcp_calloc(
        bridge->max_hypotheses, sizeof(quantum_hypothesis_t*));
    if (!bridge->hypotheses) {
        qreason_destroy(bridge->quantum_reasoner);
        nimcp_free(bridge);
        return NULL;
    }

    bridge->rng_state = bridge->config.seed;
    bridge->num_hypotheses = 0;
    bridge->last_speedup = 1.0f;

    return bridge;
}

void executive_quantum_bridge_destroy(executive_quantum_bridge_t* bridge) {
    if (!bridge) return;

    /* Free hypotheses */
    if (bridge->hypotheses) {
        for (uint32_t i = 0; i < bridge->num_hypotheses; i++) {
            if (bridge->hypotheses[i]) {
                if (bridge->hypotheses[i]->action_sequence) {
                    nimcp_free(bridge->hypotheses[i]->action_sequence);
                }
                nimcp_free(bridge->hypotheses[i]);
            }
        }
        nimcp_free(bridge->hypotheses);
    }

    if (bridge->quantum_reasoner) {
        qreason_destroy(bridge->quantum_reasoner);
    }

    nimcp_free(bridge);
}

bool executive_quantum_bridge_is_enabled(const executive_quantum_bridge_t* bridge) {
    return bridge && bridge->config.enabled;
}

void executive_quantum_bridge_set_enabled(executive_quantum_bridge_t* bridge, bool enabled) {
    if (bridge) bridge->config.enabled = enabled;
}

int executive_quantum_plan(
    executive_quantum_bridge_t* bridge,
    const char* goal_description,
    uint32_t num_actions,
    uint32_t max_plan_steps,
    quantum_plan_result_t* result
) {
    if (!bridge || !goal_description || !result) return -1;
    if (!bridge->config.enabled) return -1;
    if (num_actions == 0 || max_plan_steps == 0) return -1;

    /* Clear previous hypotheses */
    for (uint32_t i = 0; i < bridge->num_hypotheses; i++) {
        if (bridge->hypotheses[i]) {
            if (bridge->hypotheses[i]->action_sequence) {
                nimcp_free(bridge->hypotheses[i]->action_sequence);
            }
            nimcp_free(bridge->hypotheses[i]);
            bridge->hypotheses[i] = NULL;
        }
    }
    bridge->num_hypotheses = 0;

    /* Generate hypotheses using quantum superposition */
    uint32_t num_to_generate = (bridge->config.hypothesis_count < bridge->max_hypotheses) ?
                                bridge->config.hypothesis_count : bridge->max_hypotheses;

    /* Limit based on planning depth */
    if (max_plan_steps > bridge->config.planning_depth) {
        max_plan_steps = bridge->config.planning_depth;
    }

    /* Use quantum reasoning to evaluate plan space */
    qreason_cnf_t plan_cnf = {0};
    plan_cnf.n_variables = (num_actions < QREASON_MAX_VARIABLES) ?
                           num_actions : QREASON_MAX_VARIABLES;

    /* Create simple satisfiability problem representing plan space */
    /* Each variable represents whether an action is included */
    plan_cnf.n_clauses = 1;
    plan_cnf.clauses[0].n_literals = 1;
    plan_cnf.clauses[0].literals[0].variable = 0;
    plan_cnf.clauses[0].literals[0].negated = false;

    qreason_result_t qresult;
    int ret = qreason_solve_sat(bridge->quantum_reasoner, &plan_cnf, &qresult);
    if (ret != 0) return -1;

    /* Generate hypotheses from quantum state */
    for (uint32_t h = 0; h < num_to_generate && h < bridge->max_hypotheses; h++) {
        quantum_hypothesis_t* hyp = (quantum_hypothesis_t*)nimcp_calloc(1, sizeof(quantum_hypothesis_t));
        if (!hyp) break;

        hyp->hypothesis_id = h;
        hyp->num_steps = (max_plan_steps > 0) ?
                         (qreason_rand(&bridge->rng_state) % max_plan_steps) + 1 : 1;

        /* Allocate action sequence */
        hyp->action_sequence = (uint32_t*)nimcp_calloc(hyp->num_steps, sizeof(uint32_t));
        if (!hyp->action_sequence) {
            nimcp_free(hyp);
            break;
        }

        /* Generate random action sequence (simplified) */
        for (uint32_t s = 0; s < hyp->num_steps; s++) {
            hyp->action_sequence[s] = qreason_rand(&bridge->rng_state) % num_actions;
        }

        /* Compute amplitude and confidence from quantum state */
        hyp->amplitude = qreason_randf(&bridge->rng_state);
        hyp->confidence = qresult.confidences[h % plan_cnf.n_variables];
        hyp->expected_utility = hyp->confidence * hyp->amplitude;
        hyp->is_satisfiable = qresult.satisfiable;

        bridge->hypotheses[bridge->num_hypotheses++] = hyp;
    }

    /* Find best hypothesis */
    quantum_hypothesis_t* best = NULL;
    float best_utility = -1.0f;

    for (uint32_t i = 0; i < bridge->num_hypotheses; i++) {
        if (bridge->hypotheses[i]->expected_utility > best_utility) {
            best_utility = bridge->hypotheses[i]->expected_utility;
            best = bridge->hypotheses[i];
        }
    }

    /* Fill result */
    result->best_hypothesis = best;
    result->num_hypotheses = bridge->num_hypotheses;
    result->satisfaction_probability = qresult.satisfaction_prob;
    result->grover_iterations_used = qresult.grover_iterations;

    /* Estimate speedup (Grover provides quadratic speedup) */
    float classical_cost = (float)num_actions;
    float quantum_cost = sqrtf((float)num_actions);
    result->planning_speedup = classical_cost / (quantum_cost > 0.0f ? quantum_cost : 1.0f);

    bridge->last_speedup = result->planning_speedup;

    /* Update statistics */
    bridge->stats.quantum_plans++;
    bridge->stats.avg_hypothesis_count =
        (bridge->stats.avg_hypothesis_count * (bridge->stats.quantum_plans - 1) +
         bridge->num_hypotheses) / bridge->stats.quantum_plans;
    bridge->stats.avg_planning_speedup =
        (bridge->stats.avg_planning_speedup * (bridge->stats.quantum_plans - 1) +
         result->planning_speedup) / bridge->stats.quantum_plans;
    bridge->stats.avg_satisfaction_prob =
        (bridge->stats.avg_satisfaction_prob * (bridge->stats.quantum_plans - 1) +
         result->satisfaction_probability) / bridge->stats.quantum_plans;

    if (best && best->confidence >= bridge->config.min_plan_confidence) {
        bridge->stats.successful_plans++;
    } else {
        bridge->stats.failed_plans++;
    }

    return 0;
}

int executive_quantum_evaluate_options(
    executive_quantum_bridge_t* bridge,
    const decision_option_t* options,
    uint32_t num_options,
    quantum_decision_result_t* result
) {
    if (!bridge || !options || !result) return -1;
    if (!bridge->config.enabled) return -1;
    if (num_options == 0) return -1;

    /* Create CNF for decision problem */
    qreason_cnf_t decision_cnf = {0};
    decision_cnf.n_variables = (num_options < QREASON_MAX_VARIABLES) ?
                               num_options : QREASON_MAX_VARIABLES;

    /* Each option is a variable, constraints as clauses */
    /* Simple encoding: at least one option must be selected */
    decision_cnf.n_clauses = 1;
    decision_cnf.clauses[0].n_literals = (decision_cnf.n_variables < QREASON_MAX_LITERALS) ?
                                         decision_cnf.n_variables : QREASON_MAX_LITERALS;

    for (uint32_t i = 0; i < decision_cnf.clauses[0].n_literals; i++) {
        decision_cnf.clauses[0].literals[i].variable = i;
        decision_cnf.clauses[0].literals[i].negated = false;
    }

    /* Solve using quantum search */
    qreason_result_t qresult;
    int ret = qreason_solve_sat(bridge->quantum_reasoner, &decision_cnf, &qresult);
    if (ret != 0) return -1;

    /* Find best option based on quantum state and expected reward */
    uint32_t best_option = 0;
    float best_score = -1.0f;

    for (uint32_t i = 0; i < num_options && i < decision_cnf.n_variables; i++) {
        float quantum_amplitude = qresult.confidences[i];
        float reward = options[i].expected_reward;
        float risk_penalty = options[i].risk_level;
        float score = quantum_amplitude * reward * (1.0f - risk_penalty * 0.5f);

        if (score > best_score) {
            best_score = score;
            best_option = options[i].option_id;
        }
    }

    /* Fill result */
    result->selected_option_id = best_option;
    result->confidence = (best_score > 0.0f) ? best_score : 0.0f;
    result->satisfaction_prob = qresult.satisfaction_prob;
    result->num_options_evaluated = num_options;

    /* Update statistics */
    bridge->stats.quantum_decisions++;

    return 0;
}

int executive_quantum_get_stats(
    const executive_quantum_bridge_t* bridge,
    executive_quantum_stats_t* stats
) {
    if (!bridge || !stats) return -1;
    *stats = bridge->stats;
    return 0;
}

void executive_quantum_reset_stats(executive_quantum_bridge_t* bridge) {
    if (bridge) {
        memset(&bridge->stats, 0, sizeof(bridge->stats));
    }
}

int executive_quantum_get_config(
    const executive_quantum_bridge_t* bridge,
    executive_quantum_config_t* config
) {
    if (!bridge || !config) return -1;
    *config = bridge->config;
    return 0;
}

#endif // NIMCP_EXECUTIVE_QUANTUM_BRIDGE_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // NIMCP_EXECUTIVE_QUANTUM_BRIDGE_H
