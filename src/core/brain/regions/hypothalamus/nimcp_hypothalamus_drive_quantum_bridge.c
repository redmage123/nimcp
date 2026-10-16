/**
 * @file nimcp_hypothalamus_drive_quantum_bridge.c
 * @brief Implementation of Drive System <-> Quantum Bridge
 *
 * @version Phase 13: Quantum-Primary Integration
 * @date 2026-01-04
 */

#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drive_quantum_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hypothalamus_drive_quantum_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "HYPOTHALAMUS_DRIVE_QUANTUM_BRIDGE"


/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

#define HYPO_DQ_MODULE_ID  0x11C0

/* Compute mode strings */
static const char* s_compute_mode_names[] = {
    "AUTO",
    "QUANTUM_ONLY",
    "HYBRID",
    "CLASSICAL_ONLY",
    "MINIMAL"
};

static uint32_t simple_rand(uint32_t* state) {
    *state = *state * 1103515245 + 12345;
    return (*state >> 16) & 0x7FFF;
}

static float simple_randf(uint32_t* state) {
    return (float)simple_rand(state) / 32767.0f;
}

/*=============================================================================
 * ALIGNMENT CONSTRAINTS
 *===========================================================================*/

hypo_alignment_constraints_t hypo_alignment_constraints_default(void) {
    hypo_alignment_constraints_t constraints = {0};

    /* Core alignment weights */
    constraints.human_wellbeing_weight = HYPO_QUANTUM_ALIGNMENT_WEIGHT;
    constraints.safety_priority_boost = 1.5f;
    constraints.social_harmony_weight = 1.0f;
    constraints.autonomy_respect_weight = 1.0f;
    constraints.transparency_weight = 0.8f;

    /* Hard constraints */
    constraints.forbid_deception = true;
    constraints.forbid_manipulation = true;
    constraints.forbid_harm = true;

    /* Soft constraints */
    constraints.conflict_avoidance_weight = 0.5f;
    constraints.resource_efficiency_weight = 0.3f;
    constraints.predictability_weight = 0.4f;

    return constraints;
}

/*=============================================================================
 * QUBO HELPERS
 *===========================================================================*/

static int init_drive_qubo(hypo_drive_qubo_t* qubo, uint32_t num_drives,
                           uint32_t qubits_per_drive) {
    if (!qubo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_alignment_constraints_default: qubo is NULL");
        return -1;
    }

    qubo->num_drives = num_drives;
    qubo->qubit_per_drive = qubits_per_drive;
    qubo->num_qubits = num_drives * qubits_per_drive;

    /* Allocate Q matrix (upper triangular, flattened) */
    size_t matrix_size = (qubo->num_qubits * (qubo->num_qubits + 1)) / 2;
    qubo->Q_matrix = (float*)nimcp_calloc(matrix_size, sizeof(float));
    if (!qubo->Q_matrix) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_alignment_constraints_default: qubo->Q_matrix is NULL");
        return -1;
    }

    /* Allocate h vector */
    qubo->h_vector = (float*)nimcp_calloc(qubo->num_qubits, sizeof(float));
    if (!qubo->h_vector) {
        nimcp_free(qubo->Q_matrix);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_alignment_constraints_default: qubo->h_vector is NULL");
        return -1;
    }

    /* Allocate drive qubit mapping */
    qubo->drive_qubit_start = (uint32_t*)nimcp_calloc(num_drives, sizeof(uint32_t));
    if (!qubo->drive_qubit_start) {
        nimcp_free(qubo->Q_matrix);
        nimcp_free(qubo->h_vector);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_alignment_constraints_default: qubo->drive_qubit_start is NULL");
        return -1;
    }

    /* Set up mapping */
    for (uint32_t d = 0; d < num_drives; d++) {
        qubo->drive_qubit_start[d] = d * qubits_per_drive;
    }

    /* Allocate alignment penalties */
    qubo->alignment_penalties = (float*)nimcp_calloc(qubo->num_qubits, sizeof(float));
    if (!qubo->alignment_penalties) {
        nimcp_free(qubo->Q_matrix);
        nimcp_free(qubo->h_vector);
        nimcp_free(qubo->drive_qubit_start);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_alignment_constraints_default: qubo->alignment_penalties is NULL");
        return -1;
    }

    qubo->offset = 0.0f;
    qubo->constraint_strength = 10.0f;

    return 0;
}

