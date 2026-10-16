//=============================================================================
// nimcp_brain_quantum_reasoning.c - Brain-Level Quantum Reasoning Implementation
//=============================================================================

#include "core/brain/inference/nimcp_brain_quantum_reasoning.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/algorithms/nimcp_monte_carlo.h"
#include "utils/quantum/nimcp_quantum_monte_carlo.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_threshold_constants.h"
#include "constants/nimcp_math_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_quantum_reasoning, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Monte Carlo Integration - Thread-local seed
//=============================================================================

static __thread uint32_t g_bqr_mc_seed = 0;

//=============================================================================
// Internal State
//=============================================================================

/**
 * @brief Brain quantum reasoning context
 *
 * Stored in brain_struct as opaque pointer
 */
typedef struct brain_qreason_ctx_s {
    qreason_t reasoner;              /**< Underlying quantum reasoner */
    brain_qreason_config_t config;   /**< Configuration */
    brain_qreason_stats_t stats;     /**< Statistics */

    /* Modulation state */
    float fatigue_level;             /**< Current fatigue [0,1] */
    float stress_level;              /**< Current stress [0,1] */

    /* Computed modulation factors */
    float effective_max_iterations;  /**< Modulated max iterations */
    float effective_min_confidence;  /**< Modulated confidence threshold */
} brain_qreason_ctx_t;

//=============================================================================
// Configuration API
//=============================================================================

brain_qreason_config_t brain_qreason_default_config(void) {
    return (brain_qreason_config_t){
        .enabled = true,
        .max_grover_iterations = 0,      /* Auto-compute */
        .max_inference_depth = 20,
        .min_confidence = NIMCP_CONFIDENCE_MEDIUM,
        .use_ternary_logic = true,
        .enable_interference = true,
        .integrate_with_executive = true,
        .integrate_with_parietal = true,
        .fatigue_sensitivity = 0.5f,
        .stress_sensitivity = 0.5f
    };
}

//=============================================================================
// Helper Functions
//=============================================================================

static brain_qreason_ctx_t* get_ctx(brain_t brain) {
    if (!brain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;

    }
    struct brain_struct* b = (struct brain_struct*)brain;
    return (brain_qreason_ctx_t*)b->quantum_reasoner;
}

static void update_modulation(brain_qreason_ctx_t* ctx) {
    if (!ctx) return;

    /* Fatigue reduces max iterations */
    float fatigue_factor = 1.0f - (ctx->fatigue_level * ctx->config.fatigue_sensitivity);
    if (fatigue_factor < 0.2f) fatigue_factor = 0.2f;

    /* Stress increases confidence threshold (more conservative) */
    float stress_factor = 1.0f + (ctx->stress_level * ctx->config.stress_sensitivity * 0.5f);
    if (stress_factor > 2.0f) stress_factor = 2.0f;

    ctx->effective_max_iterations = (float)ctx->config.max_grover_iterations * fatigue_factor;
    ctx->effective_min_confidence = ctx->config.min_confidence * stress_factor;
    if (ctx->effective_min_confidence > 0.95f) {
        ctx->effective_min_confidence = 0.95f;
    }
}

//=============================================================================
// Lifecycle API
//=============================================================================

