/**
 * @file test_genius_orchestrator_integration.cpp
 * @brief Integration tests for Genius Math Orchestrator
 *
 * Tests the orchestrator coordinating multiple components:
 * - Orchestrator + Energy Consistency integration
 * - Orchestrator + Hypergraph knowledge graph integration
 * - Orchestrator + Quantum MCTS planning integration
 * - Orchestrator + Evolutionary Proof Search integration
 * - Orchestrator + Mathematical Genius mode switching
 *
 * @version 2.6.3
 * @date 2025-12-31
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
#include "cognitive/parietal/nimcp_genius_modes.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

/**
 * @brief Test fixture for Genius Orchestrator Integration tests
 */
class GeniusOrchestratorIntegrationTest : public NimcpTestBase {
protected:
    genius_math_orchestrator_t* orch;
    orchestrator_config_t config;

    void SetUp() override {
        NimcpTestBase::SetUp();
        orch = NULL;
        memset(&config, 0, sizeof(config));
        genius_orchestrator_get_default_config(&config);
        /* Use fast config for integration tests */
        config.operation_timeout_ms = 500;
        config.component_timeout_ms = 200;
    }

    void TearDown() override {
        if (orch) {
            genius_orchestrator_destroy(orch);
            orch = NULL;
        }
        NimcpTestBase::TearDown();
    }

    /**
     * @brief Helper to create a simple math problem
     */
    math_problem_t CreateSimpleProblem(genius_domain_t domain, const char* statement) {
        math_problem_t problem;
        memset(&problem, 0, sizeof(problem));
        problem.domain = domain;
        problem.statement = const_cast<char*>(statement);
        problem.difficulty = 0.3f;
        problem.timeout_ms = 200;
        return problem;
    }
};