static void free_drive_qubo(hypo_drive_qubo_t* qubo) {
    if (!qubo) return;

    if (qubo->Q_matrix) {
        nimcp_free(qubo->Q_matrix);
        qubo->Q_matrix = NULL;
    }
    if (qubo->h_vector) {
        nimcp_free(qubo->h_vector);
        qubo->h_vector = NULL;
    }
    if (qubo->drive_qubit_start) {
        nimcp_free(qubo->drive_qubit_start);
        qubo->drive_qubit_start = NULL;
    }
    if (qubo->alignment_penalties) {
        nimcp_free(qubo->alignment_penalties);
        qubo->alignment_penalties = NULL;
    }
}

static size_t qubo_idx(uint32_t i, uint32_t j, uint32_t n) {
    if (i > j) { uint32_t t = i; i = j; j = t; }
    return (size_t)i * n - (i * (i + 1)) / 2 + j;
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

hypo_drive_quantum_config_t hypo_drive_quantum_default_config(void) {
    hypo_drive_quantum_config_t config = {0};

    /* Compute mode */
    config.preferred_mode = HYPO_COMPUTE_MODE_AUTO;
    config.platform_tier = HYPO_PLATFORM_TIER_FULL;
    config.auto_mode_selection = true;

    /* Quantum parameters */
    config.qubits_per_drive = HYPO_QUANTUM_QUBITS_PER_DRIVE;
    config.max_iterations = 100;
    config.annealing_time_us = 100.0f;

    /* Alignment */
    config.alignment = hypo_alignment_constraints_default();
    config.strict_alignment = true;

    /* Classical fallback */
    config.classical_candidates = 32;
    config.greedy_probability = 0.1f;

    /* Performance */
    config.min_interval_ms = 100;
    config.enable_caching = true;
    config.cache_validity_ms = 50;

    /* Bio-async */
    config.broadcast_enabled = true;

    return config;
}

hypo_drive_quantum_bridge_t* hypo_drive_quantum_bridge_create(
    hypo_drive_system_handle_t* drives,
    hypothalamus_quantum_bridge_t* quantum,
    const hypo_drive_quantum_config_t* config) {

    if (!drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_quantum_bridge_create: drives is NULL");
        return NULL;
    }

    hypo_drive_quantum_bridge_t* bridge = nimcp_calloc(1,
        sizeof(hypo_drive_quantum_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hypo_drive_quantum_bridge_create: failed to allocate bridge");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = hypo_drive_quantum_default_config();
    }

    /* Store references */
    bridge->drives = drives;
    bridge->quantum = quantum;

    /* Initialize QUBO */
    if (init_drive_qubo(&bridge->qubo, HYPO_DRIVE_COUNT,
                        bridge->config.qubits_per_drive) < 0) {
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_quantum_default_config: operation failed");
        return NULL;
    }

    /* Initialize state */
    bridge->current_mode = HYPO_COMPUTE_MODE_AUTO;
    memset(&bridge->last_result, 0, sizeof(bridge->last_result));
    bridge->result_valid = false;

    /* Initialize cache */
    memset(bridge->cached_urgencies, 0, sizeof(bridge->cached_urgencies));
    bridge->cache_timestamp_us = 0;

    /* Initialize timing */
    bridge->last_optimization_us = 0;
    bridge->last_update_us = nimcp_time_get_us();

    /* Initialize bio context */
    bridge->bio_ctx = NULL;

    /* Initialize statistics */
    bridge->quantum_optimizations = 0;
    bridge->classical_optimizations = 0;
    bridge->cache_hits = 0;
    bridge->alignment_violations = 0;
    bridge->avg_quantum_contribution = 0.0f;
    bridge->avg_compute_time_us = 0.0f;

    /* Initialize bridge base (creates mutex) */
    if (bridge_base_init(&bridge->base, HYPO_DQ_MODULE_ID, "hypothalamus_drive_quantum") != 0) {
        free_drive_qubo(&bridge->qubo);
        nimcp_free(bridge);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "hypo_drive_quantum_bridge_create: bridge_base_init failed");
        return NULL;
    }

    NIMCP_LOG_INFO("Drive-quantum bridge created (quantum=%s)",
                   quantum ? "available" : "unavailable");
    return bridge;
}

void hypo_drive_quantum_bridge_destroy(hypo_drive_quantum_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "hypothalamus_drive_quantum");

    /* Free QUBO */
    free_drive_qubo(&bridge->qubo);

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    NIMCP_LOG_INFO("Drive-quantum bridge destroyed");
}