bool nimcp_brain_factory_init_quantum_reasoning(brain_t brain,
                                                  const brain_qreason_config_t* config) {
    if (!brain) {
        NIMCP_LOGGING_ERROR("Cannot init quantum reasoning: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "update_modulation: brain is NULL");
        return false;
    }

    struct brain_struct* b = (struct brain_struct*)brain;

    /* Skip if already initialized */
    if (b->quantum_reasoner != NULL) {
        NIMCP_LOGGING_WARN("Quantum reasoning already initialized");
        return true;
    }

    NIMCP_LOGGING_INFO("Initializing brain quantum reasoning subsystem...");

    /* Allocate context */
    brain_qreason_ctx_t* ctx = nimcp_malloc(sizeof(brain_qreason_ctx_t));
    if (!ctx) {
        NIMCP_LOGGING_ERROR("Failed to allocate quantum reasoning context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "update_modulation: ctx is NULL");
        return false;
    }
    memset(ctx, 0, sizeof(brain_qreason_ctx_t));

    /* Apply configuration */
    ctx->config = config ? *config : brain_qreason_default_config();

    /* Create underlying quantum reasoner */
    qreason_config_t qconfig = qreason_default_config();
    qconfig.max_grover_iterations = ctx->config.max_grover_iterations;
    qconfig.max_inference_depth = ctx->config.max_inference_depth;
    qconfig.min_confidence = ctx->config.min_confidence;
    qconfig.use_ternary_logic = ctx->config.use_ternary_logic;
    qconfig.enable_interference = ctx->config.enable_interference;

    ctx->reasoner = qreason_create(&qconfig);
    if (!ctx->reasoner) {
        NIMCP_LOGGING_ERROR("Failed to create quantum reasoner");
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "update_modulation: ctx->reasoner is NULL");
        return false;
    }

    /* Initialize modulation */
    ctx->fatigue_level = 0.0f;
    ctx->stress_level = 0.0f;
    update_modulation(ctx);

    /* Store in brain */
    b->quantum_reasoner = ctx;
    b->quantum_reasoning_enabled = ctx->config.enabled;

    NIMCP_LOGGING_INFO("Brain quantum reasoning initialized successfully");
    NIMCP_LOGGING_DEBUG("  - Max Grover iterations: %u",
        ctx->config.max_grover_iterations == 0 ? 10 : ctx->config.max_grover_iterations);
    NIMCP_LOGGING_DEBUG("  - Ternary logic: %s", ctx->config.use_ternary_logic ? "enabled" : "disabled");
    NIMCP_LOGGING_DEBUG("  - Interference: %s", ctx->config.enable_interference ? "enabled" : "disabled");

    return true;
}

void nimcp_brain_qreason_destroy(brain_t brain) {
    if (!brain) return;

    struct brain_struct* b = (struct brain_struct*)brain;
    brain_qreason_ctx_t* ctx = (brain_qreason_ctx_t*)b->quantum_reasoner;

    if (ctx) {
        if (ctx->reasoner) {
            qreason_destroy(ctx->reasoner);
        }
        nimcp_free(ctx);
        b->quantum_reasoner = NULL;
        b->quantum_reasoning_enabled = false;
        NIMCP_LOGGING_DEBUG("Brain quantum reasoning destroyed");
    }
}

bool nimcp_brain_qreason_is_enabled(brain_t brain) {
    if (!brain) {
        return false;
    }
    struct brain_struct* b = (struct brain_struct*)brain;
    return b->quantum_reasoning_enabled && b->quantum_reasoner != NULL;
}

int nimcp_brain_qreason_set_enabled(brain_t brain, bool enabled) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_qreason_set_enabled: brain is NULL");
        return -1;
    }

    struct brain_struct* b = (struct brain_struct*)brain;
    brain_qreason_ctx_t* ctx = get_ctx(brain);

    if (!ctx) {
        if (enabled) {
            /* Initialize if enabling and not yet created */
            return nimcp_brain_factory_init_quantum_reasoning(brain, NULL) ? 0 : -1;
        }
        return 0;
    }

    ctx->config.enabled = enabled;
    b->quantum_reasoning_enabled = enabled;
    return 0;
}

//=============================================================================
// Query API
//=============================================================================