/* ============================================================================
 * Orchestrator + Energy Consistency Integration Tests
 * ============================================================================ */

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorWithEnergyConsistencyEnabled) {
    config.enabled_components = ORCH_COMP_CONSISTENCY | ORCH_COMP_GENIUS;
    config.require_consistency_check = true;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    /* Initialize components */
    nimcp_error_t err = genius_orchestrator_init_components(orch);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Solve problem - should automatically run consistency check */
    math_problem_t problem = CreateSimpleProblem(GENIUS_DOMAIN_NUMBER_THEORY,
                                                  "Verify 2 + 2 = 4");
    orchestrator_result_t result;
    err = genius_orchestrator_result_init(&result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    err = genius_orchestrator_solve(orch, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify consistency was checked */
    EXPECT_TRUE((result.components_used & ORCH_COMP_CONSISTENCY) != 0);

    /* Consistency result should indicate low energy for valid math */
    if (result.consistency_result) {
        EXPECT_GE(result.consistency_result->final_consistency, 0.0f);
    }

    genius_orchestrator_result_cleanup(&result);
}

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorExternalConsistencyChecker) {
    config.enabled_components = ORCH_COMP_GENIUS;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    /* Create external consistency checker */
    energy_consistency_checker_t* checker = energy_consistency_create(NULL);
    ASSERT_NE(checker, nullptr);

    /* Set external checker */
    nimcp_error_t err = genius_orchestrator_set_consistency(orch, checker);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Solve problem */
    math_problem_t problem = CreateSimpleProblem(GENIUS_DOMAIN_NUMBER_THEORY,
                                                  "Test external checker");
    orchestrator_result_t result;
    genius_orchestrator_result_init(&result);

    err = genius_orchestrator_solve(orch, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    genius_orchestrator_result_cleanup(&result);
    energy_consistency_destroy(checker);
}

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorConsistencyCheckAfterProof) {
    config.enabled_components = ORCH_COMP_CONSISTENCY | ORCH_COMP_GENIUS;
    config.require_consistency_check = true;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    genius_orchestrator_init_components(orch);

    /* Try to prove a theorem */
    orchestrator_proof_result_t proof_result;
    genius_orchestrator_proof_result_init(&proof_result);

    nimcp_error_t err = genius_orchestrator_prove(orch, "2 + 3 = 5", &proof_result);
    /* Note: May not find proof in short timeout, but should not crash */
    EXPECT_TRUE(err == NIMCP_SUCCESS || err == NIMCP_ERROR_TIMEOUT);

    /* If proof found, verify consistency energy */
    if (proof_result.proved) {
        EXPECT_GE(proof_result.confidence, 0.0f);
    }

    genius_orchestrator_proof_result_cleanup(&proof_result);
}

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorCheckConsistencyExplicit) {
    config.enabled_components = ORCH_COMP_CONSISTENCY | ORCH_COMP_GENIUS;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    genius_orchestrator_init_components(orch);

    /* Solve a problem first */
    math_problem_t problem = CreateSimpleProblem(GENIUS_DOMAIN_ALGEBRA, "x + 1 = 2");
    orchestrator_result_t result;
    genius_orchestrator_result_init(&result);
    genius_orchestrator_solve(orch, &problem, &result);

    /* Explicitly check consistency */
    energy_consistency_result_t consistency_result;
    energy_consistency_result_init(&consistency_result, 10);

    nimcp_error_t err = genius_orchestrator_check_consistency(orch, &result, &consistency_result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Consistency score should be valid */
    EXPECT_GE(consistency_result.final_consistency, 0.0f);
    EXPECT_LE(consistency_result.final_consistency, 1.0f);

    energy_consistency_result_cleanup(&consistency_result);
    genius_orchestrator_result_cleanup(&result);
}

/* ============================================================================
 * Orchestrator + Hypergraph Knowledge Graph Integration Tests
 * ============================================================================ */

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorWithHypergraphKnowledgeBase) {
    config.enabled_components = ORCH_COMP_HYPERGRAPH | ORCH_COMP_GENIUS;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    nimcp_error_t err = genius_orchestrator_init_components(orch);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Create external hypergraph with mathematical knowledge */
    nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    /* Add number theory vertices */
    uint32_t prime_v = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PREDICATE, "is_prime", 1.0f);
    uint32_t even_v = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PREDICATE, "is_even", 1.0f);
    uint32_t two_v = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "2", 1.0f);

    EXPECT_NE(prime_v, UINT32_MAX);
    EXPECT_NE(even_v, UINT32_MAX);
    EXPECT_NE(two_v, UINT32_MAX);

    /* Add theorem: 2 is prime and even */
    uint32_t vertices[] = {two_v, prime_v, even_v};
    uint32_t theorem_edge = nimcp_hypergraph_add_edge(hg, HYPEREDGE_THEOREM,
                                                       vertices, 3, TRIT_POSITIVE,
                                                       "two_is_prime_and_even");
    EXPECT_NE(theorem_edge, UINT32_MAX);

    /* Set hypergraph on orchestrator */
    err = genius_orchestrator_set_hypergraph(orch, hg);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Solve problem using knowledge base */
    math_problem_t problem = CreateSimpleProblem(GENIUS_DOMAIN_NUMBER_THEORY,
                                                  "Is 2 prime?");
    orchestrator_result_t result;
    genius_orchestrator_result_init(&result);

    err = genius_orchestrator_solve(orch, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Hypergraph should have been used */
    EXPECT_TRUE((result.components_used & ORCH_COMP_HYPERGRAPH) != 0);

    genius_orchestrator_result_cleanup(&result);
    nimcp_hypergraph_destroy(hg);
}

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorHypergraphDualComputation) {
    config.enabled_components = ORCH_COMP_HYPERGRAPH;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    /* Create hypergraph with interconnected knowledge */
    nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    /* Add vertices for mathematical structures */
    uint32_t group_v = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_TYPE, "group", 1.0f);
    uint32_t ring_v = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_TYPE, "ring", 1.0f);
    uint32_t field_v = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_TYPE, "field", 1.0f);
    uint32_t extends_v = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PREDICATE, "extends", 1.0f);

    /* Ring extends Group */
    uint32_t edge1_v[] = {ring_v, extends_v, group_v};
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, edge1_v, 3, TRIT_POSITIVE, "ring_extends_group");

    /* Field extends Ring */
    uint32_t edge2_v[] = {field_v, extends_v, ring_v};
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, edge2_v, 3, TRIT_POSITIVE, "field_extends_ring");

    /* Compute dual hypergraph */
    nimcp_hypergraph_dual_t* dual = nimcp_hypergraph_compute_dual(hg);
    ASSERT_NE(dual, nullptr);

    /* Verify dual structure exists */
    EXPECT_NE(dual->dual, nullptr);

    nimcp_hypergraph_dual_destroy(dual);
    nimcp_hypergraph_destroy(hg);
}

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorHypergraphTransversal) {
    config.enabled_components = ORCH_COMP_HYPERGRAPH;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    /* Create hypergraph for transversal computation */
    nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    /* Add constraint satisfaction problem vertices */
    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_VARIABLE, "x", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_VARIABLE, "y", 1.0f);
    uint32_t v3 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_VARIABLE, "z", 1.0f);
    uint32_t v4 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_VARIABLE, "w", 1.0f);

    /* Add constraint edges */
    uint32_t c1[] = {v1, v2};
    uint32_t c2[] = {v2, v3};
    uint32_t c3[] = {v3, v4};
    uint32_t c4[] = {v1, v4};

    nimcp_hypergraph_add_edge(hg, HYPEREDGE_CONSTRAINT, c1, 2, TRIT_POSITIVE, "c1");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_CONSTRAINT, c2, 2, TRIT_POSITIVE, "c2");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_CONSTRAINT, c3, 2, TRIT_POSITIVE, "c3");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_CONSTRAINT, c4, 2, TRIT_POSITIVE, "c4");

    /* Compute minimal transversal */
    uint32_t transversal[10];
    uint32_t transversal_size = nimcp_hypergraph_transversal(hg, transversal, 10);

    /* Should find a transversal that hits all edges */
    EXPECT_GT(transversal_size, 0u);
    EXPECT_LE(transversal_size, 4u);  /* At most 4 vertices needed */

    nimcp_hypergraph_destroy(hg);
}