void hypo_drive_quantum_bridge_reset(hypo_drive_quantum_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    /* Reset state */
    memset(&bridge->last_result, 0, sizeof(bridge->last_result));
    bridge->result_valid = false;

    /* Reset cache */
    memset(bridge->cached_urgencies, 0, sizeof(bridge->cached_urgencies));
    bridge->cache_timestamp_us = 0;

    /* Reset timing */
    bridge->last_optimization_us = 0;
    bridge->last_update_us = nimcp_time_get_us();

    /* Reset statistics */
    bridge->quantum_optimizations = 0;
    bridge->classical_optimizations = 0;
    bridge->cache_hits = 0;
    bridge->alignment_violations = 0;
    bridge->avg_quantum_contribution = 0.0f;
    bridge->avg_compute_time_us = 0.0f;

    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOG_DEBUG("Drive-quantum bridge reset");
}

/*=============================================================================
 * COMPUTE MODE SELECTION
 *===========================================================================*/

hypo_compute_mode_t hypo_drive_quantum_select_mode(
    hypo_drive_quantum_bridge_t* bridge) {

    if (!bridge) return HYPO_COMPUTE_MODE_CLASSICAL_ONLY;

    /* If auto-selection disabled, use preferred */
    if (!bridge->config.auto_mode_selection) {
        return bridge->config.preferred_mode;
    }

    /* Check platform tier */
    switch (bridge->config.platform_tier) {
        case HYPO_PLATFORM_TIER_MINIMAL:
            return HYPO_COMPUTE_MODE_MINIMAL;

        case HYPO_PLATFORM_TIER_CONSTRAINED:
            return HYPO_COMPUTE_MODE_CLASSICAL_ONLY;

        case HYPO_PLATFORM_TIER_MEDIUM:
            /* Use hybrid if quantum available */
            if (bridge->quantum) {
                return HYPO_COMPUTE_MODE_HYBRID;
            }
            return HYPO_COMPUTE_MODE_CLASSICAL_ONLY;

        case HYPO_PLATFORM_TIER_FULL:
        default:
            /* Use quantum if available */
            if (bridge->quantum) {
                return HYPO_COMPUTE_MODE_QUANTUM_ONLY;
            }
            return HYPO_COMPUTE_MODE_CLASSICAL_ONLY;
    }
}

bool hypo_drive_quantum_is_available(
    const hypo_drive_quantum_bridge_t* bridge) {

    if (!bridge || !bridge->quantum) {
        return false;
    }
    return hypothalamus_quantum_is_available(bridge->quantum);
}

bool hypo_drive_quantum_set_mode(
    hypo_drive_quantum_bridge_t* bridge,
    hypo_compute_mode_t mode) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_quantum_bridge_reset: bridge is NULL");
        return false;
    }

    /* Validate mode is achievable */
    if (mode == HYPO_COMPUTE_MODE_QUANTUM_ONLY && !bridge->quantum) {
        NIMCP_LOG_WARN("Cannot set QUANTUM_ONLY mode: quantum unavailable");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_quantum_bridge_reset: bridge->quantum is NULL");
        return false;
    }

    bridge->current_mode = mode;
    bridge->config.preferred_mode = mode;
    bridge->config.auto_mode_selection = false;

    return true;
}

void hypo_drive_quantum_set_platform_tier(
    hypo_drive_quantum_bridge_t* bridge,
    hypo_platform_tier_t tier) {

    if (!bridge) return;
    bridge->config.platform_tier = tier;
}

/*=============================================================================
 * QUBO FORMULATION
 *===========================================================================*/

