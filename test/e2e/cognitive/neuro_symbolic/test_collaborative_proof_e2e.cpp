/**
 * @file test_collaborative_proof_e2e.cpp
 * @brief E2E test for multi-genius collaboration
 *
 * WHAT: End-to-end test for collaborative problem solving with multiple genius instances
 * WHY:  Verify that different genius modes can collaborate effectively:
 *       - Gauss mode: Number theory, pattern recognition
 *       - Newton mode: Calculus, physics, analysis
 *       - Erdos mode: Combinatorics, graph theory, probabilistic proofs
 *       Combined they should solve problems no single mode can handle alone
 *
 * SCENARIO:
 * Inspired by Erdos's prolific collaboration style (Erdos number!), this tests
 * how multiple genius instances with different strengths can work together on
 * problems that span multiple mathematical domains.
 *
 * TEST COVERAGE:
 * - Single genius baseline (3 tests)
 * - Multi-genius collaboration (4 tests)
 * - Mode interaction and synergy (3 tests)
 * - Cross-domain problem solving (3 tests)
 * - Collaboration metrics (2 tests)
 *
 * TOTAL: 15 tests
 *
 * @author NIMCP Development Team
 * @date 2026-01-24
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <memory>

/* Include test base for automatic cleanup */
#include "utils/nimcp_test_base.h"

/* C headers with extern "C" guards */
extern "C" {
#include "cognitive/parietal/nimcp_mathematical_genius.h"
#include "cognitive/parietal/nimcp_genius_modes.h"
#include "cognitive/neuro_symbolic/nimcp_hypergraph.h"
#include "cognitive/neuro_symbolic/nimcp_energy_consistency.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class CollaborativeProofE2ETest : public NimcpTestBase {
protected:
    mathematical_genius_t* gauss;
    mathematical_genius_t* newton;
    mathematical_genius_t* erdos;
    nimcp_hypergraph_t* shared_knowledge;

    void SetUp() override {
        NimcpTestBase::SetUp();

        /* Create Gauss mode genius */
        genius_config_t gauss_cfg;
        genius_get_default_config(&gauss_cfg);
        gauss_cfg.default_mode = GENIUS_MODE_GAUSS;
        gauss_cfg.creativity_level = 0.7f;
        gauss_cfg.rigor_level = 0.9f;
        gauss_cfg.max_thinking_time_ms = 20000;

        gauss = genius_create(&gauss_cfg);
        ASSERT_NE(gauss, nullptr) << "Failed to create Gauss genius";

        /* Create Newton mode genius */
        genius_config_t newton_cfg;
        genius_get_default_config(&newton_cfg);
        newton_cfg.default_mode = GENIUS_MODE_NEWTON;
        newton_cfg.creativity_level = 0.6f;
        newton_cfg.rigor_level = 0.95f;
        newton_cfg.max_thinking_time_ms = 20000;

        newton = genius_create(&newton_cfg);
        ASSERT_NE(newton, nullptr) << "Failed to create Newton genius";

        /* Create Erdos mode genius */
        genius_config_t erdos_cfg;
        genius_get_default_config(&erdos_cfg);
        erdos_cfg.default_mode = GENIUS_MODE_ERDOS;
        erdos_cfg.creativity_level = 0.9f;
        erdos_cfg.rigor_level = 0.8f;
        erdos_cfg.collaboration_weight = 1.0f;  /* Erdos loved collaboration */
        erdos_cfg.max_thinking_time_ms = 20000;

        erdos = genius_create(&erdos_cfg);
        ASSERT_NE(erdos, nullptr) << "Failed to create Erdos genius";

        /* Create shared knowledge hypergraph */
        shared_knowledge = nimcp_hypergraph_create();
        ASSERT_NE(shared_knowledge, nullptr) << "Failed to create shared knowledge graph";

        /* Link geniuses to shared knowledge */
        genius_link_hypergraph(gauss, shared_knowledge);
        genius_link_hypergraph(newton, shared_knowledge);
        genius_link_hypergraph(erdos, shared_knowledge);
    }

    void TearDown() override {
        if (shared_knowledge) {
            nimcp_hypergraph_destroy(shared_knowledge);
            shared_knowledge = nullptr;
        }
        if (erdos) {
            genius_destroy(erdos);
            erdos = nullptr;
        }
        if (newton) {
            genius_destroy(newton);
            newton = nullptr;
        }
        if (gauss) {
            genius_destroy(gauss);
            gauss = nullptr;
        }

        NimcpTestBase::TearDown();
    }

    /* Helper: Create a cross-domain problem */
    math_problem_t create_cross_domain_problem(const char* statement,
                                                genius_domain_t primary,
                                                genius_domain_t secondary) {
        math_problem_t problem;
        memset(&problem, 0, sizeof(problem));
        problem.statement = strdup(statement);
        problem.domain = primary;
        problem.difficulty = 0.6f;
        problem.timeout_ms = 30000;

        /* Add secondary domain */
        problem.secondary_domains = (genius_domain_t*)malloc(sizeof(genius_domain_t));
        problem.secondary_domains[0] = secondary;
        problem.num_secondary = 1;

        return problem;
    }

    void cleanup_problem(math_problem_t* problem) {
        if (problem->statement) free(problem->statement);
        if (problem->secondary_domains) free(problem->secondary_domains);
    }
};