int nimcp_brain_qreason_solve_sat(brain_t brain,
                                   const brain_reasoning_query_t* query,
                                   brain_reasoning_result_t* result) {
    if (!brain || !query || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_qreason_set_enabled: required parameter is NULL (brain, query, result)");
        return -1;
    }

    brain_qreason_ctx_t* ctx = get_ctx(brain);
    if (!ctx || !ctx->reasoner) {
        NIMCP_LOGGING_WARN("Quantum reasoning not available");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_qreason_set_enabled: required parameter is NULL (ctx, ctx->reasoner)");
        return -1;
    }

    if (!ctx->config.enabled) {
        NIMCP_LOGGING_DEBUG("Quantum reasoning disabled, skipping");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_qreason_set_enabled: ctx->config is NULL");
        return -1;
    }

    memset(result, 0, sizeof(*result));

    uint64_t start_time = nimcp_time_get_us();

    /* Solve using quantum reasoner */
    qreason_result_t qresult = {0};
    int status = qreason_solve_sat(ctx->reasoner, &query->cnf, &qresult);

    uint64_t end_time = nimcp_time_get_us();

    if (status != 0) {
        NIMCP_LOGGING_ERROR("Quantum SAT solve failed");
        ctx->stats.timeout_count++;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_qreason_set_enabled: validation failed");
        return -1;
    }

    /* Populate result */
    result->satisfiable = qresult.satisfiable;
    result->satisfaction_probability = qresult.satisfaction_prob;
    result->grover_iterations = qresult.grover_iterations;
    result->num_variables = query->cnf.n_variables;
    result->solve_time_us = end_time - start_time;

    /* Copy assignment and compute average confidence */
    float total_confidence = 0.0f;
    uint32_t conf_count = 0;
    for (uint32_t i = 0; i < query->cnf.n_variables && i < BRAIN_QREASON_MAX_VARIABLES; i++) {
        result->assignment[i] = qresult.assignment[i];
        if (qresult.confidences[i] > 0.0f) {
            total_confidence += qresult.confidences[i];
            conf_count++;
        }
    }
    result->confidence = conf_count > 0 ? total_confidence / conf_count : qresult.satisfaction_prob;

    /* Estimate quantum speedup: √(2^N) vs 2^N */
    if (query->cnf.n_variables > 0 && query->cnf.n_variables < 63) {
        float classical_ops = (float)((uint64_t)1 << query->cnf.n_variables);
        float quantum_ops = sqrtf(classical_ops);
        result->quantum_speedup = classical_ops / quantum_ops;
    } else if (query->cnf.n_variables >= 63) {
        /* Cap at maximum representable value to avoid UB */
        result->quantum_speedup = sqrtf((float)((uint64_t)1 << 62));
    }

    /* Update statistics */
    ctx->stats.total_queries++;
    if (result->satisfiable) {
        ctx->stats.satisfiable_count++;
    } else {
        ctx->stats.unsatisfiable_count++;
    }
    ctx->stats.total_grover_iterations += result->grover_iterations;
    ctx->stats.avg_grover_iterations =
        (float)ctx->stats.total_grover_iterations / (float)ctx->stats.total_queries;
    ctx->stats.avg_solve_time_us =
        (ctx->stats.avg_solve_time_us * (ctx->stats.total_queries - 1) + result->solve_time_us)
        / ctx->stats.total_queries;
    ctx->stats.avg_confidence =
        (ctx->stats.avg_confidence * (ctx->stats.total_queries - 1) + result->confidence)
        / ctx->stats.total_queries;

    NIMCP_LOGGING_DEBUG("SAT solve: %s (conf=%.2f, iters=%u, time=%luus)",
        result->satisfiable ? "SAT" : "UNSAT",
        result->confidence,
        result->grover_iterations,
        (unsigned long)result->solve_time_us);

    return 0;
}

int nimcp_brain_qreason_check_goal_feasibility(brain_t brain,
                                                 uint32_t goal_id,
                                                 const uint32_t* constraints,
                                                 uint32_t num_constraints,
                                                 bool* feasible,
                                                 float* confidence) {
    if (!brain || !feasible || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_qreason_set_enabled: required parameter is NULL (brain, feasible, confidence)");
        return -1;
    }

    brain_qreason_ctx_t* ctx = get_ctx(brain);
    if (!ctx || !ctx->reasoner) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_qreason_set_enabled: required parameter is NULL (ctx, ctx->reasoner)");
        return -1;
    }

    /* Build CNF from goal and constraints */
    brain_reasoning_query_t query = {0};
    snprintf(query.description, sizeof(query.description),
             "Goal %u feasibility", goal_id);

    /* Goal variable must be true */
    query.cnf.n_variables = num_constraints + 1;
    query.cnf.n_clauses = num_constraints + 1;

    /* Clause 0: goal must be true */
    query.cnf.clauses[0].n_literals = 1;
    query.cnf.clauses[0].literals[0].variable = goal_id;
    query.cnf.clauses[0].literals[0].negated = false;

    /* Additional clauses from constraints */
    for (uint32_t i = 0; i < num_constraints && i < QREASON_MAX_CLAUSES - 1; i++) {
        query.cnf.clauses[i + 1].n_literals = 1;
        query.cnf.clauses[i + 1].literals[0].variable = constraints[i];
        query.cnf.clauses[i + 1].literals[0].negated = false;
    }

    brain_reasoning_result_t result;
    int status = nimcp_brain_qreason_solve_sat(brain, &query, &result);

    if (status != 0) {
        *feasible = false;
        *confidence = 0.0f;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_brain_qreason_set_enabled: validation failed");
        return -1;
    }

    *feasible = result.satisfiable;
    *confidence = result.confidence;

    return 0;
}