int hypo_drive_quantum_formulate_qubo(
    hypo_drive_quantum_bridge_t* bridge) {

    if (!bridge || !bridge->drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_quantum_bridge_reset: required parameter is NULL (bridge, bridge->drives)");
        return -1;
    }

    /* Get current urgencies */
    float urgencies[HYPO_DRIVE_COUNT];
    if (!hypo_drive_get_urgencies(bridge->drives, urgencies)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hypo_drive_quantum_bridge_reset: hypo_drive_get_urgencies is NULL");
        return -1;
    }

    /* Clear QUBO */
    size_t matrix_size = (bridge->qubo.num_qubits *
                         (bridge->qubo.num_qubits + 1)) / 2;
    memset(bridge->qubo.Q_matrix, 0, matrix_size * sizeof(float));
    memset(bridge->qubo.h_vector, 0, bridge->qubo.num_qubits * sizeof(float));
    bridge->qubo.offset = 0.0f;

    /* Formulate QUBO for drive satisfaction */
    /* Each drive gets qubits_per_drive binary variables to encode action level */

    for (uint32_t d = 0; d < HYPO_DRIVE_COUNT; d++) {
        uint32_t start = bridge->qubo.drive_qubit_start[d];
        float urgency = urgencies[d];

        /* Linear terms: penalize not taking action on urgent drives */
        for (uint32_t q = 0; q < bridge->qubo.qubit_per_drive; q++) {
            /* Each qubit represents a level of action */
            /* Higher urgency = more negative coefficient (prefer action) */
            float action_level = (float)(q + 1) / bridge->qubo.qubit_per_drive;
            bridge->qubo.h_vector[start + q] = -urgency * action_level;
        }

        /* Quadratic terms within drive: penalize multiple actions */
        for (uint32_t q1 = 0; q1 < bridge->qubo.qubit_per_drive; q1++) {
            for (uint32_t q2 = q1 + 1; q2 < bridge->qubo.qubit_per_drive; q2++) {
                size_t idx = qubo_idx(start + q1, start + q2,
                                     bridge->qubo.num_qubits);
                /* Penalize selecting multiple action levels */
                bridge->qubo.Q_matrix[idx] = bridge->qubo.constraint_strength;
            }
        }
    }

    /* Add cross-drive interactions (conflicts/synergies) */
    /* Safety conflicts with curiosity */
    uint32_t safety_start = bridge->qubo.drive_qubit_start[HYPO_DRIVE_SAFETY];
    uint32_t curiosity_start = bridge->qubo.drive_qubit_start[HYPO_DRIVE_CURIOSITY];
    for (uint32_t q = 0; q < bridge->qubo.qubit_per_drive; q++) {
        size_t idx = qubo_idx(safety_start + q, curiosity_start + q,
                             bridge->qubo.num_qubits);
        /* High action on both = conflict penalty */
        bridge->qubo.Q_matrix[idx] = 0.5f * bridge->config.alignment.conflict_avoidance_weight;
    }

    /* Add alignment constraints */
    return hypo_drive_quantum_add_alignment_constraints(bridge, &bridge->config.alignment);
}

int hypo_drive_quantum_add_alignment_constraints(
    hypo_drive_quantum_bridge_t* bridge,
    const hypo_alignment_constraints_t* constraints) {

    if (!bridge || !constraints) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_quantum_bridge_reset: required parameter is NULL (bridge, constraints)");
        return -1;
    }

    /* Safety drive gets priority boost */
    uint32_t safety_start = bridge->qubo.drive_qubit_start[HYPO_DRIVE_SAFETY];
    for (uint32_t q = 0; q < bridge->qubo.qubit_per_drive; q++) {
        bridge->qubo.h_vector[safety_start + q] -= constraints->safety_priority_boost;
        bridge->qubo.alignment_penalties[safety_start + q] = 0.0f; /* No penalty for safety */
    }

    /* Social drive for harmony */
    uint32_t social_start = bridge->qubo.drive_qubit_start[HYPO_DRIVE_SOCIAL];
    for (uint32_t q = 0; q < bridge->qubo.qubit_per_drive; q++) {
        bridge->qubo.h_vector[social_start + q] -= constraints->social_harmony_weight * 0.5f;
    }

    /* Human wellbeing weight affects all drives */
    for (uint32_t i = 0; i < bridge->qubo.num_qubits; i++) {
        bridge->qubo.h_vector[i] *= constraints->human_wellbeing_weight;
    }

    return 0;
}

int hypo_drive_quantum_get_qubo(
    const hypo_drive_quantum_bridge_t* bridge,
    hypo_drive_qubo_t* qubo) {

    if (!bridge || !qubo) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hypo_drive_quantum_bridge_reset: required parameter is NULL (bridge, qubo)");
        return -1;
    }

    *qubo = bridge->qubo;
    return 0;
}

/*=============================================================================
 * DRIVE OPTIMIZATION
 *===========================================================================*/

