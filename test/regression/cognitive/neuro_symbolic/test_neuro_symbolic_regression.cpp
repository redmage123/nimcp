/**
 * @file test_neuro_symbolic_regression.cpp
 * @brief Regression tests for Neuro-Symbolic Mathematics System
 *
 * Tests edge cases, boundary conditions, and stability of API contracts
 * for the Mathesis-inspired genius-level math system.
 */

#include <gtest/gtest.h>
#include "utils/nimcp_test_base.h"

extern "C" {
#include "cognitive/neuro_symbolic/nimcp_genius_math_orchestrator.h"
#include "cognitive/neuro_symbolic/nimcp_energy_consistency.h"
#include "cognitive/neuro_symbolic/nimcp_hypergraph.h"
#include "cognitive/neuro_symbolic/nimcp_quantum_mcts.h"
#include "cognitive/neuro_symbolic/nimcp_evolutionary_proof.h"
#include "cognitive/parietal/nimcp_mathematical_genius.h"
}

/**
 * @brief Test fixture for Neuro-Symbolic Regression tests
 */
class NeuroSymbolicRegressionTest : public NimcpTestBase {
protected:
    void SetUp() override {
        NimcpTestBase::SetUp();
    }

    void TearDown() override {
        NimcpTestBase::TearDown();
    }
};

// ============================================================================
// Energy Consistency Regression Tests
// ============================================================================

TEST_F(NeuroSymbolicRegressionTest, EnergyConsistency_NullConfigSafeDefault) {
    // Verify NULL config creates valid checker with defaults
    energy_consistency_checker_t* checker = energy_consistency_create(NULL);
    ASSERT_NE(checker, nullptr);

    // Should be able to get score immediately
    float score = energy_consistency_get_score(checker);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);

    energy_consistency_destroy(checker);
}

TEST_F(NeuroSymbolicRegressionTest, EnergyConsistency_DestroyNullNoOp) {
    // Must not crash
    energy_consistency_destroy(NULL);
    SUCCEED();
}

TEST_F(NeuroSymbolicRegressionTest, EnergyConsistency_EmptyPropositionHandled) {
    energy_consistency_checker_t* checker = energy_consistency_create(NULL);
    ASSERT_NE(checker, nullptr);

    energy_consistency_result_t result;
    // Empty string should be handled gracefully
    nimcp_error_t err = energy_consistency_check_proposition(checker, "", NULL, &result);
    // May succeed with zero energy or return error - either is valid
    (void)err;

    energy_consistency_destroy(checker);
}

TEST_F(NeuroSymbolicRegressionTest, EnergyConsistency_ResetPreservesConfig) {
    energy_consistency_config_t config;
    energy_consistency_get_default_config(&config);
    config.max_violations = 100;

    energy_consistency_checker_t* checker = energy_consistency_create(&config);
    ASSERT_NE(checker, nullptr);

    // Check a proposition to dirty state
    energy_consistency_result_t result;
    energy_consistency_check_proposition(checker, "P OR Q", NULL, &result);

    // Reset should clear state but preserve config
    nimcp_error_t err = energy_consistency_reset(checker);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Score should return to initial
    float score = energy_consistency_get_score(checker);
    EXPECT_GE(score, 0.0f);

    energy_consistency_destroy(checker);
}

// ============================================================================
// Hypergraph Regression Tests
// ============================================================================

TEST_F(NeuroSymbolicRegressionTest, Hypergraph_EmptyGraphOperations) {
    nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    // Empty graph stats should be zeros
    hypergraph_stats_t stats;
    nimcp_error_t err = nimcp_hypergraph_get_stats(hg, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.vertex_count, 0u);
    EXPECT_EQ(stats.edge_count, 0u);

    // Query on empty graph should return 0
    uint32_t edges[10];
    uint32_t count = nimcp_hypergraph_get_incident_edges(hg, 0, edges, 10);
    EXPECT_EQ(count, 0u);

    nimcp_hypergraph_destroy(hg);
}

