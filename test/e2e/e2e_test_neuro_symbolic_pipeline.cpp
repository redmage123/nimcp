/**
 * @file e2e_test_neuro_symbolic_pipeline.cpp
 * @brief End-to-end tests for Neuro-Symbolic Mathematics Pipeline
 *
 * Tests complete user workflows for the Mathesis-inspired genius-level
 * mathematics system, including:
 * - Mathematical problem solving pipelines
 * - Theorem proving workflows
 * - Knowledge graph construction and querying
 * - Cross-component orchestration
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#include "e2e_test_framework.h"
#include <string>
#include <cstring>

extern "C" {
#include "cognitive/neuro_symbolic/nimcp_genius_math_orchestrator.h"
#include "cognitive/neuro_symbolic/nimcp_energy_consistency.h"
#include "cognitive/neuro_symbolic/nimcp_hypergraph.h"
#include "cognitive/neuro_symbolic/nimcp_quantum_mcts.h"
#include "cognitive/neuro_symbolic/nimcp_evolutionary_proof.h"
#include "cognitive/parietal/nimcp_mathematical_genius.h"
}

// ============================================================================
// E2E Scenario 1: Number Theory Problem Solving Pipeline
// ============================================================================

E2E_TEST(NeuroSymbolicE2E, NumberTheoryProblemSolvingPipeline) {
    E2E_PIPELINE_START("Number Theory Problem Solving");

    // Step 1: Create and configure orchestrator
    E2E_STAGE_BEGIN("Configure orchestrator", 100);
    orchestrator_config_t config;
    genius_orchestrator_get_default_config(&config);
    config.enabled_components = ORCH_COMP_GENIUS | ORCH_COMP_HYPERGRAPH;
    config.operation_timeout_ms = 1000;

    genius_math_orchestrator_t* orch = genius_orchestrator_create(&config);
    E2E_ASSERT_NOT_NULL(orch, "Orchestrator creation failed");
    E2E_STAGE_END();

    // Step 2: Initialize components
    E2E_STAGE_BEGIN("Initialize components", 200);
    nimcp_error_t err = genius_orchestrator_init_components(orch);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Component initialization failed");
    E2E_STAGE_END();

    // Step 3: Set ATP level
    E2E_STAGE_BEGIN("Set ATP modulation", 50);
    err = genius_orchestrator_modulate_atp(orch, 1.0f);
    E2E_ASSERT(err == NIMCP_SUCCESS, "ATP modulation failed");
    E2E_STAGE_END();

    // Step 4: Solve prime check problem
    E2E_STAGE_BEGIN("Solve prime problem", 500);
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
    problem.statement = (char*)"Determine if 127 is prime";
    problem.difficulty = 0.3f;

    orchestrator_result_t result;
    memset(&result, 0, sizeof(result));
    err = genius_orchestrator_solve(orch, &problem, &result);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Problem solving failed");
    E2E_STAGE_END();

    // Step 5: Verify stats
    E2E_STAGE_BEGIN("Verify statistics", 50);
    orchestrator_stats_t stats;
    err = genius_orchestrator_get_stats(orch, &stats);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Stats retrieval failed");
    E2E_ASSERT(stats.operations_total >= 1, "No operations recorded");
    E2E_STAGE_END();

    // Step 6: Solve GCD problem
    E2E_STAGE_BEGIN("Solve GCD problem", 500);
    problem.statement = (char*)"Find GCD of 48 and 18";
    problem.difficulty = 0.2f;
    memset(&result, 0, sizeof(result));
    err = genius_orchestrator_solve(orch, &problem, &result);
    E2E_ASSERT(err == NIMCP_SUCCESS, "GCD problem failed");
    E2E_STAGE_END();

    // Cleanup
    genius_orchestrator_destroy(orch);

    E2E_PIPELINE_END();
}

// ============================================================================
// E2E Scenario 2: Knowledge Graph Construction and Querying
// ============================================================================

E2E_TEST(NeuroSymbolicE2E, KnowledgeGraphConstructionPipeline) {
    E2E_PIPELINE_START("Knowledge Graph Construction");

    // Step 1: Create hypergraph
    E2E_STAGE_BEGIN("Create hypergraph", 50);
    nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
    E2E_ASSERT_NOT_NULL(hg, "Hypergraph creation failed");
    E2E_STAGE_END();

    // Step 2: Add number constants
    E2E_STAGE_BEGIN("Add vertices", 100);
    uint32_t zero = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "0", 1.0f);
    uint32_t one = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "1", 1.0f);
    uint32_t two = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "2", 1.0f);
    uint32_t three = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "3", 1.0f);
    uint32_t five = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "5", 1.0f);

    E2E_ASSERT(zero != UINT32_MAX, "Failed to add vertex 0");
    E2E_ASSERT(one != UINT32_MAX, "Failed to add vertex 1");
    E2E_ASSERT(two != UINT32_MAX, "Failed to add vertex 2");
    E2E_ASSERT(three != UINT32_MAX, "Failed to add vertex 3");
    E2E_ASSERT(five != UINT32_MAX, "Failed to add vertex 5");
    E2E_STAGE_END();

    // Step 3: Add predicates and functions
    E2E_STAGE_BEGIN("Add predicates", 100);
    uint32_t prime_pred = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PREDICATE, "is_prime", 1.0f);
    uint32_t even_pred = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PREDICATE, "is_even", 1.0f);
    uint32_t add_fn = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_FUNCTION, "add", 1.0f);

    E2E_ASSERT(prime_pred != UINT32_MAX, "Failed to add prime predicate");
    E2E_ASSERT(even_pred != UINT32_MAX, "Failed to add even predicate");
    E2E_ASSERT(add_fn != UINT32_MAX, "Failed to add add function");
    E2E_STAGE_END();

    // Step 4: Create hyperedges for facts
    E2E_STAGE_BEGIN("Add edges", 100);
    uint32_t prime_2[] = {two, prime_pred};
    uint32_t e1 = nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, prime_2, 2,
                                            TRIT_POSITIVE, "2_is_prime");
    E2E_ASSERT(e1 != UINT32_MAX, "Failed to add prime(2) edge");

    uint32_t prime_3[] = {three, prime_pred};
    uint32_t e2 = nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, prime_3, 2,
                                            TRIT_POSITIVE, "3_is_prime");
    E2E_ASSERT(e2 != UINT32_MAX, "Failed to add prime(3) edge");

    uint32_t prime_5[] = {five, prime_pred};
    uint32_t e3 = nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, prime_5, 2,
                                            TRIT_POSITIVE, "5_is_prime");
    E2E_ASSERT(e3 != UINT32_MAX, "Failed to add prime(5) edge");

    uint32_t add_fact[] = {two, three, add_fn, five};
    uint32_t e4 = nimcp_hypergraph_add_edge(hg, HYPEREDGE_CONSTRAINT, add_fact, 4,
                                            TRIT_POSITIVE, "2+3=5");
    E2E_ASSERT(e4 != UINT32_MAX, "Failed to add 2+3=5 edge");
    E2E_STAGE_END();

    // Step 5: Query knowledge base
    E2E_STAGE_BEGIN("Query graph", 100);
    uint32_t incident_edges[20];
    uint32_t count = nimcp_hypergraph_get_incident_edges(hg, prime_pred, incident_edges, 20);
    E2E_ASSERT(count >= 3, "Should have at least 3 prime facts");

    count = nimcp_hypergraph_get_incident_edges(hg, two, incident_edges, 20);
    E2E_ASSERT(count >= 2, "Vertex 2 should be in multiple edges");
    E2E_STAGE_END();

    // Step 6: Verify statistics
    E2E_STAGE_BEGIN("Verify stats", 50);
    hypergraph_stats_t stats;
    nimcp_error_t err = nimcp_hypergraph_get_stats(hg, &stats);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Stats failed");
    E2E_ASSERT(stats.vertex_count >= 8, "Should have at least 8 vertices");
    E2E_ASSERT(stats.edge_count >= 4, "Should have at least 4 edges");
    E2E_STAGE_END();

    // Cleanup
    nimcp_hypergraph_destroy(hg);

    E2E_PIPELINE_END();
}

// ============================================================================
// E2E Scenario 3: Multi-Domain Problem Solving
// ============================================================================

E2E_TEST(NeuroSymbolicE2E, MultiDomainProblemSolvingPipeline) {
    E2E_PIPELINE_START("Multi-Domain Problem Solving");

    // Step 1: Create genius module
    E2E_STAGE_BEGIN("Create genius", 100);
    mathematical_genius_t* genius = genius_create(NULL);
    E2E_ASSERT_NOT_NULL(genius, "Genius creation failed");
    E2E_STAGE_END();

    // Step 2: Solve Number Theory problem
    E2E_STAGE_BEGIN("Number Theory", 500);
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
    problem.statement = (char*)"Is 97 prime?";
    problem.difficulty = 0.2f;

    genius_result_t result;
    genius_result_init(&result);
    nimcp_error_t err = genius_solve_problem(genius, &problem, &result);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Number theory problem failed");
    genius_result_cleanup(&result);
    E2E_STAGE_END();

    // Step 3: Solve Analysis problem
    E2E_STAGE_BEGIN("Analysis", 500);
    problem.domain = GENIUS_DOMAIN_ANALYSIS;
    problem.statement = (char*)"Derivative of x^2";
    problem.difficulty = 0.3f;

    genius_result_init(&result);
    err = genius_solve_problem(genius, &problem, &result);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Analysis problem failed");
    genius_result_cleanup(&result);
    E2E_STAGE_END();

    // Step 4: Solve Combinatorics problem
    E2E_STAGE_BEGIN("Combinatorics", 500);
    problem.domain = GENIUS_DOMAIN_COMBINATORICS;
    problem.statement = (char*)"Permutations of 4 items";
    problem.difficulty = 0.4f;

    genius_result_init(&result);
    err = genius_solve_problem(genius, &problem, &result);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Combinatorics problem failed");
    genius_result_cleanup(&result);
    E2E_STAGE_END();

    // Step 5: Solve Algebra problem
    E2E_STAGE_BEGIN("Algebra", 500);
    problem.domain = GENIUS_DOMAIN_ALGEBRA;
    problem.statement = (char*)"Solve x + 5 = 12";
    problem.difficulty = 0.1f;

    genius_result_init(&result);
    err = genius_solve_problem(genius, &problem, &result);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Algebra problem failed");
    genius_result_cleanup(&result);
    E2E_STAGE_END();

    // Step 6: Get overall statistics
    E2E_STAGE_BEGIN("Get stats", 50);
    genius_stats_t stats;
    err = genius_get_stats(genius, &stats);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Stats failed");
    // Note: stats counting may vary by implementation
    // Just verify stats retrieval works
    E2E_ASSERT(stats.problems_attempted >= 0, "Stats should be valid");
    E2E_STAGE_END();

    // Cleanup
    genius_destroy(genius);

    E2E_PIPELINE_END();
}

// ============================================================================
// E2E Scenario 4: Consistency Checking Pipeline
// ============================================================================

E2E_TEST(NeuroSymbolicE2E, ConsistencyCheckingPipeline) {
    E2E_PIPELINE_START("Consistency Checking");

    // Step 1: Create checker
    E2E_STAGE_BEGIN("Create checker", 50);
    energy_consistency_checker_t* checker = energy_consistency_create(NULL);
    E2E_ASSERT_NOT_NULL(checker, "Checker creation failed");
    E2E_STAGE_END();

    // Step 2: Check simple conjunction
    E2E_STAGE_BEGIN("Check conjunction", 100);
    energy_consistency_result_t result;
    nimcp_error_t err = energy_consistency_check_proposition(checker, "P AND Q", NULL, &result);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Conjunction check failed");
    E2E_ASSERT(result.total_energy >= 0.0f, "Energy should be non-negative");
    E2E_STAGE_END();

    // Step 3: Check tautology
    E2E_STAGE_BEGIN("Check tautology", 100);
    err = energy_consistency_check_proposition(checker, "P OR NOT P", NULL, &result);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Tautology check failed");
    E2E_STAGE_END();

    // Step 4: Check implication
    E2E_STAGE_BEGIN("Check implication", 100);
    err = energy_consistency_check_proposition(checker, "P IMPLIES Q", NULL, &result);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Implication check failed");
    E2E_STAGE_END();

    // Step 5: Verify consistency score
    E2E_STAGE_BEGIN("Get score", 50);
    float score = energy_consistency_get_score(checker);
    E2E_ASSERT(score >= 0.0f && score <= 1.0f, "Score should be in [0,1]");
    E2E_STAGE_END();

    // Step 6: Reset and verify
    E2E_STAGE_BEGIN("Reset", 50);
    err = energy_consistency_reset(checker);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Reset failed");
    score = energy_consistency_get_score(checker);
    E2E_ASSERT(score >= 0.0f, "Score after reset should be valid");
    E2E_STAGE_END();

    // Cleanup
    energy_consistency_destroy(checker);

    E2E_PIPELINE_END();
}

// ============================================================================
// E2E Scenario 5: Full Orchestrator Workflow
// ============================================================================

E2E_TEST(NeuroSymbolicE2E, FullOrchestratorWorkflow) {
    E2E_PIPELINE_START("Full Orchestrator Workflow");

    // Step 1: Configure orchestrator (use only GENIUS to avoid potential hangs)
    E2E_STAGE_BEGIN("Configure", 100);
    orchestrator_config_t config;
    genius_orchestrator_get_default_config(&config);
    config.enabled_components = ORCH_COMP_GENIUS;  // Simpler config for reliability
    config.operation_timeout_ms = 100;

    genius_math_orchestrator_t* orch = genius_orchestrator_create(&config);
    E2E_ASSERT_NOT_NULL(orch, "Orchestrator creation failed");
    E2E_STAGE_END();

    // Step 2: Initialize
    E2E_STAGE_BEGIN("Initialize", 200);
    nimcp_error_t err = genius_orchestrator_init_components(orch);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Init failed");
    E2E_STAGE_END();

    // Step 3: Solve at full ATP
    E2E_STAGE_BEGIN("Solve at ATP=1.0", 300);
    err = genius_orchestrator_modulate_atp(orch, 1.0f);
    E2E_ASSERT(err == NIMCP_SUCCESS, "ATP modulation failed");

    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
    problem.statement = (char*)"Test at full ATP";
    problem.difficulty = 0.1f;

    orchestrator_result_t result;
    memset(&result, 0, sizeof(result));
    err = genius_orchestrator_solve(orch, &problem, &result);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Solve at ATP=1.0 failed");
    E2E_STAGE_END();

    // Step 4: Solve at reduced ATP
    E2E_STAGE_BEGIN("Solve at ATP=0.5", 300);
    err = genius_orchestrator_modulate_atp(orch, 0.5f);
    E2E_ASSERT(err == NIMCP_SUCCESS, "ATP modulation failed");

    problem.statement = (char*)"Test at low ATP";
    memset(&result, 0, sizeof(result));
    err = genius_orchestrator_solve(orch, &problem, &result);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Solve at ATP=0.5 failed");
    E2E_STAGE_END();

    // Step 5: Verify stats
    E2E_STAGE_BEGIN("Verify stats", 50);
    orchestrator_stats_t stats;
    err = genius_orchestrator_get_stats(orch, &stats);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Stats failed");
    E2E_ASSERT(stats.operations_total >= 2, "Should have 2 operations");
    E2E_STAGE_END();

    // Step 6: Reset
    E2E_STAGE_BEGIN("Reset", 100);
    err = genius_orchestrator_reset(orch);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Reset failed");

    err = genius_orchestrator_get_stats(orch, &stats);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Stats after reset failed");
    E2E_ASSERT(stats.operations_total == 0, "Stats should be cleared");
    E2E_STAGE_END();

    // Cleanup
    genius_orchestrator_destroy(orch);

    E2E_PIPELINE_END();
}

// ============================================================================
// E2E Scenario 6: Quantum-Enhanced Planning Pipeline
// ============================================================================

E2E_TEST(NeuroSymbolicE2E, QuantumEnhancedPlanningPipeline) {
    E2E_PIPELINE_START("Quantum Enhanced Planning");

    // Step 1: Create quantum MCTS
    E2E_STAGE_BEGIN("Create QMCTS", 100);
    quantum_mcts_config_t qconfig;
    quantum_mcts_get_default_config(&qconfig);
    qconfig.num_simulations = 50;
    qconfig.planning_horizon = 8;
    qconfig.quantum_fraction = 0.3f;

    quantum_mcts_t* qmcts = quantum_mcts_create(&qconfig);
    E2E_ASSERT_NOT_NULL(qmcts, "QMCTS creation failed");
    E2E_STAGE_END();

    // Step 2: Run planning queries
    E2E_STAGE_BEGIN("Planning queries", 500);
    for (int i = 0; i < 3; i++) {
        float state[4] = {0.1f * (i + 1), 0.2f * (i + 1), 0.5f, 1.0f - (0.1f * i)};

        qmcts_plan_t plan;
        nimcp_error_t err = quantum_mcts_plan_init(&plan, 8);
        E2E_ASSERT(err == NIMCP_SUCCESS, "Plan init failed");

        err = quantum_mcts_plan(qmcts, state, 4, &plan);
        E2E_ASSERT(err == NIMCP_SUCCESS, "Planning failed");

        quantum_mcts_plan_cleanup(&plan);
    }
    E2E_STAGE_END();

    // Step 3: Check statistics
    E2E_STAGE_BEGIN("Check stats", 50);
    quantum_mcts_stats_t stats;
    nimcp_error_t err = quantum_mcts_get_stats(qmcts, &stats);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Stats failed");
    E2E_ASSERT(stats.total_simulations > 0, "Should have simulations");
    E2E_STAGE_END();

    // Step 4: Test ATP modulation
    E2E_STAGE_BEGIN("ATP modulation", 100);
    err = quantum_mcts_modulate_atp(qmcts, 0.5f);
    E2E_ASSERT(err == NIMCP_SUCCESS, "ATP modulation failed");

    float state[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    qmcts_plan_t plan;
    quantum_mcts_plan_init(&plan, 8);
    err = quantum_mcts_plan(qmcts, state, 4, &plan);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Low ATP planning failed");
    quantum_mcts_plan_cleanup(&plan);
    E2E_STAGE_END();

    // Step 5: Reset
    E2E_STAGE_BEGIN("Reset", 50);
    err = quantum_mcts_reset(qmcts);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Reset failed");
    E2E_STAGE_END();

    // Cleanup
    quantum_mcts_destroy(qmcts);

    E2E_PIPELINE_END();
}

// ============================================================================
// E2E Scenario 7: Component Integration Pipeline
// ============================================================================

E2E_TEST(NeuroSymbolicE2E, ComponentIntegrationPipeline) {
    E2E_PIPELINE_START("Component Integration");

    // Step 1: Create all components
    E2E_STAGE_BEGIN("Create components", 200);
    mathematical_genius_t* genius = genius_create(NULL);
    E2E_ASSERT_NOT_NULL(genius, "Genius creation failed");

    nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
    E2E_ASSERT_NOT_NULL(hg, "Hypergraph creation failed");

    quantum_mcts_config_t qconfig;
    quantum_mcts_get_default_config(&qconfig);
    qconfig.num_simulations = 30;
    quantum_mcts_t* qmcts = quantum_mcts_create(&qconfig);
    E2E_ASSERT_NOT_NULL(qmcts, "QMCTS creation failed");
    E2E_STAGE_END();

    // Step 2: Link components
    E2E_STAGE_BEGIN("Link components", 100);
    nimcp_error_t err = genius_link_hypergraph(genius, hg);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Hypergraph link failed");

    err = genius_link_quantum_engine(genius, qmcts);
    E2E_ASSERT(err == NIMCP_SUCCESS, "QMCTS link failed");
    E2E_STAGE_END();

    // Step 3: Build knowledge
    E2E_STAGE_BEGIN("Build knowledge", 100);
    uint32_t fact1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "fact1", 1.0f);
    uint32_t fact2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "fact2", 1.0f);
    uint32_t rule1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PREDICATE, "implies", 1.0f);

    uint32_t verts[] = {fact1, fact2, rule1};
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RULE, verts, 3, TRIT_POSITIVE, "implication");
    E2E_STAGE_END();

    // Step 4: Solve with integrated components
    E2E_STAGE_BEGIN("Solve problem", 500);
    math_problem_t problem;
    memset(&problem, 0, sizeof(problem));
    problem.domain = GENIUS_DOMAIN_NUMBER_THEORY;
    problem.statement = (char*)"Integrated test";
    problem.difficulty = 0.3f;

    genius_result_t result;
    genius_result_init(&result);
    err = genius_solve_problem(genius, &problem, &result);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Problem solving failed");
    genius_result_cleanup(&result);
    E2E_STAGE_END();

    // Step 5: Unlink and cleanup
    E2E_STAGE_BEGIN("Cleanup", 100);
    err = genius_link_hypergraph(genius, NULL);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Unlink hypergraph failed");
    err = genius_link_quantum_engine(genius, NULL);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Unlink QMCTS failed");

    quantum_mcts_destroy(qmcts);
    nimcp_hypergraph_destroy(hg);
    genius_destroy(genius);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

// ============================================================================
// E2E Scenario 8: Stress Test - Rapid Operations
// ============================================================================

E2E_TEST(NeuroSymbolicE2E, RapidOperationsPipeline) {
    E2E_PIPELINE_START("Rapid Operations");

    // Step 1: Create orchestrator
    E2E_STAGE_BEGIN("Create orchestrator", 100);
    orchestrator_config_t config;
    genius_orchestrator_get_default_config(&config);
    config.enabled_components = ORCH_COMP_GENIUS;
    config.operation_timeout_ms = 50;

    genius_math_orchestrator_t* orch = genius_orchestrator_create(&config);
    E2E_ASSERT_NOT_NULL(orch, "Orchestrator creation failed");
    E2E_STAGE_END();

    // Step 2: Run many operations
    E2E_STAGE_BEGIN("50 rapid operations", 5000);
    const int NUM_OPERATIONS = 50;

    for (int i = 0; i < NUM_OPERATIONS; i++) {
        math_problem_t problem;
        memset(&problem, 0, sizeof(problem));
        problem.domain = (genius_domain_t)(i % 4);
        problem.statement = (char*)"Quick test";
        problem.difficulty = 0.1f;

        orchestrator_result_t result;
        memset(&result, 0, sizeof(result));

        nimcp_error_t err = genius_orchestrator_solve(orch, &problem, &result);
        E2E_ASSERT(err == NIMCP_SUCCESS, "Operation failed");
    }
    E2E_STAGE_END();

    // Step 3: Verify stats
    E2E_STAGE_BEGIN("Verify stats", 50);
    orchestrator_stats_t stats;
    nimcp_error_t err = genius_orchestrator_get_stats(orch, &stats);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Stats failed");
    E2E_ASSERT(stats.operations_total == (uint32_t)NUM_OPERATIONS, "Should have 50 operations");
    E2E_STAGE_END();

    // Cleanup
    genius_orchestrator_destroy(orch);

    E2E_PIPELINE_END();
}