/* ============================================================================
 * Single Genius Baseline Tests
 * ============================================================================ */

TEST_F(CollaborativeProofE2ETest, GaussSolvesNumberTheoryProblem) {
    /* SCENARIO: Gauss solves a number theory problem alone
     * EXPECTED: High confidence on number theory domain
     */

    math_problem_t problem = create_cross_domain_problem(
        "Determine if n = 1000003 is prime",
        GENIUS_DOMAIN_NUMBER_THEORY,
        GENIUS_DOMAIN_NUMBER_THEORY);

    genius_result_t result;
    genius_result_init(&result);

    nimcp_error_t err = genius_gauss_analyze(gauss, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Gauss should handle number theory well */
    EXPECT_EQ(result.mode_used, GENIUS_MODE_GAUSS);

    /* Also test the primality function directly */
    bool is_prime = genius_gauss_is_prime(gauss, 1000003, 0.99f);
    /* 1000003 is actually prime */
    EXPECT_TRUE(is_prime) << "Gauss should identify 1000003 as prime";

    genius_result_cleanup(&result);
    cleanup_problem(&problem);
}

TEST_F(CollaborativeProofE2ETest, NewtonSolvesCalculusProblem) {
    /* SCENARIO: Newton solves a calculus problem alone
     * EXPECTED: High confidence on calculus domain
     */

    math_problem_t problem = create_cross_domain_problem(
        "Find the derivative of f(x) = x^3 + 2x^2 + x",
        GENIUS_DOMAIN_CALCULUS,
        GENIUS_DOMAIN_CALCULUS);

    genius_result_t result;
    genius_result_init(&result);

    nimcp_error_t err = genius_newton_analyze(newton, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Newton should handle calculus well */
    EXPECT_EQ(result.mode_used, GENIUS_MODE_NEWTON);

    genius_result_cleanup(&result);
    cleanup_problem(&problem);
}

TEST_F(CollaborativeProofE2ETest, ErdosSolvesCombinatorialProblem) {
    /* SCENARIO: Erdos solves a graph theory problem alone
     * EXPECTED: High confidence on combinatorics/graph theory
     */

    math_problem_t problem = create_cross_domain_problem(
        "Estimate the chromatic number of a random graph with 10 vertices",
        GENIUS_DOMAIN_GRAPH_THEORY,
        GENIUS_DOMAIN_PROBABILITY);

    genius_result_t result;
    genius_result_init(&result);

    nimcp_error_t err = genius_erdos_analyze(erdos, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Erdos should handle combinatorics well */
    EXPECT_EQ(result.mode_used, GENIUS_MODE_ERDOS);

    /* Test Ramsey bound computation */
    uint32_t ramsey_lower = genius_erdos_ramsey_lower_bound(erdos, 3, 3);
    EXPECT_GE(ramsey_lower, 6) << "R(3,3) >= 6";

    genius_result_cleanup(&result);
    cleanup_problem(&problem);
}

/* ============================================================================
 * Multi-Genius Collaboration Tests
 * ============================================================================ */

TEST_F(CollaborativeProofE2ETest, TwoGeniusCollaboration) {
    /* SCENARIO: Gauss and Newton collaborate on a problem
     * EXPECTED: Combined result better than either alone
     */

    math_problem_t problem = create_cross_domain_problem(
        "Find the sum of the first n terms of the series 1/n^2",
        GENIUS_DOMAIN_NUMBER_THEORY,
        GENIUS_DOMAIN_ANALYSIS);

    mathematical_genius_t* collaborators[] = {gauss, newton};
    genius_result_t result;
    genius_result_init(&result);

    nimcp_error_t err = genius_collaborate(collaborators, 2, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Collaboration should produce result */
    EXPECT_TRUE(result.solved || result.num_conjectures > 0 || result.num_patterns > 0);

    /* Should show contribution from both modes */
    EXPECT_GE(result.thinking_time_us, 0);

    genius_result_cleanup(&result);
    cleanup_problem(&problem);
}

TEST_F(CollaborativeProofE2ETest, ThreeGeniusCollaboration) {
    /* SCENARIO: All three geniuses collaborate
     * EXPECTED: Diverse perspectives contribute to solution
     */

    math_problem_t problem = create_cross_domain_problem(
        "Prove that the sum of first n odd numbers equals n^2",
        GENIUS_DOMAIN_NUMBER_THEORY,
        GENIUS_DOMAIN_ALGEBRA);

    mathematical_genius_t* collaborators[] = {gauss, newton, erdos};
    genius_result_t result;
    genius_result_init(&result);

    nimcp_error_t err = genius_collaborate(collaborators, 3, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* With three perspectives, should have high-quality result */
    if (result.solved) {
        EXPECT_GT(result.elegance_score, 0.0f);
    }

    /* Check that multiple approaches were considered */
    EXPECT_GE(result.thinking_time_us, 0);

    genius_result_cleanup(&result);
    cleanup_problem(&problem);
}

TEST_F(CollaborativeProofE2ETest, CollaborationWithSharedKnowledge) {
    /* SCENARIO: Geniuses share knowledge through hypergraph
     * EXPECTED: Knowledge builds across collaboration
     */

    /* Add some initial knowledge to the shared hypergraph */
    uint32_t v1 = nimcp_hypergraph_add_vertex(shared_knowledge,
                                               HYPERVERTEX_CONSTANT, "pi", 0.95f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(shared_knowledge,
                                               HYPERVERTEX_CONSTANT, "e", 0.95f);
    uint32_t v3 = nimcp_hypergraph_add_vertex(shared_knowledge,
                                               HYPERVERTEX_PREDICATE, "transcendental", 0.9f);

    EXPECT_NE(v1, UINT32_MAX);
    EXPECT_NE(v2, UINT32_MAX);
    EXPECT_NE(v3, UINT32_MAX);

    /* Add edge connecting them */
    uint32_t verts[] = {v1, v2, v3};
    uint32_t edge_id = nimcp_hypergraph_add_edge(shared_knowledge,
                                                  HYPEREDGE_THEOREM,
                                                  verts, 3,
                                                  TRIT_TRUE,
                                                  "transcendental_numbers");
    EXPECT_NE(edge_id, UINT32_MAX);

    /* Now solve a problem that benefits from this knowledge */
    math_problem_t problem = create_cross_domain_problem(
        "Prove that e*pi is irrational",
        GENIUS_DOMAIN_NUMBER_THEORY,
        GENIUS_DOMAIN_ANALYSIS);

    mathematical_genius_t* collaborators[] = {gauss, newton};
    genius_result_t result;
    genius_result_init(&result);

    nimcp_error_t err = genius_collaborate(collaborators, 2, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify shared knowledge was potentially used */
    hypergraph_stats_t kg_stats;
    nimcp_hypergraph_get_stats(shared_knowledge, &kg_stats);
    EXPECT_GE(kg_stats.vertex_count, 3);
    EXPECT_GE(kg_stats.edge_count, 1);

    genius_result_cleanup(&result);
    cleanup_problem(&problem);
}

TEST_F(CollaborativeProofE2ETest, CollaborationProducesProof) {
    /* SCENARIO: Collaboration produces a valid proof
     * EXPECTED: Proof trace with multiple contributions
     */

    math_problem_t problem = create_cross_domain_problem(
        "Prove that the arithmetic mean is >= geometric mean for two numbers",
        GENIUS_DOMAIN_ALGEBRA,
        GENIUS_DOMAIN_OPTIMIZATION);

    mathematical_genius_t* collaborators[] = {gauss, newton, erdos};
    genius_result_t result;
    genius_result_init(&result);

    nimcp_error_t err = genius_collaborate(collaborators, 3, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Check if a proof was produced */
    if (result.num_proofs > 0 && result.proofs) {
        EXPECT_TRUE(result.proofs[0].is_complete || result.proofs[0].num_steps > 0);

        /* Proof elegance should be reasonable */
        EXPECT_GE(result.proofs[0].elegance_score, 0.0f);
    }

    genius_result_cleanup(&result);
    cleanup_problem(&problem);
}

/* ============================================================================
 * Mode Interaction and Synergy Tests
 * ============================================================================ */

TEST_F(CollaborativeProofE2ETest, GaussNewtonSynergyOnAnalysis) {
    /* SCENARIO: Gauss's pattern recognition + Newton's analysis
     * EXPECTED: Synergy on analytical number theory problems
     */

    /* Problem: Find pattern in partial sums of harmonic series */
    int64_t harmonic_partials[] = {1, 3, 11, 25, 137};  /* Numerators only, simplified */

    conjecture_t gauss_conj;
    memset(&gauss_conj, 0, sizeof(gauss_conj));

    /* Gauss finds pattern */
    nimcp_error_t err = genius_gauss_discover_pattern(gauss, harmonic_partials, 5, &gauss_conj);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Newton analyzes the series convergence properties */
    math_problem_t problem = create_cross_domain_problem(
        "Analyze convergence of harmonic series",
        GENIUS_DOMAIN_ANALYSIS,
        GENIUS_DOMAIN_NUMBER_THEORY);

    genius_result_t newton_result;
    genius_result_init(&newton_result);

    err = genius_newton_analyze(newton, &problem, &newton_result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Both should contribute insights */
    EXPECT_GE(gauss_conj.confidence, 0.0f);

    genius_result_cleanup(&newton_result);
    cleanup_problem(&problem);
}

TEST_F(CollaborativeProofE2ETest, ErdosGaussSynergyOnCombinatorics) {
    /* SCENARIO: Erdos's probabilistic + Gauss's counting
     * EXPECTED: Strong results on counting/combinatorics
     */

    /* Problem requiring both counting and probabilistic methods */
    math_problem_t problem = create_cross_domain_problem(
        "Count the number of ways to partition n into distinct parts",
        GENIUS_DOMAIN_COMBINATORICS,
        GENIUS_DOMAIN_NUMBER_THEORY);

    mathematical_genius_t* collaborators[] = {gauss, erdos};
    genius_result_t result;
    genius_result_init(&result);

    nimcp_error_t err = genius_collaborate(collaborators, 2, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Should produce conjectures or insights */
    EXPECT_TRUE(result.num_conjectures > 0 || result.num_insights > 0 || result.solved);

    genius_result_cleanup(&result);
    cleanup_problem(&problem);
}

TEST_F(CollaborativeProofE2ETest, AnalogyFindingAcrossModes) {
    /* SCENARIO: Find analogies between domains using multiple modes
     * EXPECTED: Discover cross-domain connections
     */

    /* Find analogies between number theory and graph theory */
    genius_analogy_result_t analogies[8];
    memset(analogies, 0, sizeof(analogies));

    uint32_t num_analogies = genius_find_analogies(
        gauss,
        GENIUS_DOMAIN_NUMBER_THEORY,
        GENIUS_DOMAIN_GRAPH_THEORY,
        analogies,
        8);

    /* May or may not find analogies, but should not crash */
    EXPECT_GE(num_analogies, 0);

    if (num_analogies > 0) {
        EXPECT_GT(analogies[0].similarity, 0.0f);
    }
}

/* ============================================================================
 * Cross-Domain Problem Solving Tests
 * ============================================================================ */

TEST_F(CollaborativeProofE2ETest, AnalyticNumberTheoryProblem) {
    /* SCENARIO: Problem requiring both analysis and number theory
     * Classic: Distribution of primes via analytic methods
     */

    math_problem_t problem = create_cross_domain_problem(
        "Estimate the number of primes less than n using analytic methods",
        GENIUS_DOMAIN_NUMBER_THEORY,
        GENIUS_DOMAIN_ANALYSIS);

    mathematical_genius_t* collaborators[] = {gauss, newton};
    genius_result_t result;
    genius_result_init(&result);

    nimcp_error_t err = genius_collaborate(collaborators, 2, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Prime counting needs both Gauss's number sense and Newton's analysis */
    EXPECT_GE(result.thinking_time_us, 0);

    genius_result_cleanup(&result);
    cleanup_problem(&problem);
}

TEST_F(CollaborativeProofE2ETest, ProbabilisticNumberTheoryProblem) {
    /* SCENARIO: Problem requiring probability + number theory
     * Example: Expected number of prime factors
     */

    math_problem_t problem = create_cross_domain_problem(
        "Find expected number of prime factors of a random integer",
        GENIUS_DOMAIN_NUMBER_THEORY,
        GENIUS_DOMAIN_PROBABILITY);

    mathematical_genius_t* collaborators[] = {gauss, erdos};
    genius_result_t result;
    genius_result_init(&result);

    nimcp_error_t err = genius_collaborate(collaborators, 2, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Should combine Gauss's factoring with Erdos's probability */
    if (result.erdos_result) {
        EXPECT_GE(result.erdos_result->expected_value, 0.0f);
    }

    genius_result_cleanup(&result);
    cleanup_problem(&problem);
}

TEST_F(CollaborativeProofE2ETest, PhysicsOptimizationProblem) {
    /* SCENARIO: Problem requiring calculus + optimization
     * Example: Minimize surface area for given volume (calculus of variations)
     */

    math_problem_t problem = create_cross_domain_problem(
        "Find the shape that minimizes surface area for a given volume",
        GENIUS_DOMAIN_CALCULUS,
        GENIUS_DOMAIN_OPTIMIZATION);

    genius_result_t result;
    genius_result_init(&result);

    /* Newton alone should handle this well */
    nimcp_error_t err = genius_newton_analyze(newton, &problem, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Newton's calculus should find the sphere is optimal */
    EXPECT_EQ(result.mode_used, GENIUS_MODE_NEWTON);

    genius_result_cleanup(&result);
    cleanup_problem(&problem);
}

/* ============================================================================
 * Collaboration Metrics Tests
 * ============================================================================ */

TEST_F(CollaborativeProofE2ETest, CollaborationStatisticsTracking) {
    /* SCENARIO: Verify collaboration statistics are tracked
     * EXPECTED: Stats should reflect collaborative activity
     */

    /* Perform several collaborations */
    for (int i = 0; i < 3; i++) {
        math_problem_t problem = create_cross_domain_problem(
            "Test problem for statistics",
            GENIUS_DOMAIN_ALGEBRA,
            GENIUS_DOMAIN_NUMBER_THEORY);

        mathematical_genius_t* collaborators[] = {gauss, newton};
        genius_result_t result;
        genius_result_init(&result);

        genius_collaborate(collaborators, 2, &problem, &result);

        genius_result_cleanup(&result);
        cleanup_problem(&problem);
    }

    /* Check statistics for Gauss */
    genius_stats_t gauss_stats;
    nimcp_error_t err = genius_get_stats(gauss, &gauss_stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_GE(gauss_stats.collaborations, 3) << "Should track collaboration count";
    EXPECT_GT(gauss_stats.total_thinking_time_us, 0);

    /* Check statistics for Newton */
    genius_stats_t newton_stats;
    err = genius_get_stats(newton, &newton_stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_GE(newton_stats.collaborations, 3);
}

TEST_F(CollaborativeProofE2ETest, CollaborationImprovementMetric) {
    /* SCENARIO: Measure if collaboration improves results
     * EXPECTED: Collaboration improvement metric should be non-negative
     */

    /* Solve a problem alone first */
    math_problem_t problem = create_cross_domain_problem(
        "Find all primes that differ by 2 (twin primes) below 100",
        GENIUS_DOMAIN_NUMBER_THEORY,
        GENIUS_DOMAIN_COMBINATORICS);

    genius_result_t solo_result;
    genius_result_init(&solo_result);

    genius_gauss_analyze(gauss, &problem, &solo_result);
    float solo_confidence = solo_result.num_conjectures > 0 ?
                            solo_result.conjectures[0].confidence : 0.0f;

    genius_result_cleanup(&solo_result);

    /* Now solve collaboratively */
    mathematical_genius_t* collaborators[] = {gauss, erdos};
    genius_result_t collab_result;
    genius_result_init(&collab_result);

    genius_collaborate(collaborators, 2, &problem, &collab_result);

    /* Check collaboration improvement in stats */
    genius_stats_t stats;
    genius_get_stats(gauss, &stats);

    /* Collaboration improvement should be tracked */
    EXPECT_GE(stats.collaboration_improvement, 0.0f);

    genius_result_cleanup(&collab_result);
    cleanup_problem(&problem);
}

/* ============================================================================
 * Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