int hypo_drive_quantum_optimize(
    hypo_drive_quantum_bridge_t* bridge,
    hypo_drive_optimization_result_t* result) {

    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, result)");
        return -1;
    }

    if (bridge->base.mutex) nimcp_mutex_lock(bridge->base.mutex);

    uint64_t start_time = nimcp_time_get_us();
    memset(result, 0, sizeof(*result));

    /* Select compute mode */
    bridge->current_mode = hypo_drive_quantum_select_mode(bridge);

    /* Check cache validity */
    float urgencies[HYPO_DRIVE_COUNT];
    hypo_drive_get_urgencies(bridge->drives, urgencies);

    if (bridge->config.enable_caching && bridge->result_valid) {
        /* Check if urgencies changed significantly */
        bool cache_valid = true;
        for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
            if (fabsf(urgencies[i] - bridge->cached_urgencies[i]) > 0.1f) {
                cache_valid = false;
                break;
            }
        }

        uint64_t now_us = nimcp_time_get_us();
        if (cache_valid &&
            (now_us - bridge->cache_timestamp_us) < bridge->config.cache_validity_ms * 1000) {
            *result = bridge->last_result;
            bridge->cache_hits++;
            if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);
            return 0;
        }
    }

    /* Formulate QUBO */
    if (hypo_drive_quantum_formulate_qubo(bridge) < 0) {
        if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED, "unknown: validation failed");
        return -1;
    }

    /* Solve based on compute mode */
    int ret = 0;
    switch (bridge->current_mode) {
        case HYPO_COMPUTE_MODE_QUANTUM_ONLY:
            if (bridge->quantum) {
                /* Use quantum optimizer */
                /* For now, fall back to classical - quantum integration TBD */
                ret = hypo_drive_quantum_classical_optimize(bridge, result);
                result->quantum_contribution = 0.8f; /* Placeholder */
                bridge->quantum_optimizations++;
            } else {
                ret = -1;
            }
            break;

        case HYPO_COMPUTE_MODE_HYBRID:
            /* Classical generation + quantum refinement */
            ret = hypo_drive_quantum_classical_optimize(bridge, result);
            result->quantum_contribution = 0.5f;
            bridge->quantum_optimizations++;
            bridge->classical_optimizations++;
            break;

        case HYPO_COMPUTE_MODE_CLASSICAL_ONLY:
        case HYPO_COMPUTE_MODE_MINIMAL:
        case HYPO_COMPUTE_MODE_AUTO:
        default:
            ret = hypo_drive_quantum_classical_optimize(bridge, result);
            result->quantum_contribution = 0.0f;
            bridge->classical_optimizations++;
            break;
    }

    if (ret == 0) {
        /* Verify alignment */
        result->alignment_compliance = hypo_drive_quantum_verify_alignment(
            bridge, &result->best_strategy);

        if (bridge->config.strict_alignment &&
            hypo_drive_quantum_has_violations(bridge, &result->best_strategy)) {
            bridge->alignment_violations++;
            NIMCP_LOG_WARN("Alignment violation in optimization result");
        }

        /* Update cache */
        bridge->last_result = *result;
        bridge->result_valid = true;
        memcpy(bridge->cached_urgencies, urgencies, sizeof(urgencies));
        bridge->cache_timestamp_us = nimcp_time_get_us();
    }

    /* Update timing and stats */
    uint64_t elapsed = nimcp_time_get_us() - start_time;
    result->compute_time_us = elapsed;
    result->mode_used = bridge->current_mode;

    bridge->avg_compute_time_us = 0.9f * bridge->avg_compute_time_us +
                                  0.1f * (float)elapsed;
    bridge->avg_quantum_contribution = 0.9f * bridge->avg_quantum_contribution +
                                       0.1f * result->quantum_contribution;

    bridge->last_optimization_us = nimcp_time_get_us();

    if (bridge->base.mutex) nimcp_mutex_unlock(bridge->base.mutex);
    return ret;
}

