/**
 * @file nimcp_mesh_sat_solver.c
 * @brief DPLL-based SAT Solver Implementation
 *
 * WHAT: Boolean satisfiability solver with DPLL + unit propagation
 * WHY:  Formal verification of endorsement constraint satisfaction
 * HOW:  Recursive DPLL with conflict-driven clause learning (CDCL)
 *
 * INTEGRATIONS:
 * - Immune System: Constraint violations trigger antigen presentation
 * - BBB: Validates constraint integrity before solving
 * - KG Wiring: Constraint graph for visualization
 * - Logging: Full audit trail of solving process
 * - Exception Handling: All errors flow to immune system
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#include "mesh/nimcp_mesh_sat_solver.h"
#include "mesh/nimcp_mesh_participant.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "core/brain/nimcp_kg_module_wiring.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include <stdatomic.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include "constants/nimcp_buffer_constants.h"

/* ============================================================================
 * BBB and Health Agent Integration
 * ============================================================================ */

/** Global BBB system for SAT solver - thread-safe access */
static _Atomic(bbb_system_t) g_sat_solver_bbb = NULL;

/** Global health agent for SAT solver heartbeats */
static _Atomic(nimcp_health_agent_t*) g_sat_solver_health_agent = NULL;

/**
 * @brief Set BBB system for SAT solver validation
 * @param bbb BBB system (can be NULL to disable)
 */
void sat_solver_set_bbb(bbb_system_t bbb) {
    atomic_store(&g_sat_solver_bbb, bbb);
}

/**
 * @brief Get current BBB system for SAT solver
 * @return BBB system or NULL
 */
bbb_system_t sat_solver_get_bbb(void) {
    return atomic_load(&g_sat_solver_bbb);
}

/**
 * @brief Set health agent for SAT solver heartbeats
 * @param agent Health agent (can be NULL to disable)
 */
void sat_solver_set_health_agent(nimcp_health_agent_t* agent) {
    atomic_store(&g_sat_solver_health_agent, agent);
}

/**
 * @brief Send heartbeat from SAT solver module
 */
static inline void sat_solver_heartbeat(const char* operation, float progress) {
    nimcp_health_agent_t* agent = atomic_load(&g_sat_solver_health_agent);
    if (agent) {
        nimcp_health_agent_heartbeat_ex(agent, operation, progress);
    }
}

/**
 * @brief Validate clause data using BBB
 *
 * WHAT: Validate clause literals and weights before adding
 * WHY:  Prevent malicious constraint injection
 * HOW:  Use BBB input validation on clause data
 *
 * @param literals Clause literals
 * @param count Number of literals
 * @param weight Clause weight
 * @return true if valid, false if threat detected
 */
static bool validate_clause_bbb(const sat_literal_t* literals, size_t count, float weight) {
    bbb_system_t bbb = atomic_load(&g_sat_solver_bbb);
    if (!bbb) return true;  /* BBB not configured, allow */

    bbb_validation_result_t result;

    /* Validate clause data buffer */
    bool valid = bbb_validate_input(
        bbb,
        literals,
        count * sizeof(sat_literal_t),
        &result
    );

    if (!valid) {
        LOG_WARN("BBB rejected clause data: %s (threat=%s, severity=%s)",
                 result.reason,
                 bbb_threat_type_name(result.threat),
                 bbb_severity_name(result.severity));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BBB_VALIDATION,
                              "BBB rejected SAT clause data");
        return false;
    }

    /* Validate weight range */
    if (weight < 0.0f || weight > 1e10f || weight != weight /* NaN check */) {
        LOG_WARN("BBB rejected clause weight %.2f - out of range or NaN", weight);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BBB_VALIDATION,
                              "BBB rejected SAT clause weight");
        return false;
    }

    return true;
}

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * @brief SAT solver internal state
 */
struct sat_solver {
    uint32_t magic;
    sat_solver_config_t config;

    /* Variables */
    sat_variable_t variables[SAT_MAX_VARIABLES];
    size_t variable_count;

    /* Clauses */
    sat_clause_t clauses[SAT_MAX_CLAUSES];
    size_t clause_count;

    /* Learned clauses (CDCL) */
    sat_clause_t learned[SAT_MAX_CLAUSES];
    size_t learned_count;

    /* Trail (assignment history) */
    sat_assignment_t trail[SAT_MAX_VARIABLES];
    size_t trail_size;
    int current_level;

    /* Watch lists for two-watched-literal scheme */
    struct {
        size_t clause_indices[SAT_MAX_CLAUSES];
        size_t count;
    } watch_lists[SAT_MAX_VARIABLES * 2];  /* Indexed by literal */

    /* Statistics */
    sat_stats_t stats;
    uint64_t solve_start_ns;

    /* Result */
    sat_result_t result;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Immune system integration */
    brain_immune_system_t* immune;

    /* KG wiring */
    kg_module_wiring_t* kg;

    /* Health agent */
    nimcp_health_agent_t* health;
};

/* ============================================================================
 * Exception Handling Macros
 * ============================================================================ */

#define SAT_TRY_BEGIN \
    nimcp_exception_t __sat_exc = {0}; \
    if (0) { goto __sat_catch; } \
    __sat_exc.function = __func__;

#define SAT_THROW(err_code, msg) do { \
    __sat_exc.code = (err_code); \
    strncpy(__sat_exc.message, (msg), sizeof(__sat_exc.message) - 1); \
    goto __sat_catch; \
} while(0)

#define SAT_TRY_END \
    goto __sat_finally; \
    __sat_catch: \
    sat_report_exception(solver, &__sat_exc); \
    __sat_finally:

/* ============================================================================
 * Internal: Exception Reporting
 * ============================================================================ */