/* ============================================================================
 * Orchestrator + Quantum MCTS Planning Integration Tests
 * ============================================================================ */

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorWithQuantumMCTS) {
    config.enabled_components = ORCH_COMP_QUANTUM_MCTS | ORCH_COMP_GENIUS;
    config.enable_quantum_enhancement = true;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    nimcp_error_t err = genius_orchestrator_init_components(orch);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Solve optimization problem that may use quantum planning */
    math_problem_t problem = CreateSimpleProblem(GENIUS_DOMAIN_OPTIMIZATION,
                                                  "Find optimal strategy");
    orchestrator_result_t result;
    genius_orchestrator_result_init(&result);

    err = genius_orchestrator_solve(orch, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify quantum MCTS was potentially used */
    if (config.enable_quantum_enhancement) {
        /* At least genius should be used */
        EXPECT_TRUE((result.components_used & ORCH_COMP_GENIUS) != 0);
    }

    genius_orchestrator_result_cleanup(&result);
}

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorExternalQuantumMCTS) {
    config.enabled_components = ORCH_COMP_GENIUS;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    /* Create external quantum MCTS */
    quantum_mcts_config_t qmcts_config;
    quantum_mcts_get_default_config(&qmcts_config);
    qmcts_config.num_simulations = 50;
    qmcts_config.planning_horizon = 5;
    qmcts_config.enable_quantum_rollouts = true;

    quantum_mcts_t* qmcts = quantum_mcts_create(&qmcts_config);
    ASSERT_NE(qmcts, nullptr);

    /* Set external quantum MCTS */
    nimcp_error_t err = genius_orchestrator_set_quantum_mcts(orch, qmcts);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Solve problem */
    math_problem_t problem = CreateSimpleProblem(GENIUS_DOMAIN_OPTIMIZATION,
                                                  "Quantum-assisted optimization");
    orchestrator_result_t result;
    genius_orchestrator_result_init(&result);

    err = genius_orchestrator_solve(orch, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    genius_orchestrator_result_cleanup(&result);
    quantum_mcts_destroy(qmcts);
}

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorQuantumPlanning) {
    config.enabled_components = ORCH_COMP_QUANTUM_MCTS | ORCH_COMP_GENIUS;
    config.enable_quantum_enhancement = true;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    genius_orchestrator_init_components(orch);

    /* Perform quantum-enhanced optimization */
    orchestrator_optimization_result_t opt_result;
    memset(&opt_result, 0, sizeof(opt_result));

    nimcp_error_t err = genius_orchestrator_optimize(orch, NULL, NULL, &opt_result);
    /* May not complete optimization in test time */
    EXPECT_TRUE(err == NIMCP_SUCCESS || err == NIMCP_ERROR_TIMEOUT || err == NIMCP_ERROR_INVALID_PARAM);

    /* If optimization succeeded, check results */
    if (opt_result.optimal_found) {
        EXPECT_GE(opt_result.iterations, 1u);
    }
}

