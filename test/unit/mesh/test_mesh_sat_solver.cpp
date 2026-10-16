/**
 * @file test_mesh_sat_solver.cpp
 * @brief Unit tests for DPLL-based SAT Solver
 *
 * Tests endorsement constraint satisfaction:
 * - Variable and clause management
 * - DPLL solving with unit propagation
 * - At-least-k, at-most-k, exactly-k constraints
 * - Policy expression parsing
 * - Endorser selection via SAT solving
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 */

#include <gtest/gtest.h>

extern "C" {
#include "mesh/nimcp_mesh_sat_solver.h"
#include "mesh/nimcp_mesh_participant.h"
#include "utils/error/nimcp_error_codes.h"
}

#include <cstring>

class MeshSATSolverTest : public ::testing::Test {
protected:
    sat_solver_t* solver = nullptr;
    mesh_participant_registry_t* registry = nullptr;

    void SetUp() override {
        solver = sat_solver_create(NULL);
        registry = mesh_registry_create(NULL);
    }

    void TearDown() override {
        if (solver) {
            sat_solver_destroy(solver);
            solver = nullptr;
        }
        if (registry) {
            mesh_registry_destroy(registry);
            registry = nullptr;
        }
    }

    /* Helper to add a module and get its variable ID */
    uint32_t add_module_variable(const char* name, mesh_participant_id_t id) {
        uint32_t var = 0;
        sat_solver_add_variable(solver, name, id, &var);
        return var;
    }