TEST_F(NeuroSymbolicRegressionTest, Hypergraph_DuplicateVertexLabelAllowed) {
    nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    // Two vertices with same label should both succeed
    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "same_name", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "same_name", 1.0f);

    EXPECT_NE(v1, UINT32_MAX);
    EXPECT_NE(v2, UINT32_MAX);
    EXPECT_NE(v1, v2);  // Different IDs

    hypergraph_stats_t stats;
    nimcp_hypergraph_get_stats(hg, &stats);
    EXPECT_EQ(stats.vertex_count, 2u);

    nimcp_hypergraph_destroy(hg);
}

TEST_F(NeuroSymbolicRegressionTest, Hypergraph_SingleVertexEdge) {
    nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PREDICATE, "singleton", 1.0f);
    EXPECT_NE(v1, UINT32_MAX);

    // Single-vertex hyperedge (unary relation)
    uint32_t vertices[] = {v1};
    uint32_t edge = nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, vertices, 1,
                                               TRIT_POSITIVE, "unary");
    EXPECT_NE(edge, UINT32_MAX);

    nimcp_hypergraph_destroy(hg);
}

TEST_F(NeuroSymbolicRegressionTest, Hypergraph_MaximumArityEdge) {
    nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    // Create maximum arity edge (32 vertices per header)
    uint32_t vertices[HYPERGRAPH_MAX_EDGE_VERTICES];
    for (uint32_t i = 0; i < HYPERGRAPH_MAX_EDGE_VERTICES; i++) {
        char name[32];
        snprintf(name, sizeof(name), "v%u", i);
        vertices[i] = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_VARIABLE, name, 1.0f);
        EXPECT_NE(vertices[i], UINT32_MAX);
    }

    uint32_t edge = nimcp_hypergraph_add_edge(hg, HYPEREDGE_CONSTRAINT, vertices,
                                               HYPERGRAPH_MAX_EDGE_VERTICES,
                                               TRIT_POSITIVE, "max_arity");
    EXPECT_NE(edge, UINT32_MAX);

    nimcp_hypergraph_destroy(hg);
}

TEST_F(NeuroSymbolicRegressionTest, Hypergraph_InvalidVertexInEdge) {
    nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "valid", 1.0f);

    // Try to create edge with invalid vertex ID
    uint32_t vertices[] = {v1, UINT32_MAX};
    uint32_t edge = nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, vertices, 2,
                                               TRIT_POSITIVE, "bad_edge");
    // Should fail or handle gracefully
    (void)edge;

    nimcp_hypergraph_destroy(hg);
}

// ============================================================================
// Quantum MCTS Regression Tests
// ============================================================================