int nimcp_brain_qreason_query_ternary(brain_t brain,
                                        uint32_t variable,
                                        qreason_truth_t* value,
                                        float* confidence) {
    if (!brain || !value || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_qreason_set_enabled: required parameter is NULL (brain, value, confidence)");
        return -1;
    }

    brain_qreason_ctx_t* ctx = get_ctx(brain);
    if (!ctx || !ctx->reasoner) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_qreason_set_enabled: required parameter is NULL (ctx, ctx->reasoner)");
        return -1;
    }

    *value = qreason_get_fact(ctx->reasoner, variable, confidence);
    return 0;
}

//=============================================================================
// Knowledge Base API
//=============================================================================

int nimcp_brain_qreason_set_fact(brain_t brain,
                                   uint32_t variable,
                                   qreason_truth_t value,
                                   float confidence) {
    brain_qreason_ctx_t* ctx = get_ctx(brain);
    if (!ctx || !ctx->reasoner) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_qreason_set_enabled: required parameter is NULL (ctx, ctx->reasoner)");
        return -1;
    }

    return qreason_set_fact(ctx->reasoner, variable, value, confidence);
}

int nimcp_brain_qreason_get_fact(brain_t brain,
                                   uint32_t variable,
                                   qreason_truth_t* value,
                                   float* confidence) {
    if (!value || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_qreason_set_enabled: required parameter is NULL (value, confidence)");
        return -1;
    }

    brain_qreason_ctx_t* ctx = get_ctx(brain);
    if (!ctx || !ctx->reasoner) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_qreason_set_enabled: required parameter is NULL (ctx, ctx->reasoner)");
        return -1;
    }

    *value = qreason_get_fact(ctx->reasoner, variable, confidence);
    return 0;
}

int nimcp_brain_qreason_add_rule(brain_t brain,
                                   const uint32_t* antecedents,
                                   uint32_t num_antecedents,
                                   uint32_t consequent,
                                   float confidence) {
    brain_qreason_ctx_t* ctx = get_ctx(brain);
    if (!ctx || !ctx->reasoner) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (ctx, ctx->reasoner)");
        return -1;
    }

    return qreason_add_rule(ctx->reasoner, antecedents, num_antecedents,
                            consequent, confidence);
}

int nimcp_brain_qreason_clear_kb(brain_t brain) {
    brain_qreason_ctx_t* ctx = get_ctx(brain);
    if (!ctx || !ctx->reasoner) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_qreason_clear_kb: required parameter is NULL (ctx, ctx->reasoner)");
        return -1;
    }

    qreason_clear_facts(ctx->reasoner);
    qreason_clear_rules(ctx->reasoner);
    return 0;
}

//=============================================================================
// Modulation API
//=============================================================================

int nimcp_brain_qreason_set_fatigue(brain_t brain, float fatigue_level) {
    brain_qreason_ctx_t* ctx = get_ctx(brain);
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_qreason_set_fatigue: ctx is NULL");
        return -1;
    }

    ctx->fatigue_level = fatigue_level < 0.0f ? 0.0f :
                         (fatigue_level > 1.0f ? 1.0f : fatigue_level);
    update_modulation(ctx);

    NIMCP_LOGGING_DEBUG("Quantum reasoning fatigue set to %.2f (eff_iters=%.0f)",
        ctx->fatigue_level, ctx->effective_max_iterations);

    return 0;
}

int nimcp_brain_qreason_set_stress(brain_t brain, float stress_level) {
    brain_qreason_ctx_t* ctx = get_ctx(brain);
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_qreason_set_stress: ctx is NULL");
        return -1;
    }

    ctx->stress_level = stress_level < 0.0f ? 0.0f :
                        (stress_level > 1.0f ? 1.0f : stress_level);
    update_modulation(ctx);

    NIMCP_LOGGING_DEBUG("Quantum reasoning stress set to %.2f (eff_conf=%.2f)",
        ctx->stress_level, ctx->effective_min_confidence);

    return 0;
}

//=============================================================================
// Statistics API
//=============================================================================

int nimcp_brain_qreason_get_stats(brain_t brain, brain_qreason_stats_t* stats) {
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_qreason_get_stats: stats is NULL");
        return -1;
    }

    brain_qreason_ctx_t* ctx = get_ctx(brain);
    if (!ctx) {
        memset(stats, 0, sizeof(*stats));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_qreason_get_stats: ctx is NULL");
        return -1;
    }

    *stats = ctx->stats;
    return 0;
}

void nimcp_brain_qreason_reset_stats(brain_t brain) {
    brain_qreason_ctx_t* ctx = get_ctx(brain);
    if (ctx) {
        memset(&ctx->stats, 0, sizeof(ctx->stats));
    }
}