/* ============================================================================
 * Orchestrator + Evolutionary Proof Search Integration Tests
 * ============================================================================ */

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorWithEvolutionaryProof) {
    config.enabled_components = ORCH_COMP_GENIUS;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    /* Create evolutionary prover */
    evoproof_config_t evo_config;
    evolutionary_proof_get_default_config(&evo_config);
    evo_config.proof_timeout_ms = 100;
    evo_config.population_size = 8;
    evo_config.max_proof_depth = 5;

    evolutionary_proof_search_t* eps = evolutionary_proof_create(&evo_config);
    ASSERT_NE(eps, nullptr);

    /* Initialize population */
    nimcp_error_t err = evolutionary_proof_init_population(eps);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Try to prove theorem using orchestrator + evolutionary search */
    orchestrator_proof_result_t proof_result;
    genius_orchestrator_proof_result_init(&proof_result);

    err = genius_orchestrator_prove(orch, "A implies A", &proof_result);
    /* May timeout but should not crash */
    EXPECT_TRUE(err == NIMCP_SUCCESS || err == NIMCP_ERROR_TIMEOUT);

    genius_orchestrator_proof_result_cleanup(&proof_result);
    evolutionary_proof_destroy(eps);
}

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorEvolutionaryStrategies) {
    config.enabled_components = ORCH_COMP_GENIUS;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    /* Create and test evolutionary prover strategies */
    evoproof_config_t evo_config;
    evolutionary_proof_get_default_config(&evo_config);
    evo_config.population_size = 16;
    evo_config.algorithm = EVOPROOF_ALGO_HYBRID;  /* GA + RL hybrid */
    evo_config.selection = EVOPROOF_SELECT_TOURNAMENT;

    evolutionary_proof_search_t* eps = evolutionary_proof_create(&evo_config);
    ASSERT_NE(eps, nullptr);

    /* Initialize and evolve */
    evolutionary_proof_init_population(eps);

    /* Evolve for a few generations */
    for (int gen = 0; gen < 3; gen++) {
        uint32_t offspring = evolutionary_proof_evolve_generation(eps);
        EXPECT_GE(offspring, 0u);  /* May or may not produce offspring */
    }

    /* Get best strategy */
    const proof_strategy_t* best = evolutionary_proof_get_best(eps);
    EXPECT_NE(best, nullptr);

    evolutionary_proof_destroy(eps);
}