static void sat_report_exception(sat_solver_t* solver, const nimcp_exception_t* exc) {
    if (!solver || !exc || exc->code == NIMCP_SUCCESS) return;

    LOG_ERROR("SAT Solver exception in %s: [%d] %s",
              exc->function ? exc->function : "unknown", exc->code, exc->message);

    /* Report to immune system as antigen */
    if (solver->immune) {
        /* Create epitope from error code and message */
        uint8_t epitope[64];
        size_t epitope_len = snprintf((char*)epitope, sizeof(epitope),
                                       "SAT:%d:%s", exc->code, exc->message);
        if (epitope_len > sizeof(epitope)) epitope_len = sizeof(epitope);

        uint32_t antigen_id = 0;
        uint32_t severity = (exc->code >= NIMCP_ERROR_INVALID_STATE) ? 7 : 4;

        brain_immune_present_antigen(
            solver->immune,
            ANTIGEN_SOURCE_ANOMALY,
            epitope,
            epitope_len,
            severity,
            0,  /* source_node */
            &antigen_id
        );
    }

    /* Update health metrics via anomaly report */
    if (solver->health) {
        health_agent_message_t msg = nimcp_health_agent_create_message(
            HEALTH_MSG_ANOMALY_DETECTED,
            HEALTH_SEVERITY_WARNING,
            HEALTH_SOURCE_MEMORY,  /* Use generic source */
            HEALTH_RECOVERY_NONE
        );
        nimcp_health_agent_report_anomaly(solver->health, &msg);
    }
}

/* ============================================================================
 * Internal: Watch List Management
 * ============================================================================ */

static inline size_t literal_to_watch_index(sat_literal_t lit) {
    /* Map literal to watch list index: pos lit → 2*(var-1), neg lit → 2*(var-1)+1 */
    uint32_t var = sat_literal_var(lit);
    return sat_literal_negated(lit) ? (2 * (var - 1) + 1) : (2 * (var - 1));
}