//=============================================================================
// Internal Access
//=============================================================================

qreason_t nimcp_brain_qreason_get_handle(brain_t brain) {
    brain_qreason_ctx_t* ctx = get_ctx(brain);
    return ctx ? ctx->reasoner : NULL;
}

//=============================================================================
// Monte Carlo Enhanced Functions
//=============================================================================

/**
 * @brief Convert qreason CNF to QMC CNF format
 *
 * QMC uses int32_t literals where:
 * - positive literal i means variable i is true
 * - negative literal -i means variable i is false
 */
static bool convert_to_qmc_cnf(const qreason_cnf_t* src, qmc_cnf_t* dst) {
    if (!src || !dst) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "convert_to_qmc_cnf: required parameter is NULL (src, dst)");
        return false;
    }

    memset(dst, 0, sizeof(*dst));
    dst->num_variables = src->n_variables;
    dst->num_clauses = src->n_clauses;

    /* Allocate clause storage */
    dst->clauses = nimcp_calloc(dst->num_clauses, sizeof(qmc_clause_t));
    if (!dst->clauses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "convert_to_qmc_cnf: dst->clauses is NULL");
        return false;
    }

    for (uint32_t c = 0; c < src->n_clauses; c++) {
        const qreason_clause_t* sq = &src->clauses[c];
        qmc_clause_t* dq = &dst->clauses[c];

        dq->num_literals = sq->n_literals;
        dq->literals = nimcp_calloc(sq->n_literals, sizeof(int32_t));
        if (!dq->literals) {
            /* Cleanup on failure */
            for (uint32_t i = 0; i < c; i++) {
                if (dst->clauses[i].literals) {
                    nimcp_free(dst->clauses[i].literals);
                }
            }
            nimcp_free(dst->clauses);
            dst->clauses = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "convert_to_qmc_cnf: validation failed");
            return false;
        }

        for (uint32_t l = 0; l < sq->n_literals; l++) {
            /* Convert to QMC format: +var for true, -var for negated */
            int32_t lit = (int32_t)(sq->literals[l].variable + 1);  /* +1 because 0 is not valid */
            if (sq->literals[l].negated) {
                lit = -lit;
            }
            dq->literals[l] = lit;
        }
    }

    return true;
}

/**
 * @brief Free QMC CNF resources
 */
static void free_qmc_cnf(qmc_cnf_t* cnf) {
    if (cnf && cnf->clauses) {
        for (uint32_t c = 0; c < cnf->num_clauses; c++) {
            if (cnf->clauses[c].literals) {
                nimcp_free(cnf->clauses[c].literals);
            }
        }
        nimcp_free(cnf->clauses);
        cnf->clauses = NULL;
    }
}

/**
 * @brief Solve SAT using MCTS-guided search
 *
 * WHAT: Use Monte Carlo Tree Search for SAT solving
 * WHY:  MCTS can find solutions faster via learned exploration
 * HOW:  Use qmc_solve_sat_mcts with variable-branching tree
 *
 * @param brain Brain handle
 * @param query Reasoning query with CNF formula
 * @param result Output: reasoning result
 * @return 0 on success, -1 on error
 */