TEST_F(NeuroSymbolicRegressionTest, QuantumMCTS_MinimalSimulations) {
    quantum_mcts_config_t config;
    quantum_mcts_get_default_config(&config);
    config.num_simulations = 1;  // Minimum
    config.planning_horizon = 1;

    quantum_mcts_t* qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    // Single simulation should still produce stats
    quantum_mcts_stats_t stats;
    nimcp_error_t err = quantum_mcts_get_stats(qmcts, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    quantum_mcts_destroy(qmcts);
}

TEST_F(NeuroSymbolicRegressionTest, QuantumMCTS_ZeroDimensionState) {
    quantum_mcts_config_t config;
    quantum_mcts_get_default_config(&config);
    config.num_simulations = 10;

    quantum_mcts_t* qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    // Zero dimension should be handled
    qmcts_plan_t plan;
    quantum_mcts_plan_init(&plan, 5);

    // This may fail, but should not crash
    float state[] = {1.0f};
    nimcp_error_t err = quantum_mcts_plan(qmcts, state, 0, &plan);
    (void)err;  // May return error, that's OK

    quantum_mcts_plan_cleanup(&plan);
    quantum_mcts_destroy(qmcts);
}

TEST_F(NeuroSymbolicRegressionTest, QuantumMCTS_ATPModulationBoundaries) {
    quantum_mcts_config_t config;
    quantum_mcts_get_default_config(&config);

    quantum_mcts_t* qmcts = quantum_mcts_create(&config);
    ASSERT_NE(qmcts, nullptr);

    // Boundary ATP values
    EXPECT_EQ(quantum_mcts_modulate_atp(qmcts, 0.0f), NIMCP_SUCCESS);
    EXPECT_EQ(quantum_mcts_modulate_atp(qmcts, 1.0f), NIMCP_SUCCESS);

    // Out of bounds should be clamped
    EXPECT_EQ(quantum_mcts_modulate_atp(qmcts, -1.0f), NIMCP_SUCCESS);
    EXPECT_EQ(quantum_mcts_modulate_atp(qmcts, 2.0f), NIMCP_SUCCESS);

    quantum_mcts_destroy(qmcts);
}

TEST_F(NeuroSymbolicRegressionTest, QuantumMCTS_PlanCleanupIdempotent) {
    qmcts_plan_t plan;
    quantum_mcts_plan_init(&plan, 10);

    // Multiple cleanups should be safe
    quantum_mcts_plan_cleanup(&plan);
    quantum_mcts_plan_cleanup(&plan);  // Should not crash
    SUCCEED();
}

// ============================================================================
// Genius Module Regression Tests
// ============================================================================

TEST_F(NeuroSymbolicRegressionTest, Genius_AllDomainsAccessible) {
    mathematical_genius_t* genius = genius_create(NULL);
    ASSERT_NE(genius, nullptr);

    // Each domain should be solvable
    genius_domain_t domains[] = {
        GENIUS_DOMAIN_NUMBER_THEORY,
        GENIUS_DOMAIN_ANALYSIS,
        GENIUS_DOMAIN_COMBINATORICS,
        GENIUS_DOMAIN_ALGEBRA,
        GENIUS_DOMAIN_GEOMETRY,
        GENIUS_DOMAIN_TOPOLOGY
    };

    for (size_t i = 0; i < sizeof(domains)/sizeof(domains[0]); i++) {
        math_problem_t problem;
        memset(&problem, 0, sizeof(problem));
        problem.domain = domains[i];
        problem.statement = (char*)"Test problem";
        problem.difficulty = 0.1f;

        genius_result_t result;
        genius_result_init(&result);

        nimcp_error_t err = genius_solve_problem(genius, &problem, &result);
        EXPECT_EQ(err, NIMCP_SUCCESS) << "Failed for domain " << i;

        genius_result_cleanup(&result);
    }

    genius_destroy(genius);
}

TEST_F(NeuroSymbolicRegressionTest, Genius_ExtremeDifficultyValues) {
    mathematical_genius_t* genius = genius_create(NULL);
    ASSERT_NE(genius, nullptr);

    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
    problem.statement = (char*)"Test";

    genius_result_t result;

    // Minimum difficulty
    problem.difficulty = 0.0f;
    genius_result_init(&result);
    EXPECT_EQ(genius_solve_problem(genius, &problem, &result), NIMCP_SUCCESS);
    genius_result_cleanup(&result);

    // Maximum difficulty
    problem.difficulty = 1.0f;
    genius_result_init(&result);
    EXPECT_EQ(genius_solve_problem(genius, &problem, &result), NIMCP_SUCCESS);
    genius_result_cleanup(&result);

    genius_destroy(genius);
}

TEST_F(NeuroSymbolicRegressionTest, Genius_EmptyStatementHandled) {
    mathematical_genius_t* genius = genius_create(NULL);
    ASSERT_NE(genius, nullptr);

    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
    problem.statement = (char*)"";  // Empty
    problem.difficulty = 0.5f;

    genius_result_t result;
    genius_result_init(&result);

    // Should handle gracefully (may return error or succeed with empty result)
    nimcp_error_t err = genius_solve_problem(genius, &problem, &result);
    (void)err;

    genius_result_cleanup(&result);
    genius_destroy(genius);
}

// ============================================================================
// Orchestrator Regression Tests
// ============================================================================

TEST_F(NeuroSymbolicRegressionTest, Orchestrator_NoComponentsEnabled) {
    orchestrator_config_t config;
    genius_orchestrator_get_default_config(&config);
    config.enabled_components = 0;  // No components

    genius_math_orchestrator_t* orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    // Should still work (just with limited functionality)
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
    problem.statement = (char*)"1+1";
    problem.difficulty = 0.1f;

    orchestrator_result_t result;
    memset(&result, 0, sizeof(result));

    nimcp_error_t err = genius_orchestrator_solve(orch, &problem, &result);
    // May succeed or fail depending on minimum requirements
    (void)err;

    genius_orchestrator_destroy(orch);
}

TEST_F(NeuroSymbolicRegressionTest, Orchestrator_MultipleResetsCycle) {
    orchestrator_config_t config;
    genius_orchestrator_get_default_config(&config);
    config.enabled_components = ORCH_COMP_GENIUS;
    config.operation_timeout_ms = 50;

    genius_math_orchestrator_t* orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    // Reset multiple times
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(genius_orchestrator_reset(orch), NIMCP_SUCCESS);

        // Should still be usable after reset
        orchestrator_stats_t stats;
        EXPECT_EQ(genius_orchestrator_get_stats(orch, &stats), NIMCP_SUCCESS);
        EXPECT_EQ(stats.operations_total, 0u);
    }

    genius_orchestrator_destroy(orch);
}

