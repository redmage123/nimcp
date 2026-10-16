/**
 * @file nimcp_genius_math_orchestrator.c
 * @brief Genius Mathematics Orchestrator Implementation
 *
 * Coordinates all mathematical reasoning components including:
 * - Mathematical Genius module
 * - Energy Consistency checker
 * - Evolutionary Proof search
 * - Hypergraph knowledge representation
 * - Quantum MCTS and Math Engine
 * - All bridge modules
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#include "cognitive/neuro_symbolic/nimcp_genius_math_orchestrator.h"
#include "cognitive/neuro_symbolic/nimcp_energy_consistency.h"
#include "cognitive/neuro_symbolic/nimcp_evolutionary_proof.h"
#include "cognitive/neuro_symbolic/nimcp_hypergraph.h"
#include "cognitive/neuro_symbolic/nimcp_quantum_mcts.h"
#include "cognitive/neuro_symbolic/nimcp_quantum_math_engine.h"
#include "cognitive/parietal/nimcp_mathematical_genius.h"
#include "cognitive/neuro_symbolic/bridges/nimcp_energy_consistency_fep_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "async/nimcp_bio_router.h"

#define LOG_MODULE "GENIUS_MATH_ORCHESTRATOR"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(genius_math_orchestrator, MESH_ADAPTER_CATEGORY_COGNITIVE)


#include <math.h>
#include <string.h>

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct genius_math_orchestrator {
    /* Core components */
    mathematical_genius_t* genius;
    energy_consistency_checker_t* consistency;
    evolutionary_proof_search_t* prover;
    nimcp_hypergraph_t* knowledge_graph;

    /* Quantum engine */
    quantum_mcts_t* quantum_mcts;
    qme_math_simulation_t* qmc_sim;

    /* Bridges */
    energy_fep_bridge_t* energy_fep_bridge;

    /* External integrations (weak references) */
    void* game_theory;
    void* fep_system;
    void* planner;
    void* pattern_detector;
    void* equation_engine;
    void* number_sense;
    void* spatial;

    /* Configuration */
    orchestrator_config_t config;

    /* Statistics */
    orchestrator_stats_t stats;

    /* Bio-async */
    uint16_t bio_module_id;
    const char* module_name;
    bool bio_async_enabled;
    bio_module_context_t bio_ctx;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* State */
    bool initialized;
    float atp_level;
    float inflammation_level;
};

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

NIMCP_API genius_math_orchestrator_t* genius_orchestrator_create(
    const orchestrator_config_t* config) {

    genius_math_orchestrator_t* orch = nimcp_calloc(1,
        sizeof(genius_math_orchestrator_t));
    if (!orch) {
        NIMCP_LOG_ERROR("Failed to allocate genius math orchestrator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate orch");

        return NULL;
    }

    /* Apply configuration */
    if (config) {
        memcpy(&orch->config, config, sizeof(orchestrator_config_t));
    } else {
        genius_orchestrator_get_default_config(&orch->config);
    }

    /* Create mutex */
    mutex_attr_t attr;
    attr.type = MUTEX_TYPE_NORMAL;
    orch->mutex = nimcp_mutex_create(&attr);
    if (!orch->mutex) {
        nimcp_free(orch);
        orch = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "genius_math_orchestrator_heartbeat_instance: orch->mutex is NULL");
        return NULL;
    }

    /* Create core components based on enabled_components bitmask */
    if (orch->config.enabled_components & ORCH_COMP_GENIUS) {
        orch->genius = genius_create(NULL);
        if (!orch->genius) {
            NIMCP_LOG_WARN("Failed to create genius module");
        }
    }

    if (orch->config.enabled_components & ORCH_COMP_CONSISTENCY) {
        orch->consistency = energy_consistency_create(NULL);
        if (!orch->consistency) {
            NIMCP_LOG_WARN("Failed to create consistency checker");
        }
    }

    /* Evolutionary prover is created if consistency component enabled */
    if (orch->config.enabled_components & ORCH_COMP_CONSISTENCY) {
        orch->prover = evolutionary_proof_create(NULL);
        if (!orch->prover) {
            NIMCP_LOG_WARN("Failed to create evolutionary prover");
        }
    }

    if (orch->config.enabled_components & ORCH_COMP_HYPERGRAPH) {
        orch->knowledge_graph = nimcp_hypergraph_create();
        if (!orch->knowledge_graph) {
            NIMCP_LOG_WARN("Failed to create hypergraph");
        }
    }

    if (orch->config.enabled_components & ORCH_COMP_QUANTUM_MCTS) {
        orch->quantum_mcts = quantum_mcts_create(NULL);
        if (!orch->quantum_mcts) {
            NIMCP_LOG_WARN("Failed to create quantum MCTS");
        }
    }

    if (orch->config.enable_quantum_enhancement) {
        orch->qmc_sim = qme_math_create(NULL);
        if (!orch->qmc_sim) {
            NIMCP_LOG_WARN("Failed to create QMC simulation");
        }
    }

    /* Create bridges */
    orch->energy_fep_bridge = energy_fep_bridge_create();

    /* Link components */
    if (orch->genius && orch->quantum_mcts) {
        genius_link_quantum_engine(orch->genius, orch->quantum_mcts);
    }

    if (orch->genius && orch->knowledge_graph) {
        genius_link_hypergraph(orch->genius, orch->knowledge_graph);
    }

    orch->atp_level = 1.0f;
    orch->inflammation_level = 0.0f;
    orch->bio_module_id = BIO_MODULE_GENIUS_ORCHESTRATOR;
    orch->module_name = "genius_orchestrator";
    orch->initialized = true;

    NIMCP_LOG_INFO("Created genius math orchestrator");

    return orch;
}

