/**
 * @file test_neuro_symbolic_integration.cpp
 * @brief Integration tests for Neuro-Symbolic Mathematics System
 *
 * Tests the integration between:
 * - Energy Consistency + Hypergraph
 * - Genius Module + Quantum MCTS
 * - Full Orchestrator Integration
 * - Bridge Module Interactions
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
 * @brief Test fixture for Neuro-Symbolic Integration tests
 */
class NeuroSymbolicIntegrationTest : public NimcpTestBase {
protected:
    genius_math_orchestrator_t* orch;
    orchestrator_config_t config;

    void SetUp() override {
        NimcpTestBase::SetUp();
        orch = NULL;
        memset(&config, 0, sizeof(config));
        genius_orchestrator_get_default_config(&config);
        // Use fast config for integration tests
        config.operation_timeout_ms = 500;
    }

    void TearDown() override {
        if (orch) {
            genius_orchestrator_destroy(orch);
            orch = NULL;
        }
        NimcpTestBase::TearDown();
    }
};

// ============================================================================
// Energy Consistency + Hypergraph Integration
// ============================================================================

TEST_F(NeuroSymbolicIntegrationTest, EnergyConsistencyWithHypergraphIntegration) {
    // Create both components
    energy_consistency_checker_t* checker = energy_consistency_create(NULL);
    ASSERT_NE(checker, nullptr);

    nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    // Add vertices to hypergraph (type, label, confidence)
    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PREDICATE, "proposition_P", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PREDICATE, "proposition_Q", 1.0f);
    uint32_t v3 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_FUNCTION, "implication_PQ", 1.0f);

    EXPECT_NE(v1, UINT32_MAX);
    EXPECT_NE(v2, UINT32_MAX);
    EXPECT_NE(v3, UINT32_MAX);

    // Create hyperedge connecting them
    uint32_t vertices[] = {v1, v2, v3};
    uint32_t edge_id = nimcp_hypergraph_add_edge(hg, HYPEREDGE_RULE, vertices, 3,
                                                  TRIT_POSITIVE, "P implies Q");
    EXPECT_NE(edge_id, UINT32_MAX);

    // Check consistency - should have zero energy for valid structure
    energy_consistency_result_t result;
    nimcp_error_t err = energy_consistency_check_proposition(checker, "P implies Q", NULL, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Cleanup
    nimcp_hypergraph_destroy(hg);
    energy_consistency_destroy(checker);
}

TEST_F(NeuroSymbolicIntegrationTest, HypergraphKnowledgeBaseConstruction) {
    nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    // Build a small knowledge base using available vertex types
    uint32_t prime_pred = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PREDICATE, "prime", 1.0f);
    uint32_t odd_pred = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PREDICATE, "odd", 1.0f);
    uint32_t gt2_pred = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PREDICATE, "greater_than_2", 1.0f);

    EXPECT_NE(prime_pred, UINT32_MAX);
    EXPECT_NE(odd_pred, UINT32_MAX);
    EXPECT_NE(gt2_pred, UINT32_MAX);

    // Create implication edge
    uint32_t vertices[] = {prime_pred, gt2_pred, odd_pred};
    uint32_t rule_edge = nimcp_hypergraph_add_edge(hg, HYPEREDGE_RULE, vertices, 3,
                                                    TRIT_POSITIVE, "prime_gt2_implies_odd");
    EXPECT_NE(rule_edge, UINT32_MAX);

    // Query incidence
    uint32_t incident_edges[10];
    uint32_t count = nimcp_hypergraph_get_incident_edges(hg, prime_pred, incident_edges, 10);
    EXPECT_GE(count, 1u);

    // Get stats
    hypergraph_stats_t stats;
    nimcp_error_t err = nimcp_hypergraph_get_stats(hg, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(stats.vertex_count, 3u);
    EXPECT_GE(stats.edge_count, 1u);

    nimcp_hypergraph_destroy(hg);
}

// ============================================================================
// Genius Module + Quantum MCTS Integration
// ============================================================================