int nimcp_brain_qreason_solve_sat_mcts(brain_t brain,
                                        const brain_reasoning_query_t* query,
                                        brain_reasoning_result_t* result) {
    if (!brain || !query || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "free_qmc_cnf: required parameter is NULL (brain, query, result)");
        return -1;
    }

    brain_qreason_ctx_t* ctx = get_ctx(brain);
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "free_qmc_cnf: ctx is NULL");
        return -1;
    }

    if (g_bqr_mc_seed == 0) {
        g_bqr_mc_seed = mc_seed_from_time();
    }

    memset(result, 0, sizeof(*result));
    uint64_t start_time = nimcp_time_get_us();

    /* Convert CNF format */
    qmc_cnf_t qmc_cnf;
    if (!convert_to_qmc_cnf(&query->cnf, &qmc_cnf)) {
        NIMCP_LOGGING_ERROR("Failed to convert CNF for MCTS solver");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "free_qmc_cnf: convert_to_qmc_cnf is NULL");
        return -1;
    }

    /* Set up MCTS SAT config */
    qmc_sat_config_t config = {
        .mcts_iterations = ctx->config.max_grover_iterations > 0 ?
                          ctx->config.max_grover_iterations * 100 : 1000,
        .max_depth = query->cnf.n_variables,
        .exploration_constant = NIMCP_SQRT2_F,  /* sqrt(2) */
        .use_unit_propagation = true,
        .seed = g_bqr_mc_seed
    };

    qmc_sat_result_t sat_result;
    memset(&sat_result, 0, sizeof(sat_result));

    qmc_result_t status = qmc_solve_sat_mcts(&qmc_cnf, &config, &sat_result);

    uint64_t end_time = nimcp_time_get_us();

    /* Copy result */
    result->satisfiable = sat_result.satisfiable;
    result->confidence = sat_result.confidence;

    /* Copy assignment */
    for (uint32_t i = 0; i < query->cnf.n_variables && i < QREASON_MAX_VARIABLES; i++) {
        if (sat_result.assignment) {
            result->assignment[i] = sat_result.assignment[i] ? QREASON_TRUE : QREASON_FALSE;
        }
    }
    result->num_variables = query->cnf.n_variables;

    result->grover_iterations = sat_result.nodes_explored;
    result->solve_time_us = end_time - start_time;

    /* Update stats */
    ctx->stats.total_queries++;
    if (result->satisfiable) {
        ctx->stats.satisfiable_count++;
    } else {
        ctx->stats.unsatisfiable_count++;
    }
    ctx->stats.total_grover_iterations += sat_result.nodes_explored;

    /* Update seed */
    g_bqr_mc_seed = config.seed;

    /* Cleanup */
    qmc_sat_result_free(&sat_result);
    free_qmc_cnf(&qmc_cnf);

    return (status == QMC_OK) ? 0 : -1;
}

/**
 * @brief Estimate satisfiability probability via Monte Carlo
 *
 * WHAT: Estimate P(SAT) using random sampling
 * WHY:  Quick estimate without full solving
 * HOW:  Sample random assignments, check clause satisfaction
 *
 * @param brain Brain handle
 * @param query Reasoning query
 * @param num_samples Number of MC samples
 * @param probability_out Output: estimated P(SAT)
 * @param variance_out Output: estimate variance
 * @return 0 on success
 */
int nimcp_brain_qreason_estimate_sat_prob_mc(brain_t brain,
                                              const brain_reasoning_query_t* query,
                                              uint32_t num_samples,
                                              float* probability_out,
                                              float* variance_out) {
    if (!brain || !query || !probability_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "free_qmc_cnf: required parameter is NULL (brain, query, probability_out)");
        return -1;
    }

    brain_qreason_ctx_t* ctx = get_ctx(brain);
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "free_qmc_cnf: ctx is NULL");
        return -1;
    }

    if (g_bqr_mc_seed == 0) {
        g_bqr_mc_seed = mc_seed_from_time();
    }

    const qreason_cnf_t* cnf = &query->cnf;
    if (cnf->n_variables == 0) {
        *probability_out = 1.0f;
        if (variance_out) *variance_out = 0.0f;
        return 0;
    }

    uint32_t num_sat = 0;

    for (uint32_t s = 0; s < num_samples; s++) {
        /* Generate random assignment */
        uint32_t assignment = mc_random_int(&g_bqr_mc_seed, 1U << cnf->n_variables);

        /* Check if satisfies all clauses */
        bool sat = true;
        for (uint32_t c = 0; c < cnf->n_clauses && sat; c++) {
            const qreason_clause_t* clause = &cnf->clauses[c];
            bool clause_sat = false;

            for (uint32_t l = 0; l < clause->n_literals; l++) {
                uint32_t var = clause->literals[l].variable;
                bool val = (assignment >> var) & 1;
                if (clause->literals[l].negated) val = !val;
                if (val) {
                    clause_sat = true;
                    break;
                }
            }

            if (!clause_sat) sat = false;
        }

        if (sat) num_sat++;
    }

    *probability_out = (float)num_sat / (float)num_samples;

    if (variance_out) {
        /* Bernoulli variance: p(1-p)/n */
        float p = *probability_out;
        *variance_out = (p * (1.0f - p)) / (float)num_samples;
    }

    return 0;
}

/**
 * @brief Get thread-local MC seed for brain quantum reasoning
 *
 * @return Pointer to thread-local seed
 */
uint32_t* nimcp_brain_qreason_get_mc_seed(void) {
    if (g_bqr_mc_seed == 0) {
        g_bqr_mc_seed = mc_seed_from_time();
    }
    return &g_bqr_mc_seed;
}