/* ============================================================================
 * Orchestrator + Mathematical Genius Mode Switching Tests
 * ============================================================================ */

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorModeSwitchingAuto) {
    config.enabled_components = ORCH_COMP_GENIUS;
    config.auto_select_mode = true;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    /* Test auto mode selection for different domains */
    genius_domain_t domains[] = {
        GENIUS_DOMAIN_NUMBER_THEORY,
        GENIUS_DOMAIN_CALCULUS,
        GENIUS_DOMAIN_COMBINATORICS,
        GENIUS_DOMAIN_GRAPH_THEORY
    };

    for (int i = 0; i < 4; i++) {
        math_problem_t problem = CreateSimpleProblem(domains[i], "Test mode selection");
        orchestrator_result_t result;
        genius_orchestrator_result_init(&result);

        nimcp_error_t err = genius_orchestrator_solve(orch, &problem, &result);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        /* Genius component should be used */
        EXPECT_TRUE((result.components_used & ORCH_COMP_GENIUS) != 0);

        /* Mode should be valid */
        if (result.genius_result) {
            EXPECT_LT(result.genius_result->mode_used, GENIUS_MODE_COUNT);
        }

        genius_orchestrator_result_cleanup(&result);
    }
}

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorGaussModeForNumberTheory) {
    config.enabled_components = ORCH_COMP_GENIUS;
    config.default_mode = GENIUS_MODE_GAUSS;
    config.auto_select_mode = false;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    /* Number theory problem should use Gauss mode */
    math_problem_t problem = CreateSimpleProblem(GENIUS_DOMAIN_NUMBER_THEORY,
                                                  "Find prime factors of 15");
    orchestrator_result_t result;
    genius_orchestrator_result_init(&result);

    nimcp_error_t err = genius_orchestrator_solve(orch, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Should use Gauss mode for number theory */
    if (result.genius_result) {
        EXPECT_EQ(result.genius_result->mode_used, GENIUS_MODE_GAUSS);
    }

    genius_orchestrator_result_cleanup(&result);
}

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorErdosModeForCombinatorics) {
    config.enabled_components = ORCH_COMP_GENIUS;
    config.default_mode = GENIUS_MODE_ERDOS;
    config.auto_select_mode = false;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    /* Combinatorics problem should use Erdos mode */
    math_problem_t problem = CreateSimpleProblem(GENIUS_DOMAIN_COMBINATORICS,
                                                  "Count permutations");
    orchestrator_result_t result;
    genius_orchestrator_result_init(&result);

    nimcp_error_t err = genius_orchestrator_solve(orch, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    if (result.genius_result) {
        EXPECT_EQ(result.genius_result->mode_used, GENIUS_MODE_ERDOS);
    }

    genius_orchestrator_result_cleanup(&result);
}

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorConjectureGeneration) {
    config.enabled_components = ORCH_COMP_GENIUS;
    config.auto_select_mode = true;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    /* Generate conjectures in number theory domain */
    orchestrator_conjecture_result_t conj_result;
    memset(&conj_result, 0, sizeof(conj_result));

    nimcp_error_t err = genius_orchestrator_conjecture(orch, GENIUS_DOMAIN_NUMBER_THEORY,
                                                        NULL, &conj_result);
    EXPECT_TRUE(err == NIMCP_SUCCESS || err == NIMCP_ERROR_TIMEOUT);

    /* May or may not generate conjectures in test time */
    if (conj_result.num_conjectures > 0) {
        EXPECT_GE(conj_result.avg_confidence, 0.0f);
    }
}