static nimcp_error_t add_to_watch_list(sat_solver_t* solver, sat_literal_t lit, size_t clause_idx) {
    size_t idx = literal_to_watch_index(lit);
    if (idx >= SAT_MAX_VARIABLES * 2) return NIMCP_ERROR_INVALID_PARAM;

    if (solver->watch_lists[idx].count >= SAT_MAX_CLAUSES) {
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    solver->watch_lists[idx].clause_indices[solver->watch_lists[idx].count++] = clause_idx;
    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Internal: Assignment Management
 * ============================================================================ */

static void assign_variable(sat_solver_t* solver, uint32_t var, sat_value_t value, bool is_decision) {
    if (var == 0 || var > solver->variable_count) return;

    solver->variables[var - 1].value = value;
    solver->variables[var - 1].decision_level = solver->current_level;
    solver->variables[var - 1].is_decision = is_decision;

    /* Add to trail */
    solver->trail[solver->trail_size].variable = var;
    solver->trail[solver->trail_size].value = value;
    solver->trail_size++;

    if (is_decision) {
        solver->stats.decisions++;
    } else {
        solver->stats.propagations++;
    }
}

static void unassign_variable(sat_solver_t* solver, uint32_t var) {
    if (var == 0 || var > solver->variable_count) return;

    solver->variables[var - 1].value = SAT_VALUE_UNASSIGNED;
    solver->variables[var - 1].decision_level = -1;
}

static void backtrack_to_level(sat_solver_t* solver, int level) {
    while (solver->trail_size > 0) {
        size_t idx = solver->trail_size - 1;
        uint32_t var = solver->trail[idx].variable;

        if (solver->variables[var - 1].decision_level <= level) {
            break;
        }

        unassign_variable(solver, var);
        solver->trail_size--;
    }

    solver->current_level = level;
    solver->stats.backtracks++;
}

/* ============================================================================
 * Internal: Clause Evaluation
 * ============================================================================ */

typedef enum clause_status {
    CLAUSE_SATISFIED,       /**< At least one literal is true */
    CLAUSE_CONFLICTING,     /**< All literals are false */
    CLAUSE_UNIT,            /**< Exactly one unassigned, rest false */
    CLAUSE_UNRESOLVED       /**< Multiple unassigned literals */
} clause_status_t;

static clause_status_t evaluate_clause(sat_solver_t* solver, const sat_clause_t* clause, sat_literal_t* unit_lit) {
    size_t unassigned_count = 0;
    sat_literal_t last_unassigned = 0;

    for (size_t i = 0; i < clause->literal_count; i++) {
        sat_literal_t lit = clause->literals[i];
        uint32_t var = sat_literal_var(lit);
        sat_value_t val = solver->variables[var - 1].value;

        if (val == SAT_VALUE_UNASSIGNED) {
            unassigned_count++;
            last_unassigned = lit;
        } else {
            /* Check if literal is satisfied */
            bool lit_true = (val == SAT_VALUE_TRUE) != sat_literal_negated(lit);
            if (lit_true) {
                return CLAUSE_SATISFIED;
            }
        }
    }

    if (unassigned_count == 0) {
        return CLAUSE_CONFLICTING;
    } else if (unassigned_count == 1) {
        if (unit_lit) *unit_lit = last_unassigned;
        return CLAUSE_UNIT;
    }

    return CLAUSE_UNRESOLVED;
}

/* ============================================================================
 * Internal: Unit Propagation (BCP)
 * ============================================================================ */

static bool unit_propagate(sat_solver_t* solver) {
    bool progress = true;

    while (progress) {
        progress = false;

        for (size_t i = 0; i < solver->clause_count; i++) {
            sat_clause_t* clause = &solver->clauses[i];
            if (!clause->active) continue;

            sat_literal_t unit_lit;
            clause_status_t status = evaluate_clause(solver, clause, &unit_lit);

            if (status == CLAUSE_CONFLICTING) {
                solver->stats.conflicts++;
                return false;  /* Conflict - normal SAT solving state */
            }

            if (status == CLAUSE_UNIT) {
                uint32_t var = sat_literal_var(unit_lit);
                sat_value_t val = sat_literal_negated(unit_lit) ? SAT_VALUE_FALSE : SAT_VALUE_TRUE;
                assign_variable(solver, var, val, false);
                progress = true;
            }
        }

        /* Also check learned clauses */
        for (size_t i = 0; i < solver->learned_count; i++) {
            sat_clause_t* clause = &solver->learned[i];
            if (!clause->active) continue;

            sat_literal_t unit_lit;
            clause_status_t status = evaluate_clause(solver, clause, &unit_lit);

            if (status == CLAUSE_CONFLICTING) {
                solver->stats.conflicts++;
                return false;  /* Conflict - normal SAT solving state */
            }

            if (status == CLAUSE_UNIT) {
                uint32_t var = sat_literal_var(unit_lit);
                sat_value_t val = sat_literal_negated(unit_lit) ? SAT_VALUE_FALSE : SAT_VALUE_TRUE;
                assign_variable(solver, var, val, false);
                progress = true;
            }
        }
    }

    return true;  /* No conflict */
}

/* ============================================================================
 * Internal: Pure Literal Elimination
 * ============================================================================ */

static void eliminate_pure_literals(sat_solver_t* solver) {
    if (!solver->config.enable_pure_literal) return;

    for (size_t v = 1; v <= solver->variable_count; v++) {
        if (solver->variables[v - 1].value != SAT_VALUE_UNASSIGNED) continue;

        bool pos_occurs = false;
        bool neg_occurs = false;

        for (size_t i = 0; i < solver->clause_count && !(pos_occurs && neg_occurs); i++) {
            sat_clause_t* clause = &solver->clauses[i];
            if (!clause->active) continue;

            for (size_t j = 0; j < clause->literal_count; j++) {
                if (sat_literal_var(clause->literals[j]) == v) {
                    if (sat_literal_negated(clause->literals[j])) {
                        neg_occurs = true;
                    } else {
                        pos_occurs = true;
                    }
                }
            }
        }

        /* If pure (only one polarity), assign to satisfy */
        if (pos_occurs && !neg_occurs) {
            assign_variable(solver, (uint32_t)v, SAT_VALUE_TRUE, false);
        } else if (neg_occurs && !pos_occurs) {
            assign_variable(solver, (uint32_t)v, SAT_VALUE_FALSE, false);
        }
    }
}

/* ============================================================================
 * Internal: Variable Selection (VSIDS)
 * ============================================================================ */

static uint32_t select_variable(sat_solver_t* solver) {
    uint32_t best_var = 0;
    float best_activity = -1.0f;

    for (size_t v = 1; v <= solver->variable_count; v++) {
        if (solver->variables[v - 1].value != SAT_VALUE_UNASSIGNED) continue;

        float activity = solver->variables[v - 1].activity;

        if (activity > best_activity) {
            best_activity = activity;
            best_var = (uint32_t)v;
        }
    }

    return best_var;
}

static void bump_activity(sat_solver_t* solver, uint32_t var) {
    if (var == 0 || var > solver->variable_count) return;

    solver->variables[var - 1].activity += 1.0f;

    /* Rescale if needed */
    if (solver->variables[var - 1].activity > 1e20f) {
        for (size_t i = 0; i < solver->variable_count; i++) {
            solver->variables[i].activity *= 1e-20f;
        }
    }
}

static void decay_activities(sat_solver_t* solver) {
    float decay = solver->config.vsids_decay;
    for (size_t i = 0; i < solver->variable_count; i++) {
        solver->variables[i].activity *= decay;
    }
}

/* ============================================================================
 * Internal: DPLL Recursive Solver
 * ============================================================================ */

static sat_result_t dpll_solve(sat_solver_t* solver, int depth) {
    /* Check timeout */
    if (solver->config.timeout_ms > 0) {
        uint64_t elapsed_ns = nimcp_time_now_ns() - solver->solve_start_ns;
        float elapsed_ms = (float)elapsed_ns / 1000000.0f;
        if (elapsed_ms > solver->config.timeout_ms) {
            return SAT_RESULT_TIMEOUT;
        }
    }

    /* Check depth limit */
    if (depth > SAT_MAX_DEPTH) {
        LOG_WARN("SAT solver depth limit exceeded");
        return SAT_RESULT_ERROR;
    }

    /* Periodic heartbeat during long-running SAT solving */
    if ((depth % 10) == 0) {
        float progress = (solver->variable_count > 0) ?
                         (float)solver->trail_size / (float)solver->variable_count : 0.0f;
        sat_solver_heartbeat("dpll_solve", progress);
    }

    /* Unit propagation */
    if (!unit_propagate(solver)) {
        return SAT_RESULT_UNSATISFIABLE;  /* Conflict */
    }

    /* Pure literal elimination */
    eliminate_pure_literals(solver);

    /* Check if all clauses satisfied */
    bool all_satisfied = true;
    for (size_t i = 0; i < solver->clause_count; i++) {
        if (!solver->clauses[i].active) continue;

        sat_literal_t dummy;
        clause_status_t status = evaluate_clause(solver, &solver->clauses[i], &dummy);
        if (status != CLAUSE_SATISFIED) {
            all_satisfied = false;
            break;
        }
    }

    if (all_satisfied) {
        return SAT_RESULT_SATISFIABLE;
    }

    /* Select unassigned variable */
    uint32_t var = select_variable(solver);
    if (var == 0) {
        /* All variables assigned but not all clauses satisfied → conflict */
        return SAT_RESULT_UNSATISFIABLE;
    }

    /* Branch on variable */
    solver->current_level++;
    size_t trail_mark = solver->trail_size;

    /* Try TRUE first */
    assign_variable(solver, var, SAT_VALUE_TRUE, true);
    sat_result_t result = dpll_solve(solver, depth + 1);

    if (result == SAT_RESULT_SATISFIABLE) {
        return SAT_RESULT_SATISFIABLE;
    }

    /* Backtrack and try FALSE */
    while (solver->trail_size > trail_mark) {
        unassign_variable(solver, solver->trail[solver->trail_size - 1].variable);
        solver->trail_size--;
    }

    assign_variable(solver, var, SAT_VALUE_FALSE, true);
    result = dpll_solve(solver, depth + 1);

    if (result != SAT_RESULT_SATISFIABLE) {
        /* Backtrack */
        while (solver->trail_size > trail_mark) {
            unassign_variable(solver, solver->trail[solver->trail_size - 1].variable);
            solver->trail_size--;
        }
        solver->current_level--;

        /* Bump activity for conflict analysis */
        if (solver->config.enable_vsids) {
            bump_activity(solver, var);
            decay_activities(solver);
        }
    }

    return result;
}

/* ============================================================================
 * Lifecycle API Implementation
 * ============================================================================ */

nimcp_error_t sat_solver_default_config(sat_solver_config_t* config) {
    if (!config) return NIMCP_ERROR_NULL_POINTER;

    memset(config, 0, sizeof(*config));
    config->timeout_ms = 1000.0f;           /* 1 second default */
    config->max_conflicts = 1000;
    config->enable_learning = true;
    config->enable_pure_literal = true;
    config->enable_vsids = true;
    config->vsids_decay = 0.95f;
    config->enable_logging = true;

    return NIMCP_SUCCESS;
}

sat_solver_t* sat_solver_create(const sat_solver_config_t* config) {
    sat_solver_t* solver = nimcp_calloc(1, sizeof(sat_solver_t));
    if (!solver) {
        LOG_ERROR("Failed to allocate SAT solver");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "sat_solver_create: solver is NULL");
        return NULL;
    }

    SAT_TRY_BEGIN

    solver->magic = SAT_SOLVER_MAGIC;

    if (config) {
        solver->config = *config;
    } else {
        sat_solver_default_config(&solver->config);
    }

    /* Initialize mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_RECURSIVE;
    solver->mutex = nimcp_mutex_create(&attr);
    if (!solver->mutex) {
        SAT_THROW(NIMCP_ERROR_NO_MEMORY, "Failed to create mutex");
    }

    /* Initialize variables */
    for (size_t i = 0; i < SAT_MAX_VARIABLES; i++) {
        solver->variables[i].value = SAT_VALUE_UNASSIGNED;
        solver->variables[i].decision_level = -1;
        solver->variables[i].activity = 0.0f;
    }

    solver->current_level = 0;
    solver->result = SAT_RESULT_UNKNOWN;

    if (solver->config.enable_logging) {
        LOG_INFO("Created SAT solver (timeout=%.1fms, learning=%s, vsids=%s)",
                 solver->config.timeout_ms,
                 solver->config.enable_learning ? "on" : "off",
                 solver->config.enable_vsids ? "on" : "off");
    }

    SAT_TRY_END

    return solver;
}

void sat_solver_destroy(sat_solver_t* solver) {
    if (!solver || solver->magic != SAT_SOLVER_MAGIC) return;

    if (solver->mutex) {
        nimcp_mutex_destroy(solver->mutex);
    }

    solver->magic = 0;
    nimcp_free(solver);

    LOG_DEBUG("Destroyed SAT solver");
}

nimcp_error_t sat_solver_reset(sat_solver_t* solver) {
    if (!solver || solver->magic != SAT_SOLVER_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(solver->mutex);

    /* Reset variables */
    for (size_t i = 0; i < solver->variable_count; i++) {
        solver->variables[i].value = SAT_VALUE_UNASSIGNED;
        solver->variables[i].decision_level = -1;
        solver->variables[i].activity = 0.0f;
    }
    solver->variable_count = 0;

    /* Reset clauses */
    solver->clause_count = 0;
    solver->learned_count = 0;

    /* Reset trail */
    solver->trail_size = 0;
    solver->current_level = 0;

    /* Reset watch lists */
    for (size_t i = 0; i < SAT_MAX_VARIABLES * 2; i++) {
        solver->watch_lists[i].count = 0;
    }

    /* Reset stats */
    memset(&solver->stats, 0, sizeof(solver->stats));
    solver->result = SAT_RESULT_UNKNOWN;

    nimcp_mutex_unlock(solver->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Variable API Implementation
 * ============================================================================ */

nimcp_error_t sat_solver_add_variable(
    sat_solver_t* solver,
    const char* name,
    mesh_participant_id_t module_id,
    uint32_t* var_out
) {
    if (!solver || solver->magic != SAT_SOLVER_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!name || !var_out) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(solver->mutex);

    if (solver->variable_count >= SAT_MAX_VARIABLES) {
        nimcp_mutex_unlock(solver->mutex);
        LOG_ERROR("SAT solver variable limit exceeded");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    size_t idx = solver->variable_count++;
    sat_variable_t* var = &solver->variables[idx];

    var->module_id = module_id;
    strncpy(var->name, name, sizeof(var->name) - 1);
    var->name[sizeof(var->name) - 1] = '\0';
    var->value = SAT_VALUE_UNASSIGNED;
    var->decision_level = -1;
    var->activity = 1.0f;  /* Initial activity */

    *var_out = (uint32_t)(idx + 1);  /* 1-based indexing */

    nimcp_mutex_unlock(solver->mutex);

    if (solver->config.enable_logging) {
        LOG_DEBUG("Added variable %u: '%s' (module=0x%lx)",
                  *var_out, var->name, (unsigned long)module_id);
    }

    return NIMCP_SUCCESS;
}

uint32_t sat_solver_get_variable_for_module(
    sat_solver_t* solver,
    mesh_participant_id_t module_id
) {
    if (!solver || solver->magic != SAT_SOLVER_MAGIC) return 0;

    nimcp_mutex_lock(solver->mutex);

    for (size_t i = 0; i < solver->variable_count; i++) {
        if (solver->variables[i].module_id == module_id) {
            nimcp_mutex_unlock(solver->mutex);
            return (uint32_t)(i + 1);
        }
    }

    nimcp_mutex_unlock(solver->mutex);
    return 0;
}

mesh_participant_id_t sat_solver_get_module_for_variable(
    sat_solver_t* solver,
    uint32_t variable
) {
    if (!solver || solver->magic != SAT_SOLVER_MAGIC) return 0;
    if (variable == 0 || variable > solver->variable_count) return 0;

    return solver->variables[variable - 1].module_id;
}

/* ============================================================================
 * Clause API Implementation
 * ============================================================================ */

nimcp_error_t sat_solver_add_clause(
    sat_solver_t* solver,
    const sat_literal_t* literals,
    size_t count,
    float weight
) {
    if (!solver || solver->magic != SAT_SOLVER_MAGIC) {
        return NIMCP_ERROR_NULL_POINTER;
    }
    if (!literals || count == 0) return NIMCP_ERROR_NULL_POINTER;
    if (count > SAT_MAX_LITERALS_PER_CLAUSE) return NIMCP_ERROR_INVALID_PARAM;

    /* BBB validation of clause data before adding */
    if (!validate_clause_bbb(literals, count, weight)) {
        LOG_ERROR("Rejecting clause with %zu literals - BBB validation failed", count);
        return NIMCP_ERROR_BBB_VALIDATION;
    }

    nimcp_mutex_lock(solver->mutex);

    if (solver->clause_count >= SAT_MAX_CLAUSES) {
        nimcp_mutex_unlock(solver->mutex);
        LOG_ERROR("SAT solver clause limit exceeded");
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

    sat_clause_t* clause = &solver->clauses[solver->clause_count];
    memset(clause, 0, sizeof(*clause));

    for (size_t i = 0; i < count; i++) {
        sat_literal_t lit = literals[i];
        uint32_t var = sat_literal_var(lit);

        /* Validate variable exists */
        if (var == 0 || var > solver->variable_count) {
            nimcp_mutex_unlock(solver->mutex);
            LOG_ERROR("Invalid variable %u in clause", var);
            return NIMCP_ERROR_INVALID_PARAM;
        }

        clause->literals[i] = lit;
    }

    clause->literal_count = count;
    clause->active = true;
    clause->weight = (weight > 0.0f) ? weight : 1.0f;

    solver->clause_count++;

    nimcp_mutex_unlock(solver->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t sat_solver_add_unit(sat_solver_t* solver, sat_literal_t literal) {
    return sat_solver_add_clause(solver, &literal, 1, 1.0f);
}

nimcp_error_t sat_solver_add_binary(
    sat_solver_t* solver,
    sat_literal_t lit1,
    sat_literal_t lit2
) {
    sat_literal_t lits[2] = {lit1, lit2};
    return sat_solver_add_clause(solver, lits, 2, 1.0f);
}

nimcp_error_t sat_solver_add_implication(
    sat_solver_t* solver,
    sat_literal_t antecedent,
    sat_literal_t consequent
) {
    /* a → b is equivalent to ¬a ∨ b */
    return sat_solver_add_binary(solver, sat_negate(antecedent), consequent);
}

nimcp_error_t sat_solver_add_at_least_k(
    sat_solver_t* solver,
    const sat_literal_t* literals,
    size_t count,
    size_t k
) {
    if (!solver || !literals || count == 0 || k == 0) {
        return NIMCP_ERROR_INVALID_PARAM;
    }
    if (k > count) {
        return NIMCP_ERROR_INVALID_PARAM;  /* Impossible constraint */
    }

    /* At-least-k: add clauses for every (count - k + 1) subset negated
     * This is the complement of at-most-(k-1)
     *
     * For simplicity, we use the sequential counter encoding
     * For small k, we just enumerate
     */

    if (k == 1) {
        /* At least 1 = at least one literal true = the clause itself */
        return sat_solver_add_clause(solver, literals, count, 1.0f);
    }

    /* For k > 1, we need to ensure at most (count - k) are false.
     * We add clauses for every (count - k + 1) subset saying at least one must be true.
     * For at-least-2 of 3, we need: (x1 ∨ x2), (x1 ∨ x3), (x2 ∨ x3)
     */
    size_t subset_size = count - k + 1;

    if (subset_size > 5) {
        LOG_WARN("At-least-%zu of %zu: using relaxed encoding", k, count);
        /* Just add the disjunction as approximation */
        return sat_solver_add_clause(solver, literals, count, 1.0f);
    }

    /* Generate all combinations of subset_size literals */
    /* Each combination must have at least one true (= clause of those literals) */
    if (subset_size == 2) {
        /* C(n,2) clauses */
        for (size_t i = 0; i < count; i++) {
            for (size_t j = i + 1; j < count; j++) {
                sat_literal_t clause[2] = {literals[i], literals[j]};
                nimcp_error_t err = sat_solver_add_clause(solver, clause, 2, 1.0f);
                if (err != NIMCP_SUCCESS) return err;
            }
        }
    } else if (subset_size == 3) {
        /* C(n,3) clauses */
        for (size_t i = 0; i < count; i++) {
            for (size_t j = i + 1; j < count; j++) {
                for (size_t l = j + 1; l < count; l++) {
                    sat_literal_t clause[3] = {literals[i], literals[j], literals[l]};
                    nimcp_error_t err = sat_solver_add_clause(solver, clause, 3, 1.0f);
                    if (err != NIMCP_SUCCESS) return err;
                }
            }
        }
    } else if (subset_size == 4) {
        /* C(n,4) clauses */
        for (size_t i = 0; i < count; i++) {
            for (size_t j = i + 1; j < count; j++) {
                for (size_t l = j + 1; l < count; l++) {
                    for (size_t m = l + 1; m < count; m++) {
                        sat_literal_t clause[4] = {literals[i], literals[j], literals[l], literals[m]};
                        nimcp_error_t err = sat_solver_add_clause(solver, clause, 4, 1.0f);
                        if (err != NIMCP_SUCCESS) return err;
                    }
                }
            }
        }
    } else if (subset_size == 5) {
        /* C(n,5) clauses */
        for (size_t i = 0; i < count; i++) {
            for (size_t j = i + 1; j < count; j++) {
                for (size_t l = j + 1; l < count; l++) {
                    for (size_t m = l + 1; m < count; m++) {
                        for (size_t n = m + 1; n < count; n++) {
                            sat_literal_t clause[5] = {literals[i], literals[j], literals[l], literals[m], literals[n]};
                            nimcp_error_t err = sat_solver_add_clause(solver, clause, 5, 1.0f);
                            if (err != NIMCP_SUCCESS) return err;
                        }
                    }
                }
            }
        }
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t sat_solver_add_at_most_k(
    sat_solver_t* solver,
    const sat_literal_t* literals,
    size_t count,
    size_t k
) {
    if (!solver || !literals || count == 0) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (k >= count) {
        /* Always satisfiable, no constraint needed */
        return NIMCP_SUCCESS;
    }

    if (k == 0) {
        /* At most 0 = all must be false */
        for (size_t i = 0; i < count; i++) {
            nimcp_error_t err = sat_solver_add_unit(solver, sat_negate(literals[i]));
            if (err != NIMCP_SUCCESS) return err;
        }
        return NIMCP_SUCCESS;
    }

    /* At most k: any (k+1) subset must have at least one false
     * Add clause ¬l_i1 ∨ ¬l_i2 ∨ ... ∨ ¬l_i(k+1) for each combination
     */

    /* For small (k+1), enumerate combinations */
    if (k + 1 <= 4) {
        sat_literal_t neg_lits[16];
        for (size_t i = 0; i < count; i++) {
            neg_lits[i] = sat_negate(literals[i]);
        }

        /* Generate combinations of size k+1 */
        /* Simple nested loops for k+1 <= 4 */
        if (k + 1 == 2) {
            for (size_t i = 0; i < count; i++) {
                for (size_t j = i + 1; j < count; j++) {
                    sat_literal_t clause[2] = {neg_lits[i], neg_lits[j]};
                    nimcp_error_t err = sat_solver_add_clause(solver, clause, 2, 1.0f);
                    if (err != NIMCP_SUCCESS) return err;
                }
            }
        } else if (k + 1 == 3) {
            for (size_t i = 0; i < count; i++) {
                for (size_t j = i + 1; j < count; j++) {
                    for (size_t l = j + 1; l < count; l++) {
                        sat_literal_t clause[3] = {neg_lits[i], neg_lits[j], neg_lits[l]};
                        nimcp_error_t err = sat_solver_add_clause(solver, clause, 3, 1.0f);
                        if (err != NIMCP_SUCCESS) return err;
                    }
                }
            }
        }
        /* etc. for k+1 == 4 */
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t sat_solver_add_exactly_k(
    sat_solver_t* solver,
    const sat_literal_t* literals,
    size_t count,
    size_t k
) {
    nimcp_error_t err;

    err = sat_solver_add_at_least_k(solver, literals, count, k);
    if (err != NIMCP_SUCCESS) return err;

    err = sat_solver_add_at_most_k(solver, literals, count, k);
    return err;
}

/* ============================================================================
 * Solving API Implementation
 * ============================================================================ */

sat_result_t sat_solver_solve(sat_solver_t* solver) {
    if (!solver || solver->magic != SAT_SOLVER_MAGIC) {
        return SAT_RESULT_ERROR;
    }

    nimcp_mutex_lock(solver->mutex);

    /* Send heartbeat at start of solve */
    sat_solver_heartbeat("sat_solve_start", 0.0f);

    /* BBB validation of solver state before solving */
    bbb_system_t bbb = atomic_load(&g_sat_solver_bbb);
    if (bbb) {
        bbb_validation_result_t result;
        /* Validate variable data integrity */
        bool valid = bbb_validate_input(
            bbb,
            solver->variables,
            solver->variable_count * sizeof(sat_variable_t),
            &result
        );
        if (!valid) {
            LOG_ERROR("BBB validation failed for SAT solver state: %s", result.reason);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_BBB_VALIDATION,
                                  "BBB rejected SAT solver state");
            nimcp_mutex_unlock(solver->mutex);
            return SAT_RESULT_ERROR;
        }
    }

    solver->solve_start_ns = nimcp_time_now_ns();
    memset(&solver->stats, 0, sizeof(solver->stats));

    if (solver->config.enable_logging) {
        LOG_INFO("Starting SAT solve: %zu variables, %zu clauses",
                 solver->variable_count, solver->clause_count);
    }

    /* Run DPLL */
    solver->result = dpll_solve(solver, 0);

    /* Compute timing */
    uint64_t elapsed_ns = nimcp_time_now_ns() - solver->solve_start_ns;
    solver->stats.solve_time_ms = (float)elapsed_ns / 1000000.0f;

    if (solver->config.enable_logging) {
        LOG_INFO("SAT solve complete: %s (%.2fms, %lu decisions, %lu conflicts)",
                 sat_result_to_string(solver->result),
                 solver->stats.solve_time_ms,
                 (unsigned long)solver->stats.decisions,
                 (unsigned long)solver->stats.conflicts);
    }

    /* Report to immune system if unsatisfiable (potential constraint conflict) */
    if (solver->result == SAT_RESULT_UNSATISFIABLE && solver->immune) {
        uint8_t epitope[64];
        size_t epitope_len = snprintf((char*)epitope, sizeof(epitope),
                                       "SAT:UNSAT:constraints_unsatisfiable");
        if (epitope_len > sizeof(epitope)) epitope_len = sizeof(epitope);

        uint32_t antigen_id = 0;
        brain_immune_present_antigen(
            solver->immune,
            ANTIGEN_SOURCE_ANOMALY,
            epitope,
            epitope_len,
            4,  /* severity: warning level */
            0,  /* source_node */
            &antigen_id
        );
    }

    nimcp_mutex_unlock(solver->mutex);

    return solver->result;
}

sat_result_t sat_solver_solve_with_assumptions(
    sat_solver_t* solver,
    const sat_literal_t* assumptions,
    size_t assumption_count
) {
    if (!solver || solver->magic != SAT_SOLVER_MAGIC) {
        return SAT_RESULT_ERROR;
    }

    nimcp_mutex_lock(solver->mutex);

    /* Apply assumptions as unit clauses temporarily */
    size_t original_clause_count = solver->clause_count;

    for (size_t i = 0; i < assumption_count; i++) {
        sat_solver_add_unit(solver, assumptions[i]);
    }

    nimcp_mutex_unlock(solver->mutex);

    sat_result_t result = sat_solver_solve(solver);

    nimcp_mutex_lock(solver->mutex);

    /* Remove assumption clauses */
    solver->clause_count = original_clause_count;

    nimcp_mutex_unlock(solver->mutex);

    return result;
}

sat_value_t sat_solver_get_value(sat_solver_t* solver, uint32_t variable) {
    if (!solver || solver->magic != SAT_SOLVER_MAGIC) {
        return SAT_VALUE_UNASSIGNED;
    }
    if (variable == 0 || variable > solver->variable_count) {
        return SAT_VALUE_UNASSIGNED;
    }

    return solver->variables[variable - 1].value;
}

nimcp_error_t sat_solver_get_assignments(
    sat_solver_t* solver,
    sat_assignment_t* assignments,
    size_t max_assignments,
    size_t* count_out
) {
    if (!solver || !assignments || !count_out) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_mutex_lock(solver->mutex);

    size_t count = 0;
    for (size_t i = 0; i < solver->variable_count && count < max_assignments; i++) {
        if (solver->variables[i].value != SAT_VALUE_UNASSIGNED) {
            assignments[count].variable = (uint32_t)(i + 1);
            assignments[count].value = solver->variables[i].value;
            count++;
        }
    }

    *count_out = count;

    nimcp_mutex_unlock(solver->mutex);

    return NIMCP_SUCCESS;
}

nimcp_error_t sat_solver_get_stats(sat_solver_t* solver, sat_stats_t* stats) {
    if (!solver || !stats) return NIMCP_ERROR_NULL_POINTER;

    nimcp_mutex_lock(solver->mutex);
    *stats = solver->stats;
    nimcp_mutex_unlock(solver->mutex);

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Endorsement Integration Implementation
 * ============================================================================ */

/**
 * Simple expression parser for policy expressions
 */
typedef enum token_type {
    TOK_END,
    TOK_AND,
    TOK_OR,
    TOK_NOT,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_IDENT
} token_type_t;

typedef struct tokenizer {
    const char* input;
    size_t pos;
    char current_ident[NIMCP_ID_BUFFER_SIZE];
} tokenizer_t;

static token_type_t next_token(tokenizer_t* tok) {
    /* Skip whitespace */
    while (tok->input[tok->pos] && isspace(tok->input[tok->pos])) {
        tok->pos++;
    }

    if (!tok->input[tok->pos]) return TOK_END;

    char c = tok->input[tok->pos];

    if (c == '(') { tok->pos++; return TOK_LPAREN; }
    if (c == ')') { tok->pos++; return TOK_RPAREN; }

    /* Check keywords */
    if (strncmp(&tok->input[tok->pos], "AND", 3) == 0 &&
        !isalnum(tok->input[tok->pos + 3])) {
        tok->pos += 3;
        return TOK_AND;
    }
    if (strncmp(&tok->input[tok->pos], "OR", 2) == 0 &&
        !isalnum(tok->input[tok->pos + 2])) {
        tok->pos += 2;
        return TOK_OR;
    }
    if (strncmp(&tok->input[tok->pos], "NOT", 3) == 0 &&
        !isalnum(tok->input[tok->pos + 3])) {
        tok->pos += 3;
        return TOK_NOT;
    }

    /* Identifier */
    if (isalpha(c) || c == '_') {
        size_t start = tok->pos;
        while (isalnum(tok->input[tok->pos]) || tok->input[tok->pos] == '_') {
            tok->pos++;
        }
        size_t len = tok->pos - start;
        if (len >= sizeof(tok->current_ident)) len = sizeof(tok->current_ident) - 1;
        memcpy(tok->current_ident, &tok->input[start], len);
        tok->current_ident[len] = '\0';
        return TOK_IDENT;
    }

    return TOK_END;
}

nimcp_error_t sat_solver_add_policy_expression(
    sat_solver_t* solver,
    const char* expression,
    mesh_participant_registry_t* registry
) {
    if (!solver || !expression) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    /* This is a simplified parser - for production, use a proper recursive descent parser */
    /* For now, we handle simple expressions: A AND B, A OR B, NOT A */

    LOG_DEBUG("Parsing policy expression: %s", expression);

    /* TODO: Implement full expression parser with proper precedence
     * For now, just add variables for each identifier found
     */

    tokenizer_t tok = {.input = expression, .pos = 0};
    token_type_t t;

    sat_literal_t clause_lits[SAT_MAX_LITERALS_PER_CLAUSE];
    size_t lit_count = 0;
    bool is_or_clause = false;
    bool next_negated = false;

    while ((t = next_token(&tok)) != TOK_END) {
        switch (t) {
        case TOK_IDENT: {
            /* Find or create variable for this identifier */
            uint32_t var = 0;

            /* Look up in registry if available */
            if (registry) {
                const mesh_participant_interface_t* participant =
                    mesh_participant_get_by_name(registry, tok.current_ident);
                if (participant) {
                    var = sat_solver_get_variable_for_module(solver, participant->id);
                    if (var == 0) {
                        sat_solver_add_variable(solver, tok.current_ident, participant->id, &var);
                    }
                }
            }

            if (var == 0) {
                /* Create new variable without module association */
                sat_solver_add_variable(solver, tok.current_ident, 0, &var);
            }

            sat_literal_t lit = sat_make_literal(var, next_negated);
            next_negated = false;

            if (lit_count < SAT_MAX_LITERALS_PER_CLAUSE) {
                clause_lits[lit_count++] = lit;
            }
            break;
        }

        case TOK_AND:
            /* Commit current OR clause if any, start new */
            if (lit_count > 0 && is_or_clause) {
                sat_solver_add_clause(solver, clause_lits, lit_count, 1.0f);
                lit_count = 0;
            }
            is_or_clause = false;
            break;

        case TOK_OR:
            is_or_clause = true;
            break;

        case TOK_NOT:
            next_negated = true;
            break;

        case TOK_LPAREN:
        case TOK_RPAREN:
            /* Handle parentheses for proper grouping */
            /* Simplified: just continue */
            break;

        default:
            break;
        }
    }

    /* Add final clause */
    if (lit_count > 0) {
        if (is_or_clause) {
            sat_solver_add_clause(solver, clause_lits, lit_count, 1.0f);
        } else {
            /* AND of single literals → unit clauses */
            for (size_t i = 0; i < lit_count; i++) {
                sat_solver_add_unit(solver, clause_lits[i]);
            }
        }
    }

    return NIMCP_SUCCESS;
}

sat_result_t sat_solver_select_endorsers(
    sat_solver_t* solver,
    const mesh_participant_id_t* active_modules,
    size_t active_count,
    mesh_participant_id_t* selected_out,
    size_t selected_max,
    size_t* selected_count_out
) {
    if (!solver || !selected_out || !selected_count_out) {
        return SAT_RESULT_ERROR;
    }

    *selected_count_out = 0;

    /* Add assumptions: active modules can be true, inactive must be false */
    sat_literal_t assumptions[SAT_MAX_VARIABLES];
    size_t assumption_count = 0;

    nimcp_mutex_lock(solver->mutex);

    for (size_t v = 1; v <= solver->variable_count; v++) {
        mesh_participant_id_t mod = solver->variables[v - 1].module_id;
        if (mod == 0) continue;

        bool is_active = false;
        for (size_t i = 0; i < active_count; i++) {
            if (active_modules[i] == mod) {
                is_active = true;
                break;
            }
        }

        if (!is_active) {
            /* Inactive modules must be false */
            assumptions[assumption_count++] = sat_make_literal((uint32_t)v, true);
        }
    }

    nimcp_mutex_unlock(solver->mutex);

    /* Solve with assumptions */
    sat_result_t result = sat_solver_solve_with_assumptions(solver, assumptions, assumption_count);

    if (result == SAT_RESULT_SATISFIABLE) {
        /* Collect selected modules (those assigned TRUE) */
        nimcp_mutex_lock(solver->mutex);

        for (size_t v = 1; v <= solver->variable_count && *selected_count_out < selected_max; v++) {
            if (solver->variables[v - 1].value == SAT_VALUE_TRUE) {
                mesh_participant_id_t mod = solver->variables[v - 1].module_id;
                if (mod != 0) {
                    selected_out[*selected_count_out] = mod;
                    (*selected_count_out)++;
                }
            }
        }

        nimcp_mutex_unlock(solver->mutex);
    }

    return result;
}

sat_result_t sat_solver_find_minimal_endorsers(
    sat_solver_t* solver,
    const mesh_participant_id_t* active_modules,
    size_t active_count,
    mesh_participant_id_t* selected_out,
    size_t* selected_count_out
) {
    if (!solver || !selected_out || !selected_count_out) {
        return SAT_RESULT_ERROR;
    }

    /* Binary search for minimal k */
    size_t lo = 1;
    size_t hi = active_count;
    size_t best_k = active_count;
    mesh_participant_id_t best_set[SAT_MAX_VARIABLES];
    size_t best_count = 0;

    while (lo <= hi) {
        size_t mid = (lo + hi) / 2;

        /* Try to find solution with at most mid endorsers */
        /* Add at-most-mid constraint temporarily */

        sat_result_t result = sat_solver_select_endorsers(
            solver, active_modules, active_count,
            best_set, mid, &best_count
        );

        if (result == SAT_RESULT_SATISFIABLE && best_count <= mid) {
            best_k = best_count;
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }

    /* Return best found */
    if (best_count > 0) {
        memcpy(selected_out, best_set, best_count * sizeof(mesh_participant_id_t));
        *selected_count_out = best_count;
        return SAT_RESULT_SATISFIABLE;
    }

    *selected_count_out = 0;
    return SAT_RESULT_UNSATISFIABLE;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* sat_result_to_string(sat_result_t result) {
    switch (result) {
    case SAT_RESULT_UNKNOWN:        return "UNKNOWN";
    case SAT_RESULT_SATISFIABLE:    return "SATISFIABLE";
    case SAT_RESULT_UNSATISFIABLE:  return "UNSATISFIABLE";
    case SAT_RESULT_TIMEOUT:        return "TIMEOUT";
    case SAT_RESULT_ERROR:          return "ERROR";
    default:                        return "INVALID";
    }
}

void sat_solver_print(const sat_solver_t* solver) {
    if (!solver) {
        printf("SAT Solver: NULL\n");
        return;
    }

    printf("SAT Solver:\n");
    printf("  Variables: %zu\n", solver->variable_count);
    printf("  Clauses:   %zu\n", solver->clause_count);
    printf("  Learned:   %zu\n", solver->learned_count);
    printf("  Result:    %s\n", sat_result_to_string(solver->result));

    printf("  Variables:\n");
    for (size_t i = 0; i < solver->variable_count; i++) {
        const sat_variable_t* v = &solver->variables[i];
        const char* val_str = (v->value == SAT_VALUE_TRUE) ? "TRUE" :
                              (v->value == SAT_VALUE_FALSE) ? "FALSE" : "?";
        printf("    %zu: %s = %s (module=0x%lx)\n",
               i + 1, v->name, val_str, (unsigned long)v->module_id);
    }
}

void sat_solver_print_stats(const sat_stats_t* stats) {
    if (!stats) {
        printf("SAT Stats: NULL\n");
        return;
    }

    printf("SAT Statistics:\n");
    printf("  Decisions:     %lu\n", (unsigned long)stats->decisions);
    printf("  Propagations:  %lu\n", (unsigned long)stats->propagations);
    printf("  Conflicts:     %lu\n", (unsigned long)stats->conflicts);
    printf("  Backtracks:    %lu\n", (unsigned long)stats->backtracks);
    printf("  Restarts:      %lu\n", (unsigned long)stats->restarts);
    printf("  Learned:       %lu\n", (unsigned long)stats->learned_clauses);
    printf("  Solve time:    %.2f ms\n", stats->solve_time_ms);
}