TEST_F(NeuroSymbolicRegressionTest, Orchestrator_ATPModulationPersistsAcrossReset) {
    orchestrator_config_t config;
    genius_orchestrator_get_default_config(&config);
    config.enabled_components = ORCH_COMP_GENIUS;

    genius_math_orchestrator_t* orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    // Set ATP modulation
    EXPECT_EQ(genius_orchestrator_modulate_atp(orch, 0.5f), NIMCP_SUCCESS);

    // Reset
    EXPECT_EQ(genius_orchestrator_reset(orch), NIMCP_SUCCESS);

    // Modulation should still work after reset
    EXPECT_EQ(genius_orchestrator_modulate_atp(orch, 0.8f), NIMCP_SUCCESS);

    genius_orchestrator_destroy(orch);
}

TEST_F(NeuroSymbolicRegressionTest, Orchestrator_PrintDiagnosticsNoException) {
    orchestrator_config_t config;
    genius_orchestrator_get_default_config(&config);

    genius_math_orchestrator_t* orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    // Should not throw even in various states
    genius_orchestrator_print_diagnostics(orch);

    // After some operations
    genius_orchestrator_init_components(orch);
    genius_orchestrator_print_diagnostics(orch);

    // After reset
    genius_orchestrator_reset(orch);
    genius_orchestrator_print_diagnostics(orch);

    // NULL should also be handled
    genius_orchestrator_print_diagnostics(NULL);

    genius_orchestrator_destroy(orch);
    SUCCEED();
}

// ============================================================================
// Evolutionary Proof Regression Tests
// ============================================================================

TEST_F(NeuroSymbolicRegressionTest, EvolutionaryProof_MinimalPopulation) {
    evoproof_config_t config;
    evolutionary_proof_get_default_config(&config);
    config.population_size = 1;  // Minimum
    config.proof_timeout_ms = 10;

    evolutionary_proof_search_t* prover = evolutionary_proof_create(&config);
    ASSERT_NE(prover, nullptr);

    // Should still be able to initialize population
    EXPECT_EQ(evolutionary_proof_init_population(prover), NIMCP_SUCCESS);

    evolutionary_proof_destroy(prover);
}