    /* Helper to register a participant */
    void register_participant(const char* name, mesh_participant_id_t id) {
        mesh_participant_interface_t iface;
        memset(&iface, 0, sizeof(iface));
        strncpy((char*)iface.module_name, name, sizeof(iface.module_name) - 1);
        iface.id = id;

        mesh_participant_config_t config;
        memset(&config, 0, sizeof(config));

        mesh_participant_id_t assigned_id = 0;
        mesh_participant_register(registry, &iface, &config, &assigned_id);
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(MeshSATSolverTest, CreateSolver) {
    ASSERT_NE(solver, nullptr);
}

TEST_F(MeshSATSolverTest, CreateSolverWithConfig) {
    sat_solver_config_t config;
    ASSERT_EQ(sat_solver_default_config(&config), NIMCP_SUCCESS);

    config.timeout_ms = 5000.0f;
    config.enable_learning = true;
    config.enable_vsids = true;

    sat_solver_t* custom = sat_solver_create(&config);
    ASSERT_NE(custom, nullptr);
    sat_solver_destroy(custom);
}

TEST_F(MeshSATSolverTest, DefaultConfig) {
    sat_solver_config_t config;
    ASSERT_EQ(sat_solver_default_config(&config), NIMCP_SUCCESS);

    EXPECT_GT(config.timeout_ms, 0.0f);
    EXPECT_TRUE(config.enable_learning);
    EXPECT_TRUE(config.enable_pure_literal);
}

TEST_F(MeshSATSolverTest, DefaultConfigNull) {
    EXPECT_EQ(sat_solver_default_config(NULL), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshSATSolverTest, DestroySolverNull) {
    sat_solver_destroy(NULL);  /* Should not crash */
}

TEST_F(MeshSATSolverTest, ResetSolver) {
    uint32_t var;
    ASSERT_EQ(sat_solver_add_variable(solver, "x", 0, &var), NIMCP_SUCCESS);

    ASSERT_EQ(sat_solver_reset(solver), NIMCP_SUCCESS);

    /* After reset, variables should be cleared */
    EXPECT_EQ(sat_solver_get_variable_for_module(solver, 1), 0u);
}

/* ============================================================================
 * Variable API Tests
 * ============================================================================ */

TEST_F(MeshSATSolverTest, AddVariable) {
    uint32_t var;
    ASSERT_EQ(sat_solver_add_variable(solver, "test_var", 0x1234, &var), NIMCP_SUCCESS);
    EXPECT_GT(var, 0u);  /* Variables are 1-based */
}

TEST_F(MeshSATSolverTest, AddMultipleVariables) {
    uint32_t v1, v2, v3;
    ASSERT_EQ(sat_solver_add_variable(solver, "var1", 1, &v1), NIMCP_SUCCESS);
    ASSERT_EQ(sat_solver_add_variable(solver, "var2", 2, &v2), NIMCP_SUCCESS);
    ASSERT_EQ(sat_solver_add_variable(solver, "var3", 3, &v3), NIMCP_SUCCESS);

    EXPECT_EQ(v1, 1u);
    EXPECT_EQ(v2, 2u);
    EXPECT_EQ(v3, 3u);
}

TEST_F(MeshSATSolverTest, AddVariableNull) {
    uint32_t var;
    EXPECT_EQ(sat_solver_add_variable(NULL, "x", 0, &var), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(sat_solver_add_variable(solver, NULL, 0, &var), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(sat_solver_add_variable(solver, "x", 0, NULL), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MeshSATSolverTest, GetVariableForModule) {
    uint32_t var;
    ASSERT_EQ(sat_solver_add_variable(solver, "motor", 0x100, &var), NIMCP_SUCCESS);

    EXPECT_EQ(sat_solver_get_variable_for_module(solver, 0x100), var);
    EXPECT_EQ(sat_solver_get_variable_for_module(solver, 0x999), 0u);  /* Not found */
}

TEST_F(MeshSATSolverTest, GetModuleForVariable) {
    uint32_t var;
    ASSERT_EQ(sat_solver_add_variable(solver, "motor", 0x100, &var), NIMCP_SUCCESS);

    EXPECT_EQ(sat_solver_get_module_for_variable(solver, var), 0x100ull);
    EXPECT_EQ(sat_solver_get_module_for_variable(solver, 999), 0ull);  /* Not found */
}

/* ============================================================================
 * Clause API Tests
 * ============================================================================ */

TEST_F(MeshSATSolverTest, AddUnitClause) {
    uint32_t var;
    ASSERT_EQ(sat_solver_add_variable(solver, "x", 0, &var), NIMCP_SUCCESS);

    /* Unit clause: x must be true */
    EXPECT_EQ(sat_solver_add_unit(solver, (sat_literal_t)var), NIMCP_SUCCESS);
}

TEST_F(MeshSATSolverTest, AddBinaryClause) {
    uint32_t v1, v2;
    ASSERT_EQ(sat_solver_add_variable(solver, "x", 0, &v1), NIMCP_SUCCESS);
    ASSERT_EQ(sat_solver_add_variable(solver, "y", 0, &v2), NIMCP_SUCCESS);

    /* Binary clause: x OR y */
    EXPECT_EQ(sat_solver_add_binary(solver, (sat_literal_t)v1, (sat_literal_t)v2), NIMCP_SUCCESS);
}

TEST_F(MeshSATSolverTest, AddImplication) {
    uint32_t v1, v2;
    ASSERT_EQ(sat_solver_add_variable(solver, "a", 0, &v1), NIMCP_SUCCESS);
    ASSERT_EQ(sat_solver_add_variable(solver, "b", 0, &v2), NIMCP_SUCCESS);

    /* Implication: a -> b (equivalent to NOT a OR b) */
    EXPECT_EQ(sat_solver_add_implication(solver, (sat_literal_t)v1, (sat_literal_t)v2), NIMCP_SUCCESS);
}

TEST_F(MeshSATSolverTest, AddClause) {
    uint32_t vars[3];
    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof(name), "x%d", i);
        ASSERT_EQ(sat_solver_add_variable(solver, name, 0, &vars[i]), NIMCP_SUCCESS);
    }

    /* Clause: x0 OR x1 OR NOT x2 */
    sat_literal_t lits[3] = {
        (sat_literal_t)vars[0],
        (sat_literal_t)vars[1],
        sat_negate((sat_literal_t)vars[2])
    };

    EXPECT_EQ(sat_solver_add_clause(solver, lits, 3, 1.0f), NIMCP_SUCCESS);
}

TEST_F(MeshSATSolverTest, AddClauseNull) {
    sat_literal_t lits[2] = {1, 2};
    EXPECT_EQ(sat_solver_add_clause(NULL, lits, 2, 1.0f), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(sat_solver_add_clause(solver, NULL, 2, 1.0f), NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Cardinality Constraint Tests
 * ============================================================================ */

TEST_F(MeshSATSolverTest, AddAtLeastK) {
    uint32_t vars[4];
    sat_literal_t lits[4];

    for (int i = 0; i < 4; i++) {
        char name[32];
        snprintf(name, sizeof(name), "x%d", i);
        ASSERT_EQ(sat_solver_add_variable(solver, name, 0, &vars[i]), NIMCP_SUCCESS);
        lits[i] = (sat_literal_t)vars[i];
    }

    /* At least 2 of x0, x1, x2, x3 must be true */
    EXPECT_EQ(sat_solver_add_at_least_k(solver, lits, 4, 2), NIMCP_SUCCESS);
}

TEST_F(MeshSATSolverTest, AddAtMostK) {
    uint32_t vars[4];
    sat_literal_t lits[4];

    for (int i = 0; i < 4; i++) {
        char name[32];
        snprintf(name, sizeof(name), "x%d", i);
        ASSERT_EQ(sat_solver_add_variable(solver, name, 0, &vars[i]), NIMCP_SUCCESS);
        lits[i] = (sat_literal_t)vars[i];
    }

    /* At most 2 of x0, x1, x2, x3 can be true */
    EXPECT_EQ(sat_solver_add_at_most_k(solver, lits, 4, 2), NIMCP_SUCCESS);
}

TEST_F(MeshSATSolverTest, AddExactlyK) {
    uint32_t vars[3];
    sat_literal_t lits[3];

    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof(name), "x%d", i);
        ASSERT_EQ(sat_solver_add_variable(solver, name, 0, &vars[i]), NIMCP_SUCCESS);
        lits[i] = (sat_literal_t)vars[i];
    }

    /* Exactly 1 of x0, x1, x2 must be true */
    EXPECT_EQ(sat_solver_add_exactly_k(solver, lits, 3, 1), NIMCP_SUCCESS);
}

/* ============================================================================
 * Solving Tests
 * ============================================================================ */

TEST_F(MeshSATSolverTest, SolveSimpleSatisfiable) {
    uint32_t var;
    ASSERT_EQ(sat_solver_add_variable(solver, "x", 0, &var), NIMCP_SUCCESS);

    /* Just require x = true */
    ASSERT_EQ(sat_solver_add_unit(solver, (sat_literal_t)var), NIMCP_SUCCESS);

    sat_result_t result = sat_solver_solve(solver);
    EXPECT_EQ(result, SAT_RESULT_SATISFIABLE);

    /* x should be true */
    EXPECT_EQ(sat_solver_get_value(solver, var), SAT_VALUE_TRUE);
}

TEST_F(MeshSATSolverTest, SolveSimpleUnsatisfiable) {
    uint32_t var;
    ASSERT_EQ(sat_solver_add_variable(solver, "x", 0, &var), NIMCP_SUCCESS);

    /* x AND NOT x */
    ASSERT_EQ(sat_solver_add_unit(solver, (sat_literal_t)var), NIMCP_SUCCESS);
    ASSERT_EQ(sat_solver_add_unit(solver, sat_negate((sat_literal_t)var)), NIMCP_SUCCESS);

    sat_result_t result = sat_solver_solve(solver);
    EXPECT_EQ(result, SAT_RESULT_UNSATISFIABLE);
}

TEST_F(MeshSATSolverTest, SolveWithImplication) {
    uint32_t v1, v2;
    ASSERT_EQ(sat_solver_add_variable(solver, "a", 0, &v1), NIMCP_SUCCESS);
    ASSERT_EQ(sat_solver_add_variable(solver, "b", 0, &v2), NIMCP_SUCCESS);

    /* a = true, a -> b, therefore b must be true */
    ASSERT_EQ(sat_solver_add_unit(solver, (sat_literal_t)v1), NIMCP_SUCCESS);
    ASSERT_EQ(sat_solver_add_implication(solver, (sat_literal_t)v1, (sat_literal_t)v2), NIMCP_SUCCESS);

    sat_result_t result = sat_solver_solve(solver);
    EXPECT_EQ(result, SAT_RESULT_SATISFIABLE);
    EXPECT_EQ(sat_solver_get_value(solver, v1), SAT_VALUE_TRUE);
    EXPECT_EQ(sat_solver_get_value(solver, v2), SAT_VALUE_TRUE);
}

TEST_F(MeshSATSolverTest, SolveWithAssumptions) {
    uint32_t v1, v2;
    ASSERT_EQ(sat_solver_add_variable(solver, "a", 0, &v1), NIMCP_SUCCESS);
    ASSERT_EQ(sat_solver_add_variable(solver, "b", 0, &v2), NIMCP_SUCCESS);

    /* a OR b */
    ASSERT_EQ(sat_solver_add_binary(solver, (sat_literal_t)v1, (sat_literal_t)v2), NIMCP_SUCCESS);

    /* Assume NOT a, then b must be true */
    sat_literal_t assumptions[1] = { sat_negate((sat_literal_t)v1) };
    sat_result_t result = sat_solver_solve_with_assumptions(solver, assumptions, 1);

    EXPECT_EQ(result, SAT_RESULT_SATISFIABLE);
    EXPECT_EQ(sat_solver_get_value(solver, v2), SAT_VALUE_TRUE);
}

TEST_F(MeshSATSolverTest, SolveAtLeastK) {
    uint32_t vars[3];
    sat_literal_t lits[3];

    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof(name), "x%d", i);
        ASSERT_EQ(sat_solver_add_variable(solver, name, 0, &vars[i]), NIMCP_SUCCESS);
        lits[i] = (sat_literal_t)vars[i];
    }

    /* At least 2 must be true */
    ASSERT_EQ(sat_solver_add_at_least_k(solver, lits, 3, 2), NIMCP_SUCCESS);

    sat_result_t result = sat_solver_solve(solver);
    EXPECT_EQ(result, SAT_RESULT_SATISFIABLE);

    /* Count true values */
    int true_count = 0;
    for (int i = 0; i < 3; i++) {
        if (sat_solver_get_value(solver, vars[i]) == SAT_VALUE_TRUE) {
            true_count++;
        }
    }
    EXPECT_GE(true_count, 2);
}

TEST_F(MeshSATSolverTest, SolveExactlyK) {
    uint32_t vars[3];
    sat_literal_t lits[3];

    for (int i = 0; i < 3; i++) {
        char name[32];
        snprintf(name, sizeof(name), "x%d", i);
        ASSERT_EQ(sat_solver_add_variable(solver, name, 0, &vars[i]), NIMCP_SUCCESS);
        lits[i] = (sat_literal_t)vars[i];
    }

    /* Exactly 1 must be true */
    ASSERT_EQ(sat_solver_add_exactly_k(solver, lits, 3, 1), NIMCP_SUCCESS);

    sat_result_t result = sat_solver_solve(solver);
    EXPECT_EQ(result, SAT_RESULT_SATISFIABLE);

    /* Count true values */
    int true_count = 0;
    for (int i = 0; i < 3; i++) {
        if (sat_solver_get_value(solver, vars[i]) == SAT_VALUE_TRUE) {
            true_count++;
        }
    }
    EXPECT_EQ(true_count, 1);
}

/* ============================================================================
 * Policy Expression Tests
 * ============================================================================ */

TEST_F(MeshSATSolverTest, AddPolicyExpressionSimple) {
    /* Register participants using helper */
    register_participant("motor_cortex", 0x100);
    register_participant("cerebellum", 0x200);

    /* Policy: motor_cortex AND cerebellum */
    EXPECT_EQ(sat_solver_add_policy_expression(solver,
        "motor_cortex AND cerebellum", registry), NIMCP_SUCCESS);
}

TEST_F(MeshSATSolverTest, AddPolicyExpressionWithOR) {
    register_participant("motor", 0x100);
    register_participant("visual", 0x200);
    register_participant("auditory", 0x300);

    /* Policy: motor AND (visual OR auditory) */
    EXPECT_EQ(sat_solver_add_policy_expression(solver,
        "motor AND (visual OR auditory)", registry), NIMCP_SUCCESS);
}

TEST_F(MeshSATSolverTest, AddPolicyExpressionWithNOT) {
    register_participant("action", 0x100);
    register_participant("veto", 0x200);

    /* Policy: action AND NOT veto */
    EXPECT_EQ(sat_solver_add_policy_expression(solver,
        "action AND NOT veto", registry), NIMCP_SUCCESS);
}

TEST_F(MeshSATSolverTest, AddPolicyExpressionNull) {
    EXPECT_EQ(sat_solver_add_policy_expression(NULL, "x", registry),
              NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(sat_solver_add_policy_expression(solver, NULL, registry),
              NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Endorser Selection Tests
 * ============================================================================ */

TEST_F(MeshSATSolverTest, SelectEndorsers) {
    /* Add variables for modules */
    uint32_t v1 = add_module_variable("motor", 0x100);
    uint32_t v2 = add_module_variable("cerebellum", 0x200);
    uint32_t v3 = add_module_variable("veto", 0x300);

    /* Policy: motor AND cerebellum AND NOT veto */
    ASSERT_EQ(sat_solver_add_unit(solver, (sat_literal_t)v1), NIMCP_SUCCESS);
    ASSERT_EQ(sat_solver_add_unit(solver, (sat_literal_t)v2), NIMCP_SUCCESS);
    ASSERT_EQ(sat_solver_add_unit(solver, sat_negate((sat_literal_t)v3)), NIMCP_SUCCESS);

    mesh_participant_id_t active[] = {0x100, 0x200, 0x300};
    mesh_participant_id_t selected[8];
    size_t selected_count = 0;

    sat_result_t result = sat_solver_select_endorsers(
        solver, active, 3, selected, 8, &selected_count);

    EXPECT_EQ(result, SAT_RESULT_SATISFIABLE);
    EXPECT_GE(selected_count, 2u);  /* motor and cerebellum */

    /* Verify veto is NOT in selected */
    bool veto_selected = false;
    for (size_t i = 0; i < selected_count; i++) {
        if (selected[i] == 0x300) {
            veto_selected = true;
        }
    }
    EXPECT_FALSE(veto_selected);
}

TEST_F(MeshSATSolverTest, SelectEndorsersNull) {
    mesh_participant_id_t active[] = {1, 2};
    mesh_participant_id_t selected[8];
    size_t count = 0;

    EXPECT_EQ(sat_solver_select_endorsers(NULL, active, 2, selected, 8, &count),
              SAT_RESULT_ERROR);
}

TEST_F(MeshSATSolverTest, FindMinimalEndorsers) {
    /* Add variables for modules */
    uint32_t v1 = add_module_variable("a", 0x100);
    uint32_t v2 = add_module_variable("b", 0x200);
    uint32_t v3 = add_module_variable("c", 0x300);

    /* Policy: a OR b (either is sufficient) */
    sat_literal_t lits[2] = {(sat_literal_t)v1, (sat_literal_t)v2};
    ASSERT_EQ(sat_solver_add_clause(solver, lits, 2, 1.0f), NIMCP_SUCCESS);

    mesh_participant_id_t active[] = {0x100, 0x200, 0x300};
    mesh_participant_id_t selected[8];
    size_t selected_count = 0;

    sat_result_t result = sat_solver_find_minimal_endorsers(
        solver, active, 3, selected, &selected_count);

    EXPECT_EQ(result, SAT_RESULT_SATISFIABLE);
    /* Minimal set should have just 1 endorser (either a or b) */
    EXPECT_EQ(selected_count, 1u);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MeshSATSolverTest, GetStats) {
    uint32_t v1, v2;
    ASSERT_EQ(sat_solver_add_variable(solver, "a", 0, &v1), NIMCP_SUCCESS);
    ASSERT_EQ(sat_solver_add_variable(solver, "b", 0, &v2), NIMCP_SUCCESS);

    ASSERT_EQ(sat_solver_add_binary(solver, (sat_literal_t)v1, (sat_literal_t)v2), NIMCP_SUCCESS);
    sat_solver_solve(solver);

    sat_stats_t stats;
    ASSERT_EQ(sat_solver_get_stats(solver, &stats), NIMCP_SUCCESS);

    EXPECT_GE(stats.decisions, 0u);
    EXPECT_GE(stats.propagations, 0u);
}

TEST_F(MeshSATSolverTest, GetStatsNull) {
    sat_stats_t stats;
    EXPECT_EQ(sat_solver_get_stats(NULL, &stats), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(sat_solver_get_stats(solver, NULL), NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(MeshSATSolverTest, MakeLiteral) {
    sat_literal_t pos = sat_make_literal(5, false);
    sat_literal_t neg = sat_make_literal(5, true);

    EXPECT_EQ(pos, 5);
    EXPECT_EQ(neg, -5);
}

TEST_F(MeshSATSolverTest, LiteralVar) {
    EXPECT_EQ(sat_literal_var(5), 5u);
    EXPECT_EQ(sat_literal_var(-5), 5u);
}

TEST_F(MeshSATSolverTest, LiteralNegated) {
    EXPECT_FALSE(sat_literal_negated(5));
    EXPECT_TRUE(sat_literal_negated(-5));
}

TEST_F(MeshSATSolverTest, NegateLiteral) {
    EXPECT_EQ(sat_negate(5), -5);
    EXPECT_EQ(sat_negate(-5), 5);
}

TEST_F(MeshSATSolverTest, ResultToString) {
    EXPECT_STREQ(sat_result_to_string(SAT_RESULT_SATISFIABLE), "SATISFIABLE");
    EXPECT_STREQ(sat_result_to_string(SAT_RESULT_UNSATISFIABLE), "UNSATISFIABLE");
    EXPECT_STREQ(sat_result_to_string(SAT_RESULT_UNKNOWN), "UNKNOWN");
    EXPECT_STREQ(sat_result_to_string(SAT_RESULT_TIMEOUT), "TIMEOUT");
    EXPECT_STREQ(sat_result_to_string(SAT_RESULT_ERROR), "ERROR");
}

/* ============================================================================
 * Print Function Tests (for coverage)
 * ============================================================================ */

TEST_F(MeshSATSolverTest, PrintSolverNull) {
    sat_solver_print(NULL);  /* Should not crash */
}

TEST_F(MeshSATSolverTest, PrintSolver) {
    uint32_t var;
    sat_solver_add_variable(solver, "test", 0, &var);
    sat_solver_add_unit(solver, (sat_literal_t)var);
    sat_solver_solve(solver);

    sat_solver_print(solver);  /* Should not crash */
}

TEST_F(MeshSATSolverTest, PrintStats) {
    sat_stats_t stats = {0};
    stats.decisions = 10;
    stats.propagations = 50;
    stats.conflicts = 5;

    sat_solver_print_stats(&stats);  /* Should not crash */
}
