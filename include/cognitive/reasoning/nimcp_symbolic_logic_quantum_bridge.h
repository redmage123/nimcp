/**
 * @file nimcp_symbolic_logic_quantum_bridge.h
 * @brief Quantum-accelerated symbolic reasoning and SAT solving
 *
 * WHAT: Integrates quantum reasoning with symbolic logic system
 * WHY:  O(√N) speedup for SAT solving, ternary logic for uncertainty
 * HOW:  Grover-based SAT solver with Kleene 3-valued logic
 *
 * BIOLOGICAL INSPIRATION:
 * - Prefrontal cortex logical reasoning
 * - Uncertainty representation in neural populations
 * - Abductive inference in hippocampus
 */

#ifndef NIMCP_SYMBOLIC_LOGIC_QUANTUM_BRIDGE_H
#define NIMCP_SYMBOLIC_LOGIC_QUANTUM_BRIDGE_H

#include "cognitive/nimcp_symbolic_logic.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/reasoning/nimcp_quantum_reasoning.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Types
//=============================================================================

typedef struct symbolic_quantum_bridge symbolic_quantum_bridge_t;

typedef struct {
    bool enabled;
    uint32_t max_grover_iterations;
    float min_confidence;
    uint32_t max_inference_depth;
    bool use_ternary_logic;
} symbolic_quantum_config_t;

typedef struct {
    uint64_t sat_queries;
    uint64_t satisfiable_count;
    uint64_t unsatisfiable_count;
    uint64_t inferences_made;
    float avg_grover_iterations;
    float avg_satisfaction_prob;
} symbolic_quantum_stats_t;

//=============================================================================
// API
//=============================================================================

symbolic_quantum_config_t symbolic_quantum_default_config(void);

symbolic_quantum_bridge_t* symbolic_quantum_bridge_create(
    const symbolic_quantum_config_t* config
);

void symbolic_quantum_bridge_destroy(symbolic_quantum_bridge_t* bridge);

int symbolic_quantum_bridge_connect(
    symbolic_quantum_bridge_t* bridge,
    symbolic_logic_t* logic
);

int symbolic_quantum_bridge_disconnect(symbolic_quantum_bridge_t* bridge);

bool symbolic_quantum_bridge_is_enabled(const symbolic_quantum_bridge_t* bridge);

void symbolic_quantum_bridge_set_enabled(symbolic_quantum_bridge_t* bridge, bool enabled);

/**
 * WHAT: Set fact in knowledge base with ternary truth value
 */
int symbolic_quantum_set_fact(
    symbolic_quantum_bridge_t* bridge,
    uint32_t variable,
    qreason_truth_t value,
    float confidence
);

/**
 * WHAT: Add inference rule
 */
int symbolic_quantum_add_rule(
    symbolic_quantum_bridge_t* bridge,
    const uint32_t* antecedents,
    uint32_t n_antecedents,
    uint32_t consequent,
    float confidence
);

/**
 * WHAT: Run forward chaining inference
 */
int symbolic_quantum_forward_chain(
    symbolic_quantum_bridge_t* bridge,
    qreason_result_t* result
);

/**
 * WHAT: Solve SAT problem using Grover search
 */
int symbolic_quantum_solve_sat(
    symbolic_quantum_bridge_t* bridge,
    const qreason_cnf_t* cnf,
    qreason_result_t* result
);

/**
 * WHAT: Query knowledge base for variable truth value
 */
int symbolic_quantum_query(
    symbolic_quantum_bridge_t* bridge,
    uint32_t variable,
    qreason_result_t* result
);

/**
 * WHAT: Check consistency of knowledge base
 */
bool symbolic_quantum_check_consistency(
    symbolic_quantum_bridge_t* bridge
);

/**
 * WHAT: Ternary logic AND operation
 */
qreason_truth_t symbolic_quantum_and(qreason_truth_t a, qreason_truth_t b);

/**
 * WHAT: Ternary logic OR operation
 */
qreason_truth_t symbolic_quantum_or(qreason_truth_t a, qreason_truth_t b);

/**
 * WHAT: Ternary logic NOT operation
 */
qreason_truth_t symbolic_quantum_not(qreason_truth_t a);

/**
 * WHAT: Ternary logic IMPLIES operation
 */
qreason_truth_t symbolic_quantum_implies(qreason_truth_t a, qreason_truth_t b);

int symbolic_quantum_get_stats(
    const symbolic_quantum_bridge_t* bridge,
    symbolic_quantum_stats_t* stats
);

void symbolic_quantum_reset_stats(symbolic_quantum_bridge_t* bridge);

//=============================================================================
// Implementation
//=============================================================================

#ifdef NIMCP_SYMBOLIC_QUANTUM_BRIDGE_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include "utils/memory/nimcp_memory.h"

struct symbolic_quantum_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    symbolic_quantum_config_t config;
    symbolic_logic_t* logic;
    qreason_t reasoner;
    symbolic_quantum_stats_t stats;
    bool connected;
};

symbolic_quantum_config_t symbolic_quantum_default_config(void) {
    return (symbolic_quantum_config_t){
        .enabled = true,
        .max_grover_iterations = 0,  // Auto
        .min_confidence = 0.5f,
        .max_inference_depth = 10,
        .use_ternary_logic = true
    };
}