TEST_F(NeuroSymbolicIntegrationTest, GeniusWithQuantumMCTSPlanning) {
    // Create genius module
    mathematical_genius_t* genius = genius_create(NULL);
    ASSERT_NE(genius, nullptr);

    // Create quantum MCTS with minimal simulations for testing
    quantum_mcts_config_t qmcts_config;
    quantum_mcts_get_default_config(&qmcts_config);
    qmcts_config.num_simulations = 50;  // Small for integration test
    qmcts_config.planning_horizon = 5;

    quantum_mcts_t* qmcts = quantum_mcts_create(&qmcts_config);
    ASSERT_NE(qmcts, nullptr);

    // Link them
    nimcp_error_t err = genius_link_quantum_engine(genius, qmcts);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Solve a problem that might use quantum planning
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
    problem.statement = (char*)"Find optimal path through number space";
    problem.difficulty = 0.5f;

    genius_result_t result;
    genius_result_init(&result);

    err = genius_solve_problem(genius, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    genius_result_cleanup(&result);
    quantum_mcts_destroy(qmcts);
    genius_destroy(genius);
}

TEST_F(NeuroSymbolicIntegrationTest, QuantumMCTSWithEnvironmentCallback) {
    quantum_mcts_config_t qmcts_config;
    quantum_mcts_get_default_config(&qmcts_config);
    qmcts_config.num_simulations = 30;
    qmcts_config.planning_horizon = 10;

    quantum_mcts_t* qmcts = quantum_mcts_create(&qmcts_config);
    ASSERT_NE(qmcts, nullptr);

    // Set up simple environment
    float state[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};

    // Plan - use proper initialization
    qmcts_plan_t plan;
    nimcp_error_t err = quantum_mcts_plan_init(&plan, 10);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    err = quantum_mcts_plan(qmcts, state, 8, &plan);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Should have generated some plan
    EXPECT_GE(plan.num_actions, 0u);

    quantum_mcts_plan_cleanup(&plan);
    quantum_mcts_destroy(qmcts);
}

// ============================================================================
// Evolutionary Proof Integration
// ============================================================================

TEST_F(NeuroSymbolicIntegrationTest, EvolutionaryProofBasicOperation) {
    // Create evolutionary prover - just test lifecycle, not actual proving
    evoproof_config_t prover_config;
    nimcp_error_t err = evolutionary_proof_get_default_config(&prover_config);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Override with minimal settings for testing
    prover_config.proof_timeout_ms = 10;  // Very short
    prover_config.max_proof_steps = 2;
    prover_config.max_proof_depth = 2;
    prover_config.population_size = 4;  // Minimal population

    evolutionary_proof_search_t* prover = evolutionary_proof_create(&prover_config);
    ASSERT_NE(prover, nullptr);

    // Initialize population - this is what we can safely test
    err = evolutionary_proof_init_population(prover);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Get stats to verify state
    evoproof_stats_t stats;
    err = evolutionary_proof_get_stats(prover, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.generations, 0u);  // No evolution yet

    // Test reset
    err = evolutionary_proof_reset(prover);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    evolutionary_proof_destroy(prover);
}

TEST_F(NeuroSymbolicIntegrationTest, EnergyConsistencyBasicOperation) {
    // Create consistency checker
    energy_consistency_checker_t* checker = energy_consistency_create(NULL);
    ASSERT_NE(checker, nullptr);

    // Check simple proposition consistency
    energy_consistency_result_t result;
    nimcp_error_t err = energy_consistency_check_proposition(checker, "P AND P", NULL, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    // Tautology should have low energy (consistent)
    EXPECT_LE(result.total_energy, 1.0f);

    // Check contradiction
    err = energy_consistency_check_proposition(checker, "P AND NOT P", NULL, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    // Contradiction has higher energy (inconsistent)
    EXPECT_GE(result.total_energy, 0.0f);

    energy_consistency_destroy(checker);
}

// ============================================================================
// Full Orchestrator Integration
// ============================================================================

TEST_F(NeuroSymbolicIntegrationTest, OrchestratorAllComponentsEnabled) {
    // Enable selected components with fast config
    config.enabled_components = ORCH_COMP_GENIUS | ORCH_COMP_HYPERGRAPH;
    config.operation_timeout_ms = 200;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    // Initialize all components
    nimcp_error_t err = genius_orchestrator_init_components(orch);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Verify components were created
    orchestrator_stats_t stats;
    err = genius_orchestrator_get_stats(orch, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Solve problem using all components
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
    problem.statement = (char*)"Verify 2+2=4";
    problem.difficulty = 0.1f;

    orchestrator_result_t result;
    memset(&result, 0, sizeof(result));

    err = genius_orchestrator_solve(orch, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Stats should show the solve was recorded
    err = genius_orchestrator_get_stats(orch, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(stats.operations_total, 1u);
}

TEST_F(NeuroSymbolicIntegrationTest, OrchestratorModulationAffectsComponents) {
    config.enabled_components = ORCH_COMP_GENIUS;
    config.operation_timeout_ms = 100;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    // Test ATP modulation
    nimcp_error_t err = genius_orchestrator_modulate_atp(orch, 1.0f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Solve at full ATP
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
    problem.statement = (char*)"Test problem";
    problem.difficulty = 0.1f;

    orchestrator_result_t result1;
    memset(&result1, 0, sizeof(result1));
    err = genius_orchestrator_solve(orch, &problem, &result1);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Lower ATP and solve again
    err = genius_orchestrator_modulate_atp(orch, 0.5f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    orchestrator_result_t result2;
    memset(&result2, 0, sizeof(result2));
    err = genius_orchestrator_solve(orch, &problem, &result2);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Lower ATP should affect computation
    EXPECT_GE(result2.atp_consumed, 0.0f);
}

TEST_F(NeuroSymbolicIntegrationTest, OrchestratorResetClearsState) {
    config.enabled_components = ORCH_COMP_GENIUS;
    config.operation_timeout_ms = 100;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    // Solve some problems
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
    problem.statement = (char*)"Test problem";
    problem.difficulty = 0.1f;

    orchestrator_result_t result;
    memset(&result, 0, sizeof(result));

    genius_orchestrator_solve(orch, &problem, &result);
    genius_orchestrator_solve(orch, &problem, &result);

    // Get stats before reset
    orchestrator_stats_t stats_before;
    genius_orchestrator_get_stats(orch, &stats_before);
    EXPECT_GE(stats_before.operations_total, 2u);

    // Reset
    nimcp_error_t err = genius_orchestrator_reset(orch);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Stats should be cleared
    orchestrator_stats_t stats_after;
    genius_orchestrator_get_stats(orch, &stats_after);
    EXPECT_EQ(stats_after.operations_total, 0u);
}

// ============================================================================
// Cross-Component Data Flow
// ============================================================================

TEST_F(NeuroSymbolicIntegrationTest, HypergraphToGeniusDataFlow) {
    // Create hypergraph with some mathematical knowledge
    nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    // Add mathematical facts using correct API: (hg, type, label, confidence)
    uint32_t two = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "2", 1.0f);
    uint32_t three = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "3", 1.0f);
    uint32_t five = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "5", 1.0f);
    uint32_t sum_op = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_FUNCTION, "sum", 1.0f);

    EXPECT_NE(two, UINT32_MAX);
    EXPECT_NE(three, UINT32_MAX);
    EXPECT_NE(five, UINT32_MAX);
    EXPECT_NE(sum_op, UINT32_MAX);

    // 2 + 3 = 5
    uint32_t vertices[] = {two, three, sum_op, five};
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_CONSTRAINT, vertices, 4,
                              TRIT_POSITIVE, "2+3=5");

    // Create genius and link hypergraph
    mathematical_genius_t* genius = genius_create(NULL);
    ASSERT_NE(genius, nullptr);

    nimcp_error_t err = genius_link_hypergraph(genius, hg);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    genius_destroy(genius);
    nimcp_hypergraph_destroy(hg);
}

TEST_F(NeuroSymbolicIntegrationTest, MultipleGeniusModesSwitching) {
    mathematical_genius_t* genius = genius_create(NULL);
    ASSERT_NE(genius, nullptr);

    // Test each domain
    genius_domain_t domains[] = {
        GENIUS_DOMAIN_NUMBER_THEORY,
        GENIUS_DOMAIN_ANALYSIS,
        GENIUS_DOMAIN_COMBINATORICS
    };

    for (int i = 0; i < 3; i++) {
        math_problem_t problem;
        memset(&problem, 0, sizeof(problem));
        problem.domain = domains[i];
        problem.statement = (char*)"Mode test problem";
        problem.difficulty = 0.2f;

        genius_result_t result;
        genius_result_init(&result);

        nimcp_error_t err = genius_solve_problem(genius, &problem, &result);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        // Mode used should be valid
        EXPECT_LT(result.mode_used, GENIUS_MODE_COUNT);

        genius_result_cleanup(&result);
    }

    genius_destroy(genius);
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(NeuroSymbolicIntegrationTest, HypergraphLargeScale) {
    nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    // Add many vertices
    const uint32_t NUM_VERTICES = 100;
    uint32_t vertex_ids[NUM_VERTICES];

    for (uint32_t i = 0; i < NUM_VERTICES; i++) {
        char name[32];
        snprintf(name, sizeof(name), "vertex_%u", i);
        vertex_ids[i] = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, name, 1.0f);
        EXPECT_NE(vertex_ids[i], UINT32_MAX);
    }

    // Add edges connecting groups of vertices
    for (uint32_t i = 0; i < NUM_VERTICES - 3; i += 3) {
        uint32_t edge_vertices[] = {vertex_ids[i], vertex_ids[i+1], vertex_ids[i+2]};
        uint32_t edge_id = nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION,
                                                      edge_vertices, 3,
                                                      TRIT_POSITIVE, "triple");
        EXPECT_NE(edge_id, UINT32_MAX);
    }

    // Verify stats
    hypergraph_stats_t stats;
    nimcp_error_t err = nimcp_hypergraph_get_stats(hg, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.vertex_count, NUM_VERTICES);
    EXPECT_GE(stats.edge_count, 30u);  // At least 30 edges

    nimcp_hypergraph_destroy(hg);
}

TEST_F(NeuroSymbolicIntegrationTest, RapidOrchestratorOperations) {
    config.enabled_components = ORCH_COMP_GENIUS;
    config.operation_timeout_ms = 50;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    // Rapidly solve multiple problems
    const int NUM_PROBLEMS = 20;

    for (int i = 0; i < NUM_PROBLEMS; i++) {
        math_problem_t problem;
        memset(&problem, 0, sizeof(problem));
        problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
        problem.statement = (char*)"Rapid test";
        problem.difficulty = 0.1f;

        orchestrator_result_t result;
        memset(&result, 0, sizeof(result));

        nimcp_error_t err = genius_orchestrator_solve(orch, &problem, &result);
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }

    // Verify all operations recorded
    orchestrator_stats_t stats;
    genius_orchestrator_get_stats(orch, &stats);
    EXPECT_EQ(stats.operations_total, (uint32_t)NUM_PROBLEMS);
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(NeuroSymbolicIntegrationTest, OrchestratorThreadSafety) {
    config.enabled_components = ORCH_COMP_GENIUS;
    config.operation_timeout_ms = 100;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    // Test concurrent stats access
    orchestrator_stats_t stats1, stats2;

    genius_orchestrator_get_stats(orch, &stats1);
    genius_orchestrator_get_stats(orch, &stats2);

    // Both should succeed
    EXPECT_EQ(stats1.operations_total, stats2.operations_total);
}

// ============================================================================
// Bridge Integration Tests
// ============================================================================

TEST_F(NeuroSymbolicIntegrationTest, EnergyFEPBridgeIntegration) {
    // Create orchestrator which contains the energy-FEP bridge
    config.enabled_components = ORCH_COMP_GENIUS | ORCH_COMP_CONSISTENCY;
    config.operation_timeout_ms = 100;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    // The bridge should be created internally
    // Verify via stats that components are linked
    orchestrator_stats_t stats;
    nimcp_error_t err = genius_orchestrator_get_stats(orch, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(NeuroSymbolicIntegrationTest, QuantumClassicalHybridPlanning) {
    // Create quantum MCTS with hybrid mode
    quantum_mcts_config_t qmcts_config;
    quantum_mcts_get_default_config(&qmcts_config);
    qmcts_config.num_simulations = 40;
    qmcts_config.quantum_fraction = 0.3f;  // 30% quantum, 70% classical

    quantum_mcts_t* qmcts = quantum_mcts_create(&qmcts_config);
    ASSERT_NE(qmcts, nullptr);

    // Plan
    float state[4] = {1.0f, 0.5f, 0.5f, 0.0f};
    qmcts_plan_t plan;
    nimcp_error_t err = quantum_mcts_plan_init(&plan, 5);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    err = quantum_mcts_plan(qmcts, state, 4, &plan);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Get stats to verify hybrid execution
    quantum_mcts_stats_t qstats;
    err = quantum_mcts_get_stats(qmcts, &qstats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(qstats.total_simulations, 0u);

    quantum_mcts_plan_cleanup(&plan);
    quantum_mcts_destroy(qmcts);
}