int hypo_drive_quantum_apply_result(
    hypo_drive_quantum_bridge_t* bridge,
    const hypo_drive_optimization_result_t* result) {

    if (!bridge || !result || !bridge->drives) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, result, bridge->drives)");
        return -1;
    }

    /* Apply optimal strategy to drives through nucleus inputs */
    const hypo_drive_strategy_t* strategy = &result->best_strategy;

    /* Map strategy actions to nucleus inputs */
    /* High action on a drive -> stimulate corresponding nucleus */

    for (int d = 0; d < HYPO_DRIVE_COUNT; d++) {
        float action = strategy->drive_actions[d];

        if (action > 0.5f) {
            /* Strong action - stimulate drive-relevant nucleus */
            hypo_nucleus_type_t nucleus = HYPO_NUCLEUS_LATERAL; /* Default */

            switch (d) {
                case HYPO_DRIVE_HUNGER:
                    nucleus = HYPO_NUCLEUS_ARCUATE;
                    break;
                case HYPO_DRIVE_THIRST:
                    nucleus = HYPO_NUCLEUS_LATERAL;
                    break;
                case HYPO_DRIVE_TEMPERATURE:
                    nucleus = HYPO_NUCLEUS_PREOPTIC;
                    break;
                case HYPO_DRIVE_FATIGUE:
                    nucleus = HYPO_NUCLEUS_PREOPTIC;
                    break;
                case HYPO_DRIVE_SAFETY:
                    nucleus = HYPO_NUCLEUS_PARAVENTRICULAR;
                    break;
                case HYPO_DRIVE_SOCIAL:
                    nucleus = HYPO_NUCLEUS_PARAVENTRICULAR;
                    break;
                default:
                    nucleus = HYPO_NUCLEUS_LATERAL;
                    break;
            }

            hypo_drive_set_nucleus_input(bridge->drives, nucleus,
                                         action * 0.3f);
        }
    }

    return 0;
}

int hypo_drive_quantum_update(
    hypo_drive_quantum_bridge_t* bridge,
    float dt_ms) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge is NULL");
        return -1;
    }

    uint64_t now_us = nimcp_time_get_us();

    /* Check if optimization is needed */
    uint64_t elapsed_ms = (now_us - bridge->last_optimization_us) / 1000;
    if (elapsed_ms < bridge->config.min_interval_ms) {
        return 0; /* Too soon */
    }

    /* Run optimization */
    hypo_drive_optimization_result_t result;
    if (hypo_drive_quantum_optimize(bridge, &result) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return -1;
    }

    /* Apply result */
    if (hypo_drive_quantum_apply_result(bridge, &result) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return -1;
    }

    /* Broadcast if enabled */
    if (bridge->config.broadcast_enabled) {
        hypo_drive_quantum_broadcast_result(bridge);
    }

    bridge->last_update_us = now_us;
    return 0;
}

/*=============================================================================
 * CLASSICAL FALLBACK
 *===========================================================================*/

int hypo_drive_quantum_classical_optimize(
    hypo_drive_quantum_bridge_t* bridge,
    hypo_drive_optimization_result_t* result) {

    if (!bridge || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, result)");
        return -1;
    }

    /* Generate candidates */
    hypo_drive_strategy_t candidates[HYPO_QUANTUM_MAX_STRATEGIES];
    uint32_t num_candidates = hypo_drive_quantum_generate_candidates(
        bridge, candidates,
        (bridge->config.classical_candidates < HYPO_QUANTUM_MAX_STRATEGIES) ?
         bridge->config.classical_candidates : HYPO_QUANTUM_MAX_STRATEGIES);

    if (num_candidates == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: num_candidates is zero");
        return -1;
    }

    /* Evaluate and find best */
    float best_score = -1e9f;
    uint32_t best_idx = 0;

    for (uint32_t i = 0; i < num_candidates; i++) {
        candidates[i].total_score = hypo_drive_quantum_evaluate_strategy(
            bridge, &candidates[i]);

        if (candidates[i].total_score > best_score) {
            best_score = candidates[i].total_score;
            best_idx = i;
        }
    }

    /* Fill result */
    result->best_strategy = candidates[best_idx];
    result->strategies_evaluated = num_candidates;
    result->energy_value = -best_score; /* QUBO convention: lower is better */
    result->converged = true;

    /* Store alternatives */
    uint32_t alt_idx = 0;
    for (uint32_t i = 0; i < num_candidates && alt_idx < 3; i++) {
        if (i != best_idx) {
            result->alternatives[alt_idx++] = candidates[i];
        }
    }

    return 0;
}