symbolic_quantum_bridge_t* symbolic_quantum_bridge_create(
    const symbolic_quantum_config_t* config
) {
    symbolic_quantum_bridge_t* bridge = (symbolic_quantum_bridge_t*)nimcp_calloc(1, sizeof(*bridge));
    if (!bridge) return NULL;

    bridge->config = config ? *config : symbolic_quantum_default_config();

    qreason_config_t qconfig;
    memset(&qconfig, 0, sizeof(qconfig));
    qconfig.max_grover_iterations = bridge->config.max_grover_iterations;
    qconfig.min_confidence = bridge->config.min_confidence;
    qconfig.max_inference_depth = bridge->config.max_inference_depth;
    qconfig.use_ternary_logic = bridge->config.use_ternary_logic;
    bridge->reasoner = qreason_create(&qconfig);
    if (!bridge->reasoner) {
        nimcp_free(bridge);
        return NULL;
    }

    return bridge;
}

void symbolic_quantum_bridge_destroy(symbolic_quantum_bridge_t* bridge) {
    if (!bridge) return;
    if (bridge->reasoner) qreason_destroy(bridge->reasoner);
    nimcp_free(bridge);
}

int symbolic_quantum_bridge_connect(
    symbolic_quantum_bridge_t* bridge,
    symbolic_logic_t* logic
) {
    if (!bridge || !logic) return -1;
    bridge->logic = logic;
    bridge->connected = true;
    return 0;
}

int symbolic_quantum_bridge_disconnect(symbolic_quantum_bridge_t* bridge) {
    if (!bridge) return -1;
    bridge->logic = NULL;
    bridge->connected = false;
    return 0;
}

bool symbolic_quantum_bridge_is_enabled(const symbolic_quantum_bridge_t* bridge) {
    return bridge && bridge->config.enabled && bridge->connected;
}

void symbolic_quantum_bridge_set_enabled(symbolic_quantum_bridge_t* bridge, bool enabled) {
    if (bridge) bridge->config.enabled = enabled;
}

int symbolic_quantum_set_fact(
    symbolic_quantum_bridge_t* bridge,
    uint32_t variable,
    qreason_truth_t value,
    float confidence
) {
    if (!bridge || !bridge->reasoner) return -1;
    return qreason_set_fact(bridge->reasoner, variable, value, confidence);
}

int symbolic_quantum_add_rule(
    symbolic_quantum_bridge_t* bridge,
    const uint32_t* antecedents,
    uint32_t n_antecedents,
    uint32_t consequent,
    float confidence
) {
    if (!bridge || !bridge->reasoner) return -1;
    return qreason_add_rule(bridge->reasoner, antecedents, n_antecedents,
                            consequent, confidence);
}

int symbolic_quantum_forward_chain(
    symbolic_quantum_bridge_t* bridge,
    qreason_result_t* result
) {
    if (!bridge || !bridge->reasoner || !result) return -1;

    uint32_t inferences = qreason_forward_chain(bridge->reasoner, result);
    bridge->stats.inferences_made += inferences;

    return 0;
}

int symbolic_quantum_solve_sat(
    symbolic_quantum_bridge_t* bridge,
    const qreason_cnf_t* cnf,
    qreason_result_t* result
) {
    if (!bridge || !bridge->reasoner || !cnf || !result) return -1;

    int status = qreason_solve_sat(bridge->reasoner, cnf, result);
    if (status < 0) return status;

    bridge->stats.sat_queries++;
    if (result->satisfiable) {
        bridge->stats.satisfiable_count++;
    } else {
        bridge->stats.unsatisfiable_count++;
    }

    bridge->stats.avg_grover_iterations =
        (bridge->stats.avg_grover_iterations * (bridge->stats.sat_queries - 1)
         + result->grover_iterations) / bridge->stats.sat_queries;

    bridge->stats.avg_satisfaction_prob =
        (bridge->stats.avg_satisfaction_prob * (bridge->stats.sat_queries - 1)
         + result->satisfaction_prob) / bridge->stats.sat_queries;

    return 0;
}

int symbolic_quantum_query(
    symbolic_quantum_bridge_t* bridge,
    uint32_t variable,
    qreason_result_t* result
) {
    if (!bridge || !bridge->reasoner || !result) return -1;
    return qreason_query(bridge->reasoner, variable, result);
}

bool symbolic_quantum_check_consistency(symbolic_quantum_bridge_t* bridge) {
    if (!bridge || !bridge->reasoner) return false;
    return qreason_check_consistency(bridge->reasoner);
}

qreason_truth_t symbolic_quantum_and(qreason_truth_t a, qreason_truth_t b) {
    return qreason_and(a, b);
}

qreason_truth_t symbolic_quantum_or(qreason_truth_t a, qreason_truth_t b) {
    return qreason_or(a, b);
}

qreason_truth_t symbolic_quantum_not(qreason_truth_t a) {
    return qreason_not(a);
}

qreason_truth_t symbolic_quantum_implies(qreason_truth_t a, qreason_truth_t b) {
    return qreason_implies(a, b);
}

int symbolic_quantum_get_stats(
    const symbolic_quantum_bridge_t* bridge,
    symbolic_quantum_stats_t* stats
) {
    if (!bridge || !stats) return -1;
    *stats = bridge->stats;
    return 0;
}

void symbolic_quantum_reset_stats(symbolic_quantum_bridge_t* bridge) {
    if (bridge) memset(&bridge->stats, 0, sizeof(bridge->stats));
}

#endif // NIMCP_SYMBOLIC_QUANTUM_BRIDGE_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SYMBOLIC_LOGIC_QUANTUM_BRIDGE_H