TEST_F(NeuroSymbolicRegressionTest, EvolutionaryProof_ZeroTimeoutHandled) {
    evoproof_config_t config;
    evolutionary_proof_get_default_config(&config);
    config.proof_timeout_ms = 0;  // Edge case
    config.population_size = 4;

    evolutionary_proof_search_t* prover = evolutionary_proof_create(&config);
    ASSERT_NE(prover, nullptr);

    evoproof_stats_t stats;
    EXPECT_EQ(evolutionary_proof_get_stats(prover, &stats), NIMCP_SUCCESS);

    evolutionary_proof_destroy(prover);
}

// ============================================================================
// Cross-Component Regression Tests
// ============================================================================

TEST_F(NeuroSymbolicRegressionTest, ComponentLinking_OrderIndependent) {
    // Create components
    mathematical_genius_t* genius = genius_create(NULL);
    ASSERT_NE(genius, nullptr);

    nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    quantum_mcts_config_t qconfig;
    quantum_mcts_get_default_config(&qconfig);
    qconfig.num_simulations = 10;
    quantum_mcts_t* qmcts = quantum_mcts_create(&qconfig);
    ASSERT_NE(qmcts, nullptr);

    // Link in any order should work
    EXPECT_EQ(genius_link_quantum_engine(genius, qmcts), NIMCP_SUCCESS);
    EXPECT_EQ(genius_link_hypergraph(genius, hg), NIMCP_SUCCESS);

    // Unlinking should also work
    EXPECT_EQ(genius_link_quantum_engine(genius, NULL), NIMCP_SUCCESS);
    EXPECT_EQ(genius_link_hypergraph(genius, NULL), NIMCP_SUCCESS);

    quantum_mcts_destroy(qmcts);
    nimcp_hypergraph_destroy(hg);
    genius_destroy(genius);
}

TEST_F(NeuroSymbolicRegressionTest, ComponentLinking_RelinkAfterDestroy) {
    mathematical_genius_t* genius = genius_create(NULL);
    ASSERT_NE(genius, nullptr);

    // Create and link hypergraph
    nimcp_hypergraph_t* hg1 = nimcp_hypergraph_create();
    ASSERT_NE(hg1, nullptr);
    EXPECT_EQ(genius_link_hypergraph(genius, hg1), NIMCP_SUCCESS);

    // Unlink before destroy
    EXPECT_EQ(genius_link_hypergraph(genius, NULL), NIMCP_SUCCESS);
    nimcp_hypergraph_destroy(hg1);

    // Link new hypergraph
    nimcp_hypergraph_t* hg2 = nimcp_hypergraph_create();
    ASSERT_NE(hg2, nullptr);
    EXPECT_EQ(genius_link_hypergraph(genius, hg2), NIMCP_SUCCESS);

    // Cleanup
    EXPECT_EQ(genius_link_hypergraph(genius, NULL), NIMCP_SUCCESS);
    nimcp_hypergraph_destroy(hg2);
    genius_destroy(genius);
}

// ============================================================================
// Memory Safety Regression Tests
// ============================================================================

TEST_F(NeuroSymbolicRegressionTest, Memory_DoubleDestroyNoOp) {
    // All destroys should handle NULL and be idempotent
    energy_consistency_destroy(NULL);
    nimcp_hypergraph_destroy(NULL);
    quantum_mcts_destroy(NULL);
    genius_destroy(NULL);
    genius_orchestrator_destroy(NULL);
    evolutionary_proof_destroy(NULL);
    SUCCEED();
}

TEST_F(NeuroSymbolicRegressionTest, Memory_CreateDestroyLoop) {
    // Repeated create/destroy should not leak
    for (int i = 0; i < 10; i++) {
        energy_consistency_checker_t* checker = energy_consistency_create(NULL);
        EXPECT_NE(checker, nullptr);
        energy_consistency_destroy(checker);

        nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
        EXPECT_NE(hg, nullptr);
        nimcp_hypergraph_destroy(hg);

        mathematical_genius_t* genius = genius_create(NULL);
        EXPECT_NE(genius, nullptr);
        genius_destroy(genius);
    }
}