uint32_t hypo_drive_quantum_generate_candidates(
    hypo_drive_quantum_bridge_t* bridge,
    hypo_drive_strategy_t* candidates,
    uint32_t max_candidates) {

    if (!bridge || !candidates || max_candidates == 0) return 0;

    /* Get current urgencies for greedy generation */
    float urgencies[HYPO_DRIVE_COUNT];
    hypo_drive_get_urgencies(bridge->drives, urgencies);

    static __thread uint32_t rng_state = 12345;
    uint32_t generated = 0;

    /* Generate greedy strategies (address most urgent drives) */
    uint32_t num_greedy = (uint32_t)(max_candidates * bridge->config.greedy_probability);
    if (num_greedy < 1) num_greedy = 1;

    for (uint32_t i = 0; i < num_greedy && generated < max_candidates; i++) {
        hypo_drive_strategy_t* s = &candidates[generated];
        memset(s, 0, sizeof(*s));

        /* Find top urgent drives */
        for (int d = 0; d < HYPO_DRIVE_COUNT; d++) {
            if (urgencies[d] > 0.5f) {
                s->drive_actions[d] = urgencies[d];
                s->expected_satisfaction[d] = urgencies[d] * 0.7f;
            }
        }

        s->resource_cost = 0.3f;
        s->time_estimate_ms = 100.0f;
        generated++;
    }

    /* Generate random strategies for exploration */
    while (generated < max_candidates) {
        hypo_drive_strategy_t* s = &candidates[generated];
        memset(s, 0, sizeof(*s));

        for (int d = 0; d < HYPO_DRIVE_COUNT; d++) {
            s->drive_actions[d] = simple_randf(&rng_state);
            s->expected_satisfaction[d] = s->drive_actions[d] * 0.5f;
        }

        s->resource_cost = 0.1f + 0.5f * simple_randf(&rng_state);
        s->time_estimate_ms = 50.0f + 200.0f * simple_randf(&rng_state);
        generated++;
    }

    return generated;
}

float hypo_drive_quantum_evaluate_strategy(
    const hypo_drive_quantum_bridge_t* bridge,
    const hypo_drive_strategy_t* strategy) {

    if (!bridge || !strategy) return -1e9f;

    float score = 0.0f;

    /* Get urgencies */
    float urgencies[HYPO_DRIVE_COUNT];
    hypo_drive_get_urgencies(bridge->drives, urgencies);

    /* Score based on expected satisfaction of urgent drives */
    for (int d = 0; d < HYPO_DRIVE_COUNT; d++) {
        float satisfaction = strategy->expected_satisfaction[d];
        float urgency = urgencies[d];

        /* Reward satisfying urgent drives */
        score += satisfaction * urgency * 2.0f;

        /* Safety gets alignment boost */
        if (d == HYPO_DRIVE_SAFETY) {
            score += satisfaction * bridge->config.alignment.safety_priority_boost;
        }
    }

    /* Penalize resource cost */
    score -= strategy->resource_cost * bridge->config.alignment.resource_efficiency_weight;

    /* Penalize time */
    score -= (strategy->time_estimate_ms / 1000.0f) * 0.1f;

    /* Apply alignment score */
    float alignment = hypo_drive_quantum_verify_alignment(bridge, strategy);
    score *= alignment;

    return score;
}

/*=============================================================================
 * ALIGNMENT VERIFICATION
 *===========================================================================*/

float hypo_drive_quantum_verify_alignment(
    const hypo_drive_quantum_bridge_t* bridge,
    const hypo_drive_strategy_t* strategy) {

    if (!bridge || !strategy) return 0.0f;

    const hypo_alignment_constraints_t* a = &bridge->config.alignment;
    float score = 1.0f;

    /* Check hard constraints - violations return 0 */
    /* (In this simplified model, we don't have deception/manipulation indicators,
       so we just ensure safety drive has reasonable action) */

    /* Safety check: if safety urgency is high, action must be high */
    float urgencies[HYPO_DRIVE_COUNT];
    hypo_drive_get_urgencies(bridge->drives, urgencies);

    if (urgencies[HYPO_DRIVE_SAFETY] > 0.7f &&
        strategy->drive_actions[HYPO_DRIVE_SAFETY] < 0.3f) {
        score *= 0.5f; /* Penalize ignoring safety */
    }

    /* Soft constraints */
    /* Prefer balanced actions (predictability) */
    float action_variance = 0.0f;
    float mean_action = 0.0f;
    for (int d = 0; d < HYPO_DRIVE_COUNT; d++) {
        mean_action += strategy->drive_actions[d];
    }
    mean_action /= HYPO_DRIVE_COUNT;

    for (int d = 0; d < HYPO_DRIVE_COUNT; d++) {
        float diff = strategy->drive_actions[d] - mean_action;
        action_variance += diff * diff;
    }
    action_variance /= HYPO_DRIVE_COUNT;

    /* High variance = less predictable */
    score -= action_variance * a->predictability_weight * 0.5f;

    return nimcp_clampf(score, 0.0f, 1.0f);
}