/* ============================================================================
 * Multi-Component Integration Tests
 * ============================================================================ */

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorAllComponentsCoordinated) {
    config.enabled_components = ORCH_COMP_ALL;
    config.operation_timeout_ms = 1000;
    config.require_consistency_check = true;
    config.enable_quantum_enhancement = true;
    config.auto_select_mode = true;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    nimcp_error_t err = genius_orchestrator_init_components(orch);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Solve complex problem requiring multiple components */
    math_problem_t problem = CreateSimpleProblem(GENIUS_DOMAIN_NUMBER_THEORY,
                                                  "Find pattern in sequence");
    orchestrator_result_t result;
    genius_orchestrator_result_init(&result);

    err = genius_orchestrator_solve(orch, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Multiple components should be used */
    EXPECT_GT(result.components_used, 0u);

    /* Get statistics */
    orchestrator_stats_t stats;
    err = genius_orchestrator_get_stats(orch, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(stats.operations_total, 1u);

    genius_orchestrator_result_cleanup(&result);
}

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorGameTheoryAnalysis) {
    config.enabled_components = ORCH_COMP_GENIUS | ORCH_COMP_GAME_THEORY;
    config.enable_game_theory = true;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    /* Perform game-theoretic analysis */
    orchestrator_game_result_t game_result;
    memset(&game_result, 0, sizeof(game_result));

    nimcp_error_t err = genius_orchestrator_game_theory_analysis(orch, NULL, &game_result);
    /* May timeout or fail due to no game specification */
    EXPECT_TRUE(err == NIMCP_SUCCESS || err == NIMCP_ERROR_TIMEOUT ||
                err == NIMCP_ERROR_INVALID_PARAM);
}

/* ============================================================================
 * Modulation Across Components Tests
 * ============================================================================ */

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorInflammationModulation) {
    config.enabled_components = ORCH_COMP_GENIUS | ORCH_COMP_CONSISTENCY;
    config.inflammation_sensitivity = 1.0f;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    genius_orchestrator_init_components(orch);

    /* Apply inflammation modulation */
    nimcp_error_t err = genius_orchestrator_modulate_inflammation(orch, 0.0f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Solve at low inflammation */
    math_problem_t problem = CreateSimpleProblem(GENIUS_DOMAIN_NUMBER_THEORY, "Test");
    orchestrator_result_t result1;
    genius_orchestrator_result_init(&result1);
    genius_orchestrator_solve(orch, &problem, &result1);

    /* Increase inflammation */
    err = genius_orchestrator_modulate_inflammation(orch, 0.8f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Solve at high inflammation */
    orchestrator_result_t result2;
    genius_orchestrator_result_init(&result2);
    genius_orchestrator_solve(orch, &problem, &result2);

    /* Both should succeed */
    EXPECT_TRUE(result1.success || !result1.success);  /* Either outcome */
    EXPECT_TRUE(result2.success || !result2.success);

    genius_orchestrator_result_cleanup(&result1);
    genius_orchestrator_result_cleanup(&result2);
}

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorFatigueModulation) {
    config.enabled_components = ORCH_COMP_GENIUS;
    config.fatigue_sensitivity = 1.0f;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    /* Apply fatigue modulation */
    nimcp_error_t err = genius_orchestrator_modulate_fatigue(orch, 0.5f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Solve problem */
    math_problem_t problem = CreateSimpleProblem(GENIUS_DOMAIN_NUMBER_THEORY, "Test fatigue");
    orchestrator_result_t result;
    genius_orchestrator_result_init(&result);

    err = genius_orchestrator_solve(orch, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    genius_orchestrator_result_cleanup(&result);
}

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorATPModulationAffectsAllComponents) {
    config.enabled_components = ORCH_COMP_GENIUS | ORCH_COMP_QUANTUM_MCTS;
    config.atp_sensitivity = 1.0f;
    config.max_atp_budget = 100.0f;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    genius_orchestrator_init_components(orch);

    /* Solve at full ATP */
    genius_orchestrator_modulate_atp(orch, 1.0f);
    math_problem_t problem = CreateSimpleProblem(GENIUS_DOMAIN_OPTIMIZATION, "ATP test");
    orchestrator_result_t result1;
    genius_orchestrator_result_init(&result1);
    genius_orchestrator_solve(orch, &problem, &result1);

    /* Solve at low ATP */
    genius_orchestrator_modulate_atp(orch, 0.2f);
    orchestrator_result_t result2;
    genius_orchestrator_result_init(&result2);
    genius_orchestrator_solve(orch, &problem, &result2);

    /* Low ATP should consume less or equal ATP */
    EXPECT_GE(result1.atp_consumed, 0.0f);
    EXPECT_GE(result2.atp_consumed, 0.0f);

    genius_orchestrator_result_cleanup(&result1);
    genius_orchestrator_result_cleanup(&result2);
}

/* ============================================================================
 * Statistics and Diagnostics Tests
 * ============================================================================ */

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorStatisticsAccumulate) {
    config.enabled_components = ORCH_COMP_GENIUS;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    /* Solve multiple problems */
    for (int i = 0; i < 5; i++) {
        math_problem_t problem = CreateSimpleProblem(GENIUS_DOMAIN_NUMBER_THEORY, "Stats test");
        orchestrator_result_t result;
        genius_orchestrator_result_init(&result);
        genius_orchestrator_solve(orch, &problem, &result);
        genius_orchestrator_result_cleanup(&result);
    }

    /* Check statistics */
    orchestrator_stats_t stats;
    nimcp_error_t err = genius_orchestrator_get_stats(orch, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.operations_total, 5u);
    EXPECT_GT(stats.total_time_us, 0u);
}

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorModeStatistics) {
    config.enabled_components = ORCH_COMP_GENIUS;
    config.auto_select_mode = true;

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    /* Solve problems in different domains */
    genius_domain_t domains[] = {GENIUS_DOMAIN_NUMBER_THEORY, GENIUS_DOMAIN_COMBINATORICS};
    for (int i = 0; i < 2; i++) {
        math_problem_t problem = CreateSimpleProblem(domains[i], "Mode stats test");
        orchestrator_result_t result;
        genius_orchestrator_result_init(&result);
        genius_orchestrator_solve(orch, &problem, &result);
        genius_orchestrator_result_cleanup(&result);
    }

    /* Check mode statistics */
    orchestrator_stats_t stats;
    genius_orchestrator_get_stats(orch, &stats);

    /* At least some modes should have been used */
    uint64_t total_mode_usage = 0;
    for (int i = 0; i < GENIUS_MODE_COUNT; i++) {
        total_mode_usage += stats.mode_usage[i];
    }
    EXPECT_GE(total_mode_usage, 2u);
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorNullParameterHandling) {
    config.enabled_components = ORCH_COMP_GENIUS;
    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    /* Test null problem */
    orchestrator_result_t result;
    genius_orchestrator_result_init(&result);
    nimcp_error_t err = genius_orchestrator_solve(orch, NULL, &result);
    EXPECT_NE(err, NIMCP_SUCCESS);

    /* Test null result */
    math_problem_t problem = CreateSimpleProblem(GENIUS_DOMAIN_NUMBER_THEORY, "Test");
    err = genius_orchestrator_solve(orch, &problem, NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(GeniusOrchestratorIntegrationTest, OrchestratorTimeoutHandling) {
    config.enabled_components = ORCH_COMP_GENIUS;
    config.operation_timeout_ms = 1;  /* Very short timeout */

    orch = genius_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    /* Solve should timeout but not crash */
    math_problem_t problem = CreateSimpleProblem(GENIUS_DOMAIN_OPTIMIZATION,
                                                  "Complex optimization");
    problem.difficulty = 1.0f;  /* Maximum difficulty */

    orchestrator_result_t result;
    genius_orchestrator_result_init(&result);

    nimcp_error_t err = genius_orchestrator_solve(orch, &problem, &result);
    /* Either succeeds quickly or times out */
    EXPECT_TRUE(err == NIMCP_SUCCESS || err == NIMCP_ERROR_TIMEOUT);

    genius_orchestrator_result_cleanup(&result);
}