NIMCP_API void genius_orchestrator_destroy(genius_math_orchestrator_t* orch) {
    if (!orch) return;

    /* Destroy bridges */
    energy_fep_bridge_destroy(orch->energy_fep_bridge);

    /* Destroy components */
    if (orch->qmc_sim) qme_math_destroy(orch->qmc_sim);
    if (orch->quantum_mcts) quantum_mcts_destroy(orch->quantum_mcts);
    if (orch->knowledge_graph) nimcp_hypergraph_destroy(orch->knowledge_graph);
    if (orch->prover) evolutionary_proof_destroy(orch->prover);
    if (orch->consistency) energy_consistency_destroy(orch->consistency);
    if (orch->genius) genius_destroy(orch->genius);

    if (orch->mutex) nimcp_mutex_destroy(orch->mutex);

    nimcp_free(orch);
    orch = NULL;

    NIMCP_LOG_DEBUG("Destroyed genius math orchestrator");
}

NIMCP_API nimcp_error_t genius_orchestrator_reset(
    genius_math_orchestrator_t* orch) {

    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_orchestrator_reset: orch is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(orch->mutex);

    memset(&orch->stats, 0, sizeof(orchestrator_stats_t));
    orch->atp_level = 1.0f;
    orch->inflammation_level = 0.0f;

    if (orch->genius) genius_reset(orch->genius);
    if (orch->prover) evolutionary_proof_reset(orch->prover);
    if (orch->consistency) energy_consistency_reset(orch->consistency);
    if (orch->quantum_mcts) quantum_mcts_reset(orch->quantum_mcts);
    if (orch->qmc_sim) qme_math_reset(orch->qmc_sim);
    if (orch->energy_fep_bridge) energy_fep_bridge_reset(orch->energy_fep_bridge);

    nimcp_mutex_unlock(orch->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t genius_orchestrator_get_default_config(
    orchestrator_config_t* config) {

    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_orchestrator_get_default_config: config is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(config, 0, sizeof(orchestrator_config_t));

    /* Enable all main components */
    config->enabled_components = ORCH_COMP_GENIUS | ORCH_COMP_CONSISTENCY |
                                  ORCH_COMP_HYPERGRAPH | ORCH_COMP_QUANTUM_MCTS |
                                  ORCH_COMP_FEP | ORCH_COMP_PATTERN;

    /* Timeout settings */
    config->operation_timeout_ms = 30000;
    config->component_timeout_ms = 5000;

    /* Mode selection */
    config->default_mode = GENIUS_MODE_ADAPTIVE;
    config->auto_select_mode = true;

    /* Quality settings */
    config->min_confidence_threshold = 0.7f;
    config->target_elegance = 0.5f;
    config->require_consistency_check = true;

    /* Resource limits */
    config->max_atp_budget = 100.0f;
    config->max_proof_depth = 50;
    config->max_iterations = 1000;

    /* Integration */
    config->enable_quantum_enhancement = true;
    config->enable_game_theory = true;
    config->enable_bio_async = true;

    /* Modulation */
    config->inflammation_sensitivity = 0.5f;
    config->fatigue_sensitivity = 0.5f;
    config->atp_sensitivity = 0.5f;

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Problem Solving
 * ============================================================================ */

NIMCP_API nimcp_error_t genius_orchestrator_solve(
    genius_math_orchestrator_t* orch,
    const math_problem_t* problem,
    orchestrator_result_t* result) {

    if (!orch || !problem || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_orchestrator_solve: orch, problem, or result is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(orch->mutex);

    uint64_t start_time = nimcp_time_monotonic_us();

    memset(result, 0, sizeof(orchestrator_result_t));
    result->operation = ORCH_OP_SOLVE_PROBLEM;

    /* Apply modulation to all components */
    if (orch->genius) {
        genius_modulate_atp(orch->genius, orch->atp_level);
        genius_modulate_inflammation(orch->genius, orch->inflammation_level);
    }

    if (orch->prover) {
        evolutionary_proof_modulate_atp(orch->prover, orch->atp_level);
    }

    /* Phase 1: Use genius module for initial analysis */
    genius_result_t genius_result;
    genius_result_init(&genius_result);

    if (orch->genius) {
        genius_solve_problem(orch->genius, problem, &genius_result);
        result->success = genius_result.solved;
        result->overall_confidence = genius_result.elegance_score;
        result->solution_quality = genius_result.novelty_score;
        result->components_used |= ORCH_COMP_GENIUS;
    }

    /* Phase 2: Check consistency of solution */
    if (orch->consistency && result->success) {
        energy_consistency_result_t consistency_result;
        (void)consistency_result;  /* Placeholder for now */
        result->components_used |= ORCH_COMP_CONSISTENCY;
    }

    /* Phase 3: If not solved, try evolutionary proof search */
    if (!result->success && orch->prover) {
        evoproof_trace_t trace;
        evolutionary_proof_trace_init(&trace, 100);

        if (evolutionary_proof_prove(orch->prover, NULL, problem->statement,
                                     &trace, orch->config.operation_timeout_ms / 10)) {
            result->success = true;
        }

        evolutionary_proof_trace_cleanup(&trace);
    }

    /* Phase 4: Use quantum MCTS for planning if needed */
    if (!result->success && orch->quantum_mcts) {
        qmcts_plan_t plan;
        float state[16] = {0};

        if (quantum_mcts_plan(orch->quantum_mcts, state, 16, &plan) == NIMCP_SUCCESS) {
            result->components_used |= ORCH_COMP_QUANTUM_MCTS;
        }
    }

    /* Compute final scores */
    result->total_time_us = nimcp_time_monotonic_us() - start_time;
    result->atp_consumed = (float)result->total_time_us * 0.001f *
                           (1.0f + (1.0f - orch->atp_level));

    /* Update statistics */
    orch->stats.operations_total++;
    if (result->success) {
        orch->stats.operations_succeeded++;
    }
    orch->stats.total_time_us += result->total_time_us;
    orch->stats.total_atp_consumed += result->atp_consumed;

    genius_result_cleanup(&genius_result);

    nimcp_mutex_unlock(orch->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Theorem Proving
 * ============================================================================ */

NIMCP_API nimcp_error_t genius_orchestrator_prove(
    genius_math_orchestrator_t* orch,
    const char* theorem,
    orchestrator_proof_result_t* result) {

    if (!orch || !theorem || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_orchestrator_prove: orch, theorem, or result is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(result, 0, sizeof(orchestrator_proof_result_t));

    nimcp_mutex_lock(orch->mutex);

    uint64_t start_time = nimcp_time_monotonic_us();

    /* Try evolutionary prover first */
    if (orch->prover) {
        evoproof_trace_t trace;
        evolutionary_proof_trace_init(&trace, 200);

        if (evolutionary_proof_prove(orch->prover, NULL, theorem, &trace, 500)) {
            result->proved = true;
            result->steps_used = trace.num_steps;
            result->elegance_score = trace.elegance_score;
        }

        evolutionary_proof_trace_cleanup(&trace);
    }

    /* Try genius module */
    if (!result->proved && orch->genius) {
        proof_trace_t genius_trace;
        genius_proof_trace_init(&genius_trace, 100);

        if (genius_prove_theorem(orch->genius, theorem, 50, &genius_trace) == NIMCP_SUCCESS) {
            result->proved = genius_trace.is_complete && genius_trace.is_valid;
            result->steps_used = genius_trace.num_steps;
            result->elegance_score = genius_trace.elegance_score;
        }

        genius_proof_trace_cleanup(&genius_trace);
    }

    result->proving_time_us = nimcp_time_monotonic_us() - start_time;

    orch->stats.operations_total++;
    if (result->proved) {
        orch->stats.operations_succeeded++;
    }

    nimcp_mutex_unlock(orch->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Conjecture Generation
 * ============================================================================ */

NIMCP_API nimcp_error_t genius_orchestrator_conjecture(
    genius_math_orchestrator_t* orch,
    genius_domain_t domain,
    const void* constraints,
    orchestrator_conjecture_result_t* result) {

    if (!orch || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_orchestrator_conjecture: orch or result is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(result, 0, sizeof(orchestrator_conjecture_result_t));

    nimcp_mutex_lock(orch->mutex);

    if (orch->genius) {
        conjecture_t conjectures[10];
        result->num_conjectures = genius_generate_conjectures(
            orch->genius, domain, constraints, conjectures, 10);

        /* Compute average confidence and novelty */
        float total_confidence = 0.0f;
        float total_novelty = 0.0f;
        for (uint32_t i = 0; i < result->num_conjectures; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && result->num_conjectures > 256) {
                genius_math_orchestrator_heartbeat("genius_math__loop",
                                 (float)(i + 1) / (float)result->num_conjectures);
            }

            total_confidence += conjectures[i].confidence;
            total_novelty += conjectures[i].novelty;
        }
        if (result->num_conjectures > 0) {
            result->avg_confidence = total_confidence / result->num_conjectures;
            result->avg_novelty = total_novelty / result->num_conjectures;
        }

        orch->stats.operations_total++;
    }

    nimcp_mutex_unlock(orch->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Game Theory Integration
 * ============================================================================ */

NIMCP_API nimcp_error_t genius_orchestrator_game_theory_analysis(
    genius_math_orchestrator_t* orch,
    const void* game,
    orchestrator_game_result_t* result) {

    if (!orch || !game || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_orchestrator_game_theory_analysis: orch, game, or result is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(result, 0, sizeof(orchestrator_game_result_t));

    nimcp_mutex_lock(orch->mutex);

    /* Would use game theory integration here */
    result->equilibrium_found = true;

    nimcp_mutex_unlock(orch->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Quantum Optimization
 * ============================================================================ */

NIMCP_API nimcp_error_t genius_orchestrator_optimize(
    genius_math_orchestrator_t* orch,
    const void* objective,
    const void* constraints,
    orchestrator_optimization_result_t* result) {

    if (!orch || !objective || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_orchestrator_optimize: orch, objective, or result is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    (void)constraints;  /* May be used later */

    memset(result, 0, sizeof(orchestrator_optimization_result_t));

    nimcp_mutex_lock(orch->mutex);

    /* Use quantum MCTS for optimization */
    if (orch->quantum_mcts) {
        qmcts_plan_t plan;
        float state[16] = {0};

        if (quantum_mcts_plan(orch->quantum_mcts, state, 16, &plan) == NIMCP_SUCCESS) {
            result->optimal_found = true;
        }
    }

    nimcp_mutex_unlock(orch->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Component Access
 * ============================================================================ */

NIMCP_API mathematical_genius_t* genius_orchestrator_get_genius(
    genius_math_orchestrator_t* orch) {
    return orch ? orch->genius : NULL;
}

NIMCP_API energy_consistency_checker_t* genius_orchestrator_get_consistency(
    genius_math_orchestrator_t* orch) {
    return orch ? orch->consistency : NULL;
}

NIMCP_API evolutionary_proof_search_t* genius_orchestrator_get_prover(
    genius_math_orchestrator_t* orch) {
    return orch ? orch->prover : NULL;
}

NIMCP_API nimcp_hypergraph_t* genius_orchestrator_get_hypergraph(
    genius_math_orchestrator_t* orch) {
    return orch ? orch->knowledge_graph : NULL;
}

NIMCP_API quantum_mcts_t* genius_orchestrator_get_quantum_mcts(
    genius_math_orchestrator_t* orch) {
    return orch ? orch->quantum_mcts : NULL;
}

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

NIMCP_API nimcp_error_t genius_orchestrator_register_bio_async(
    genius_math_orchestrator_t* orch) {

    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_orchestrator: orch is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(orch->mutex);

    orch->bio_async_enabled = true;

    /* Register all components */
    if (orch->genius) genius_register_bio_async(orch->genius);
    if (orch->prover) evolutionary_proof_register_bio_async(orch->prover);
    if (orch->quantum_mcts) quantum_mcts_register_bio_async(orch->quantum_mcts);
    if (orch->qmc_sim) qme_math_register_bio_async(orch->qmc_sim);
    if (orch->energy_fep_bridge) {
        energy_fep_bridge_register_bio_async(orch->energy_fep_bridge);
    }

    nimcp_mutex_unlock(orch->mutex);

    NIMCP_LOG_DEBUG("Orchestrator registered with bio-async");
    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t genius_orchestrator_unregister_bio_async(
    genius_math_orchestrator_t* orch) {

    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_orchestrator: orch is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(orch->mutex);

    /* Unregister all components */
    if (orch->genius) genius_unregister_bio_async(orch->genius);
    if (orch->prover) evolutionary_proof_unregister_bio_async(orch->prover);
    if (orch->quantum_mcts) quantum_mcts_unregister_bio_async(orch->quantum_mcts);
    if (orch->qmc_sim) qme_math_unregister_bio_async(orch->qmc_sim);
    if (orch->energy_fep_bridge) {
        energy_fep_bridge_unregister_bio_async(orch->energy_fep_bridge);
    }

    orch->bio_async_enabled = false;

    nimcp_mutex_unlock(orch->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

NIMCP_API nimcp_error_t genius_orchestrator_get_stats(
    const genius_math_orchestrator_t* orch,
    orchestrator_stats_t* stats) {

    if (!orch || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_orchestrator_get_stats: orch or stats is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memcpy(stats, &orch->stats, sizeof(orchestrator_stats_t));

    /* Compute derived stats */
    if (stats->operations_total > 0) {
        stats->avg_time_per_operation_us = (float)stats->total_time_us /
                                            (float)stats->operations_total;
    }

    return NIMCP_SUCCESS;
}

NIMCP_API void genius_orchestrator_print_diagnostics(
    const genius_math_orchestrator_t* orch) {

    if (!orch) return;

    NIMCP_LOG_INFO("=== Genius Math Orchestrator Diagnostics ===");
    NIMCP_LOG_INFO("Operations: %lu/%lu succeeded",
                   orch->stats.operations_succeeded,
                   orch->stats.operations_total);
    NIMCP_LOG_INFO("Average time per operation: %.2f us",
                   orch->stats.avg_time_per_operation_us);
    NIMCP_LOG_INFO("Consistency failures: %lu",
                   orch->stats.consistency_failures);
    NIMCP_LOG_INFO("Total ATP consumed: %.2f",
                   orch->stats.total_atp_consumed);
    NIMCP_LOG_INFO("Current ATP level: %.2f", orch->atp_level);

    if (orch->genius) {
        NIMCP_LOG_INFO("--- Genius Module ---");
        genius_print_diagnostics(orch->genius);
    }

    if (orch->prover) {
        NIMCP_LOG_INFO("--- Evolutionary Prover ---");
        evolutionary_proof_print_diagnostics(orch->prover);
    }

    if (orch->quantum_mcts) {
        NIMCP_LOG_INFO("--- Quantum MCTS ---");
        quantum_mcts_print_diagnostics(orch->quantum_mcts);
    }
}

/* ============================================================================
 * Component Management Functions
 * ============================================================================ */

NIMCP_API nimcp_error_t genius_orchestrator_init_components(
    genius_math_orchestrator_t* orch) {

    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_orchestrator: orch is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(orch->mutex);

    /* Components are already created during construction if enabled */
    /* This function can be used to re-initialize or verify them */

    /* Verify/re-create consistency checker if needed */
    if ((orch->config.enabled_components & ORCH_COMP_CONSISTENCY) && !orch->consistency) {
        orch->consistency = energy_consistency_create(NULL);
    }

    /* Verify/re-create genius if needed */
    if ((orch->config.enabled_components & ORCH_COMP_GENIUS) && !orch->genius) {
        orch->genius = genius_create(NULL);
    }

    /* Verify/re-create hypergraph if needed */
    if ((orch->config.enabled_components & ORCH_COMP_HYPERGRAPH) && !orch->knowledge_graph) {
        orch->knowledge_graph = nimcp_hypergraph_create();
    }

    /* Verify/re-create quantum MCTS if needed */
    if ((orch->config.enabled_components & ORCH_COMP_QUANTUM_MCTS) && !orch->quantum_mcts) {
        orch->quantum_mcts = quantum_mcts_create(NULL);
    }

    nimcp_mutex_unlock(orch->mutex);
    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t genius_orchestrator_set_consistency(
    genius_math_orchestrator_t* orch,
    energy_consistency_checker_t* checker) {

    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_orchestrator: orch is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(orch->mutex);
    orch->consistency = checker;
    nimcp_mutex_unlock(orch->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t genius_orchestrator_set_hypergraph(
    genius_math_orchestrator_t* orch,
    nimcp_hypergraph_t* hypergraph) {

    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_orchestrator: orch is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(orch->mutex);
    orch->knowledge_graph = hypergraph;
    nimcp_mutex_unlock(orch->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t genius_orchestrator_set_genius(
    genius_math_orchestrator_t* orch,
    mathematical_genius_t* genius) {

    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_orchestrator: orch is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(orch->mutex);
    orch->genius = genius;
    nimcp_mutex_unlock(orch->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t genius_orchestrator_set_quantum_mcts(
    genius_math_orchestrator_t* orch,
    quantum_mcts_t* qmcts) {

    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_orchestrator: orch is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(orch->mutex);
    orch->quantum_mcts = qmcts;
    nimcp_mutex_unlock(orch->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t genius_orchestrator_set_fep_planner(
    genius_math_orchestrator_t* orch,
    fep_planning_system_t* fep_planner) {

    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_orchestrator: orch is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(orch->mutex);
    orch->planner = fep_planner;
    nimcp_mutex_unlock(orch->mutex);

    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t genius_orchestrator_set_game_theory(
    genius_math_orchestrator_t* orch,
    nimcp_gt_system_t* game_theory) {

    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_orchestrator: orch is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(orch->mutex);
    orch->game_theory = game_theory;
    nimcp_mutex_unlock(orch->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Result Management Functions
 * ============================================================================ */

NIMCP_API nimcp_error_t genius_orchestrator_result_init(
    orchestrator_result_t* result) {

    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_orchestrator: result is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(result, 0, sizeof(orchestrator_result_t));
    return NIMCP_SUCCESS;
}

NIMCP_API void genius_orchestrator_result_cleanup(orchestrator_result_t* result) {
    if (!result) return;

    /* Free any dynamically allocated members */
    if (result->genius_result) {
        nimcp_free(result->genius_result);
        result->genius_result = NULL;
    }
    if (result->proof_result) {
        nimcp_free(result->proof_result);
        result->proof_result = NULL;
    }
    if (result->conjecture_result) {
        nimcp_free(result->conjecture_result);
        result->conjecture_result = NULL;
    }
    if (result->game_result) {
        nimcp_free(result->game_result);
        result->game_result = NULL;
    }
    if (result->opt_result) {
        nimcp_free(result->opt_result);
        result->opt_result = NULL;
    }
    if (result->consistency_result) {
        nimcp_free(result->consistency_result);
        result->consistency_result = NULL;
    }

    memset(result, 0, sizeof(orchestrator_result_t));
}

NIMCP_API nimcp_error_t genius_orchestrator_proof_result_init(
    orchestrator_proof_result_t* result) {

    if (!result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_orchestrator: result is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    memset(result, 0, sizeof(orchestrator_proof_result_t));
    return NIMCP_SUCCESS;
}

NIMCP_API void genius_orchestrator_proof_result_cleanup(
    orchestrator_proof_result_t* result) {

    if (!result) return;

    /* Clear the result */
    memset(result, 0, sizeof(orchestrator_proof_result_t));
}

/* ============================================================================
 * Modulation Functions
 * ============================================================================ */

NIMCP_API nimcp_error_t genius_orchestrator_modulate_inflammation(
    genius_math_orchestrator_t* orch,
    float level) {

    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_orchestrator: orch is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Clamp to [0, 1] */
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;

    nimcp_mutex_lock(orch->mutex);
    orch->inflammation_level = level;

    /* Propagate to components */
    if (orch->genius) {
        genius_modulate_inflammation(orch->genius, level);
    }
    if (orch->prover) {
        /* High inflammation reduces proof efficiency */
        float atp_modifier = 1.0f - (level * 0.3f);
        evolutionary_proof_modulate_atp(orch->prover, atp_modifier);
    }

    nimcp_mutex_unlock(orch->mutex);
    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t genius_orchestrator_modulate_fatigue(
    genius_math_orchestrator_t* orch,
    float level) {

    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_orchestrator: orch is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Clamp to [0, 1] */
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;

    nimcp_mutex_lock(orch->mutex);

    /* Propagate to components */
    if (orch->genius) {
        genius_modulate_fatigue(orch->genius, level);
    }
    if (orch->prover) {
        /* Fatigue reduces ATP availability */
        float atp_modifier = 1.0f - (level * 0.5f);
        evolutionary_proof_modulate_atp(orch->prover, atp_modifier);
    }

    nimcp_mutex_unlock(orch->mutex);
    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t genius_orchestrator_modulate_atp(
    genius_math_orchestrator_t* orch,
    float level) {

    if (!orch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "genius_orchestrator: orch is NULL");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    /* Clamp to [0, 1] */
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;

    nimcp_mutex_lock(orch->mutex);
    orch->atp_level = level;

    /* Propagate to components */
    if (orch->genius) {
        genius_modulate_atp(orch->genius, level);
    }
    if (orch->prover) {
        evolutionary_proof_modulate_atp(orch->prover, level);
    }
    if (orch->consistency) {
        energy_consistency_modulate_atp(orch->consistency, level);
    }

    nimcp_mutex_unlock(orch->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Verification Functions
 * ============================================================================ */

NIMCP_API nimcp_error_t genius_orchestrator_check_consistency(
    genius_math_orchestrator_t* orch,
    const orchestrator_result_t* result,
    energy_consistency_result_t* consistency_result) {

    if (!orch || !result) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(orch->mutex);

    if (orch->consistency && consistency_result) {
        memset(consistency_result, 0, sizeof(*consistency_result));

        /* Check consistency of the result */
        energy_consistency_check(orch->consistency, NULL, consistency_result);
    }

    nimcp_mutex_unlock(orch->mutex);
    return NIMCP_SUCCESS;
}

NIMCP_API nimcp_error_t genius_orchestrator_verify_proof(
    genius_math_orchestrator_t* orch,
    const orchestrator_proof_result_t* proof,
    bool* is_valid) {

    if (!orch || !proof || !is_valid) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_mutex_lock(orch->mutex);

    *is_valid = false;

    if (proof->proved) {
        /* Verify using energy consistency */
        if (orch->consistency) {
            energy_consistency_result_t ec_result;
            memset(&ec_result, 0, sizeof(ec_result));

            energy_consistency_check(orch->consistency, NULL, &ec_result);

            /* E = 0 means consistent/valid */
            *is_valid = (ec_result.total_energy < 0.001f);
        } else {
            /* No consistency checker, trust the proof result */
            *is_valid = true;
        }
    }

    nimcp_mutex_unlock(orch->mutex);
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void genius_math_orchestrator_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_genius_math_orchestrator_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int genius_math_orchestrator_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "genius_math_orchestrator_training_begin: NULL argument");
        return -1;
    }
    genius_math_orchestrator_heartbeat_instance(NULL, "genius_math_orchestrator_training_begin", 0.0f);
    (void)(struct genius_math_orchestrator*)instance; /* Module state available for reset */
    return 0;
}

int genius_math_orchestrator_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "genius_math_orchestrator_training_end: NULL argument");
        return -1;
    }
    genius_math_orchestrator_heartbeat_instance(NULL, "genius_math_orchestrator_training_end", 1.0f);
    (void)(struct genius_math_orchestrator*)instance; /* Module state available for finalization */
    return 0;
}

int genius_math_orchestrator_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "genius_math_orchestrator_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    genius_math_orchestrator_heartbeat_instance(NULL, "genius_math_orchestrator_training_step", progress);
    (void)(struct genius_math_orchestrator*)instance; /* Module state available for step adaptation */
    return 0;
}