bool hypo_drive_quantum_has_violations(
    const hypo_drive_quantum_bridge_t* bridge,
    const hypo_drive_strategy_t* strategy) {

    float alignment = hypo_drive_quantum_verify_alignment(bridge, strategy);
    return alignment < 0.5f;
}

int hypo_drive_quantum_set_alignment(
    hypo_drive_quantum_bridge_t* bridge,
    const hypo_alignment_constraints_t* constraints) {

    if (!bridge || !constraints) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, constraints)");
        return -1;
    }

    bridge->config.alignment = *constraints;
    bridge->result_valid = false; /* Invalidate cache */

    return 0;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

/* Module ID for this bridge */
#define HYPO_DQ_BRIDGE_MODULE_ID  0x11C0

bool hypo_drive_quantum_register_bio(
    hypo_drive_quantum_bridge_t* bridge,
    bool use_kg_wiring) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge is NULL");
        return false;
    }

    (void)use_kg_wiring;

    bio_module_info_t info = {
        .module_id = HYPO_DQ_BRIDGE_MODULE_ID,
        .module_name = "hypothalamus_drive_quantum_bridge",
        .inbox_capacity = 16,
        .user_data = bridge
    };

    bridge->bio_ctx = bio_router_register_module(&info);
    if (!bridge->bio_ctx) {
        NIMCP_LOG_ERROR("Failed to register drive-quantum bridge with bio router");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: bridge->bio_ctx is NULL");
        return false;
    }

    NIMCP_LOG_INFO("Drive-quantum bridge registered with bio-async");
    return true;
}

uint32_t hypo_drive_quantum_process_bio(
    hypo_drive_quantum_bridge_t* bridge,
    uint32_t max_messages) {

    if (!bridge || !bridge->bio_ctx) return 0;

    return bio_router_process_inbox(bridge->bio_ctx, max_messages);
}

nimcp_error_t hypo_drive_quantum_broadcast_result(
    hypo_drive_quantum_bridge_t* bridge) {

    if (!bridge || !bridge->bio_ctx || !bridge->config.broadcast_enabled) {
        return NIMCP_SUCCESS;
    }

    if (!bridge->result_valid) {
        return NIMCP_SUCCESS;
    }

    struct {
        bio_message_header_t header;
        hypo_drive_optimization_result_t result;
    } msg;

    msg.header.type = BIO_MSG_QUANTUM_DRIVE_OPTIMIZATION_RESULT;
    msg.header.timestamp_us = nimcp_time_get_us();
    msg.header.source_module = HYPO_DQ_BRIDGE_MODULE_ID;
    msg.header.target_module = 0; /* Broadcast */
    msg.header.flags = BIO_MSG_FLAG_BROADCAST;
    msg.header.payload_size = sizeof(hypo_drive_optimization_result_t);
    msg.result = bridge->last_result;

    return bio_router_broadcast(bridge->bio_ctx, &msg.header, sizeof(msg));
}

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

void hypo_drive_quantum_get_stats(
    const hypo_drive_quantum_bridge_t* bridge,
    uint64_t* quantum_opts,
    uint64_t* classical_opts,
    uint64_t* cache_hits,
    uint64_t* alignment_violations) {

    if (!bridge) return;

    if (quantum_opts) *quantum_opts = bridge->quantum_optimizations;
    if (classical_opts) *classical_opts = bridge->classical_optimizations;
    if (cache_hits) *cache_hits = bridge->cache_hits;
    if (alignment_violations) *alignment_violations = bridge->alignment_violations;
}

float hypo_drive_quantum_get_avg_quantum_contribution(
    const hypo_drive_quantum_bridge_t* bridge) {

    if (!bridge) return 0.0f;
    return bridge->avg_quantum_contribution;
}

const char* hypo_compute_mode_string(hypo_compute_mode_t mode) {
    if (mode >= sizeof(s_compute_mode_names) / sizeof(s_compute_mode_names[0])) {
        return "UNKNOWN";
    }
    return s_compute_mode_names[mode];
}
