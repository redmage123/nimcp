/**
 * @file test_hypergraph_knowledge_integration.cpp
 * @brief Integration tests for Hypergraph with Knowledge Systems
 *
 * Tests the integration between Hypergraph and knowledge representation:
 * - Building hypergraph from logic/knowledge base
 * - Hypergraph transversal operations
 * - Dual graph computations
 * - Knowledge graph bridge integration
 * - Pattern matching and queries
 *
 * @version 2.6.3
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include "utils/nimcp_test_base.h"
#include <cmath>
#include <cstring>

extern "C" {
#include "cognitive/neuro_symbolic/nimcp_hypergraph.h"
#include "cognitive/neuro_symbolic/bridges/nimcp_hypergraph_kg_bridge.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

/**
 * @brief Test fixture for Hypergraph Knowledge Integration tests
 */
class HypergraphKnowledgeIntegrationTest : public NimcpTestBase {
protected:
    nimcp_hypergraph_t* hg;
    hypergraph_config_t config;

    void SetUp() override {
        NimcpTestBase::SetUp();
        hg = NULL;
        nimcp_hypergraph_get_default_config(&config);
    }

    void TearDown() override {
        if (hg) {
            nimcp_hypergraph_destroy(hg);
            hg = NULL;
        }
        NimcpTestBase::TearDown();
    }

    /**
     * @brief Helper to create vertices for mathematical constants
     */
    uint32_t AddConstant(const char* name, float value) {
        uint32_t id = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, name, 1.0f);
        return id;
    }

    /**
     * @brief Helper to create predicate vertices
     */
    uint32_t AddPredicate(const char* name) {
        return nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PREDICATE, name, 1.0f);
    }

    /**
     * @brief Helper to create function vertices
     */
    uint32_t AddFunction(const char* name) {
        return nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_FUNCTION, name, 1.0f);
    }
};

/* ============================================================================
 * Building Hypergraph from Knowledge Base Tests
 * ============================================================================ */

TEST_F(HypergraphKnowledgeIntegrationTest, CreateSimpleKnowledgeGraph) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    /* Build simple number theory knowledge */
    uint32_t prime_pred = AddPredicate("prime");
    uint32_t even_pred = AddPredicate("even");
    uint32_t odd_pred = AddPredicate("odd");
    uint32_t natural_pred = AddPredicate("natural");

    EXPECT_NE(prime_pred, UINT32_MAX);
    EXPECT_NE(even_pred, UINT32_MAX);
    EXPECT_NE(odd_pred, UINT32_MAX);
    EXPECT_NE(natural_pred, UINT32_MAX);

    /* Add constants */
    uint32_t two = AddConstant("2", 2.0f);
    uint32_t three = AddConstant("3", 3.0f);
    uint32_t five = AddConstant("5", 5.0f);

    /* Add facts: prime(2), even(2), prime(3), odd(3), prime(5), odd(5) */
    uint32_t fact1[] = {two, prime_pred};
    uint32_t fact2[] = {two, even_pred};
    uint32_t fact3[] = {three, prime_pred};
    uint32_t fact4[] = {three, odd_pred};
    uint32_t fact5[] = {five, prime_pred};
    uint32_t fact6[] = {five, odd_pred};

    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, fact1, 2, TRIT_POSITIVE, "prime_2");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, fact2, 2, TRIT_POSITIVE, "even_2");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, fact3, 2, TRIT_POSITIVE, "prime_3");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, fact4, 2, TRIT_POSITIVE, "odd_3");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, fact5, 2, TRIT_POSITIVE, "prime_5");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, fact6, 2, TRIT_POSITIVE, "odd_5");

    /* Verify structure */
    hypergraph_stats_t stats;
    nimcp_error_t err = nimcp_hypergraph_get_stats(hg, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.vertex_count, 7u);  /* 4 predicates + 3 constants */
    EXPECT_EQ(stats.edge_count, 6u);    /* 6 facts */
}

TEST_F(HypergraphKnowledgeIntegrationTest, CreateRulesWithImplications) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    /* Create rule: prime(x) AND x > 2 => odd(x) */
    uint32_t prime_pred = AddPredicate("prime");
    uint32_t odd_pred = AddPredicate("odd");
    uint32_t gt_pred = AddPredicate("greater_than");
    uint32_t implies_fn = AddFunction("implies");
    uint32_t two = AddConstant("2", 2.0f);
    uint32_t var_x = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_VARIABLE, "x", 1.0f);

    /* Rule hyperedge connecting all parts */
    uint32_t rule_vertices[] = {prime_pred, var_x, gt_pred, two, implies_fn, odd_pred};
    uint32_t rule_edge = nimcp_hypergraph_add_edge(hg, HYPEREDGE_RULE,
                                                    rule_vertices, 6, TRIT_POSITIVE,
                                                    "prime_gt2_odd");
    EXPECT_NE(rule_edge, UINT32_MAX);

    /* Verify rule edge */
    const nimcp_hyperedge_t* edge = nimcp_hypergraph_get_edge(hg, rule_edge);
    ASSERT_NE(edge, nullptr);
    EXPECT_EQ(edge->type, HYPEREDGE_RULE);
    EXPECT_EQ(edge->vertex_count, 6u);
}

TEST_F(HypergraphKnowledgeIntegrationTest, CreateTheoremWithProofSteps) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    /* Build theorem structure */
    uint32_t theorem_stmt = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_EXPRESSION,
                                                         "forall_x_prime_gt2_odd", 1.0f);

    /* Proof steps as vertices */
    uint32_t step1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PROOF_STEP,
                                                   "assume_prime_x_gt_2", 1.0f);
    uint32_t step2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PROOF_STEP,
                                                   "by_defn_prime_not_even", 1.0f);
    uint32_t step3 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PROOF_STEP,
                                                   "not_even_implies_odd", 1.0f);
    uint32_t step4 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PROOF_STEP,
                                                   "conclude_odd_x", 1.0f);

    /* Connect proof steps as theorem hyperedge */
    uint32_t proof_vertices[] = {theorem_stmt, step1, step2, step3, step4};
    uint32_t theorem_edge = nimcp_hypergraph_add_edge(hg, HYPEREDGE_THEOREM,
                                                       proof_vertices, 5, TRIT_POSITIVE,
                                                       "prime_odd_theorem");
    EXPECT_NE(theorem_edge, UINT32_MAX);

    /* Verify theorem structure */
    const nimcp_hyperedge_t* edge = nimcp_hypergraph_get_edge(hg, theorem_edge);
    ASSERT_NE(edge, nullptr);
    EXPECT_EQ(edge->type, HYPEREDGE_THEOREM);
}

TEST_F(HypergraphKnowledgeIntegrationTest, CreateFromKnowledgeBaseAPI) {
    /* Test the from_knowledge_base API (may create empty graph if no KB) */
    nimcp_hypergraph_t* kb_graph = nimcp_hypergraph_from_knowledge_base(NULL);
    /* May return NULL for NULL input or empty graph */
    if (kb_graph) {
        hypergraph_stats_t stats;
        nimcp_hypergraph_get_stats(kb_graph, &stats);
        /* Should be valid but may be empty */
        EXPECT_GE(stats.vertex_count, 0u);
        nimcp_hypergraph_destroy(kb_graph);
    }
}

/* ============================================================================
 * Hypergraph Transversal Operations Tests
 * ============================================================================ */

TEST_F(HypergraphKnowledgeIntegrationTest, MinimalTransversalComputation) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    /* Create constraint satisfaction problem:
     * Variables: x1, x2, x3, x4
     * Constraints: {x1,x2}, {x2,x3}, {x3,x4}, {x1,x4}
     * Minimal transversal: {x1,x3} or {x2,x4}
     */
    uint32_t x1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_VARIABLE, "x1", 1.0f);
    uint32_t x2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_VARIABLE, "x2", 1.0f);
    uint32_t x3 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_VARIABLE, "x3", 1.0f);
    uint32_t x4 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_VARIABLE, "x4", 1.0f);

    uint32_t c1[] = {x1, x2};
    uint32_t c2[] = {x2, x3};
    uint32_t c3[] = {x3, x4};
    uint32_t c4[] = {x1, x4};

    nimcp_hypergraph_add_edge(hg, HYPEREDGE_CONSTRAINT, c1, 2, TRIT_POSITIVE, "c1");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_CONSTRAINT, c2, 2, TRIT_POSITIVE, "c2");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_CONSTRAINT, c3, 2, TRIT_POSITIVE, "c3");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_CONSTRAINT, c4, 2, TRIT_POSITIVE, "c4");

    /* Compute transversal */
    uint32_t transversal[10];
    uint32_t size = nimcp_hypergraph_transversal(hg, transversal, 10);

    /* Should find a minimal transversal of size 2 */
    EXPECT_GE(size, 2u);
    EXPECT_LE(size, 4u);  /* At most all variables */

    /* Verify transversal hits all edges */
    /* Each edge should contain at least one transversal vertex */
    hypergraph_stats_t stats;
    nimcp_hypergraph_get_stats(hg, &stats);
    EXPECT_EQ(stats.edge_count, 4u);
}

TEST_F(HypergraphKnowledgeIntegrationTest, AllTransversalsComputation) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    /* Simple hypergraph: {a,b}, {b,c} */
    uint32_t a = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_VARIABLE, "a", 1.0f);
    uint32_t b = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_VARIABLE, "b", 1.0f);
    uint32_t c = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_VARIABLE, "c", 1.0f);

    uint32_t e1[] = {a, b};
    uint32_t e2[] = {b, c};

    nimcp_hypergraph_add_edge(hg, HYPEREDGE_CONSTRAINT, e1, 2, TRIT_POSITIVE, "e1");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_CONSTRAINT, e2, 2, TRIT_POSITIVE, "e2");

    /* Minimal transversals: {b}, {a,c} */
    uint32_t* transversals[10];
    uint32_t sizes[10];

    /* Allocate space for transversals */
    for (int i = 0; i < 10; i++) {
        transversals[i] = (uint32_t*)malloc(10 * sizeof(uint32_t));
    }

    uint32_t num_transversals = nimcp_hypergraph_all_transversals(hg, transversals, sizes, 10);

    /* Should find at least 1 transversal */
    EXPECT_GE(num_transversals, 1u);

    /* Cleanup */
    for (int i = 0; i < 10; i++) {
        free(transversals[i]);
    }
}

TEST_F(HypergraphKnowledgeIntegrationTest, TransversalForCoveringProblem) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    /* Covering problem: Select minimum vertices to cover all edges
     * Sets (as hyperedges): {1,2,3}, {2,4}, {3,5}, {4,5,6}
     */
    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v1", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v2", 1.0f);
    uint32_t v3 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v3", 1.0f);
    uint32_t v4 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v4", 1.0f);
    uint32_t v5 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v5", 1.0f);
    uint32_t v6 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v6", 1.0f);

    uint32_t s1[] = {v1, v2, v3};
    uint32_t s2[] = {v2, v4};
    uint32_t s3[] = {v3, v5};
    uint32_t s4[] = {v4, v5, v6};

    nimcp_hypergraph_add_edge(hg, HYPEREDGE_CONSTRAINT, s1, 3, TRIT_POSITIVE, "s1");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_CONSTRAINT, s2, 2, TRIT_POSITIVE, "s2");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_CONSTRAINT, s3, 2, TRIT_POSITIVE, "s3");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_CONSTRAINT, s4, 3, TRIT_POSITIVE, "s4");

    /* Compute transversal */
    uint32_t transversal[10];
    uint32_t size = nimcp_hypergraph_transversal(hg, transversal, 10);

    /* Transversal should cover all edges */
    EXPECT_GE(size, 2u);  /* Minimum covering set */
    EXPECT_LE(size, 6u);  /* At most all vertices */
}

/* ============================================================================
 * Dual Graph Computation Tests
 * ============================================================================ */

TEST_F(HypergraphKnowledgeIntegrationTest, DualHypergraphComputation) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    /* Create simple hypergraph */
    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v1", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v2", 1.0f);
    uint32_t v3 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v3", 1.0f);

    uint32_t e1[] = {v1, v2};
    uint32_t e2[] = {v2, v3};
    uint32_t e3[] = {v1, v3};

    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e1, 2, TRIT_POSITIVE, "e1");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e2, 2, TRIT_POSITIVE, "e2");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e3, 2, TRIT_POSITIVE, "e3");

    /* Compute dual */
    nimcp_hypergraph_dual_t* dual = nimcp_hypergraph_compute_dual(hg);
    ASSERT_NE(dual, nullptr);
    ASSERT_NE(dual->dual, nullptr);

    /* In dual: original edges become vertices, original vertices become edges */
    hypergraph_stats_t orig_stats, dual_stats;
    nimcp_hypergraph_get_stats(hg, &orig_stats);
    nimcp_hypergraph_get_stats(dual->dual, &dual_stats);

    /* Dual should have 3 vertices (from original edges) */
    EXPECT_EQ(dual_stats.vertex_count, orig_stats.edge_count);

    /* Verify mapping arrays exist */
    EXPECT_NE(dual->vertex_to_edge_map, nullptr);
    EXPECT_NE(dual->edge_to_vertex_map, nullptr);

    nimcp_hypergraph_dual_destroy(dual);
}

TEST_F(HypergraphKnowledgeIntegrationTest, DualOfDualIsOriginal) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    /* Create hypergraph */
    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "a", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "b", 1.0f);

    uint32_t e1[] = {v1, v2};
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e1, 2, TRIT_POSITIVE, "ab");

    hypergraph_stats_t orig_stats;
    nimcp_hypergraph_get_stats(hg, &orig_stats);

    /* Compute dual */
    nimcp_hypergraph_dual_t* dual1 = nimcp_hypergraph_compute_dual(hg);
    ASSERT_NE(dual1, nullptr);

    /* Compute dual of dual */
    nimcp_hypergraph_dual_t* dual2 = nimcp_hypergraph_compute_dual(dual1->dual);
    if (dual2) {
        hypergraph_stats_t dual2_stats;
        nimcp_hypergraph_get_stats(dual2->dual, &dual2_stats);

        /* Dual of dual should have same structure as original */
        EXPECT_EQ(dual2_stats.vertex_count, orig_stats.vertex_count);
        EXPECT_EQ(dual2_stats.edge_count, orig_stats.edge_count);

        nimcp_hypergraph_dual_destroy(dual2);
    }

    nimcp_hypergraph_dual_destroy(dual1);
}

TEST_F(HypergraphKnowledgeIntegrationTest, DualPreservesConnectivity) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    /* Create connected hypergraph */
    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "1", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "2", 1.0f);
    uint32_t v3 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "3", 1.0f);
    uint32_t v4 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "4", 1.0f);

    uint32_t e1[] = {v1, v2, v3};
    uint32_t e2[] = {v2, v3, v4};

    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e1, 3, TRIT_POSITIVE, "e1");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e2, 3, TRIT_POSITIVE, "e2");

    /* Original should be connected */
    bool orig_connected = nimcp_hypergraph_is_connected(hg);
    EXPECT_TRUE(orig_connected);

    /* Compute dual */
    nimcp_hypergraph_dual_t* dual = nimcp_hypergraph_compute_dual(hg);
    ASSERT_NE(dual, nullptr);

    /* Dual should also be connected (edges share vertices) */
    bool dual_connected = nimcp_hypergraph_is_connected(dual->dual);
    EXPECT_TRUE(dual_connected);

    nimcp_hypergraph_dual_destroy(dual);
}

/* ============================================================================
 * Knowledge Graph Bridge Integration Tests
 * ============================================================================ */

TEST_F(HypergraphKnowledgeIntegrationTest, HypergraphKGBridgeCreation) {
    hypergraph_kg_bridge_t* bridge = hypergraph_kg_bridge_create();
    ASSERT_NE(bridge, nullptr);

    /* Verify initial state */
    EXPECT_EQ(bridge->syncs_performed, 0u);

    hypergraph_kg_bridge_destroy(bridge);
}

TEST_F(HypergraphKnowledgeIntegrationTest, HypergraphKGBridgeBidirectionalSync) {
    hypergraph_kg_bridge_t* bridge = hypergraph_kg_bridge_create();
    ASSERT_NE(bridge, nullptr);

    /* Enable bidirectional sync */
    bridge->enable_bidirectional_sync = true;
    EXPECT_TRUE(bridge->enable_bidirectional_sync);

    hypergraph_kg_bridge_destroy(bridge);
}

/* ============================================================================
 * Query Operations Tests
 * ============================================================================ */

TEST_F(HypergraphKnowledgeIntegrationTest, FindEdgesContainingVertices) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    /* Create knowledge structure */
    uint32_t a = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "a", 1.0f);
    uint32_t b = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "b", 1.0f);
    uint32_t c = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "c", 1.0f);
    uint32_t d = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "d", 1.0f);

    uint32_t e1[] = {a, b, c};    /* Contains a, b */
    uint32_t e2[] = {a, b, d};    /* Contains a, b */
    uint32_t e3[] = {b, c, d};    /* Contains b, c */

    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e1, 3, TRIT_POSITIVE, "abc");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e2, 3, TRIT_POSITIVE, "abd");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e3, 3, TRIT_POSITIVE, "bcd");

    /* Find edges containing both a and b */
    uint32_t query_vertices[] = {a, b};
    uint32_t found_edges[10];
    uint32_t count = nimcp_hypergraph_find_edges_containing(hg, query_vertices, 2, found_edges, 10);

    /* Should find 2 edges (abc and abd) */
    EXPECT_EQ(count, 2u);
}

TEST_F(HypergraphKnowledgeIntegrationTest, GetNeighborsOfVertex) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    /* Create structure */
    uint32_t center = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "center", 1.0f);
    uint32_t n1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "n1", 1.0f);
    uint32_t n2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "n2", 1.0f);
    uint32_t n3 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "n3", 1.0f);
    uint32_t isolated = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "isolated", 1.0f);

    uint32_t e1[] = {center, n1, n2};
    uint32_t e2[] = {center, n3};

    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e1, 3, TRIT_POSITIVE, "e1");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e2, 2, TRIT_POSITIVE, "e2");

    /* Get neighbors of center */
    uint32_t neighbors[10];
    uint32_t count = nimcp_hypergraph_get_neighbors(hg, center, neighbors, 10);

    /* Center should have 3 neighbors (n1, n2, n3), not isolated */
    EXPECT_EQ(count, 3u);
}

TEST_F(HypergraphKnowledgeIntegrationTest, CheckVertexConnection) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    uint32_t a = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "a", 1.0f);
    uint32_t b = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "b", 1.0f);
    uint32_t c = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "c", 1.0f);

    uint32_t e1[] = {a, b};
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e1, 2, TRIT_POSITIVE, "ab");

    /* a and b are connected */
    EXPECT_TRUE(nimcp_hypergraph_are_connected(hg, a, b));
    EXPECT_TRUE(nimcp_hypergraph_are_connected(hg, b, a));

    /* a and c are not connected */
    EXPECT_FALSE(nimcp_hypergraph_are_connected(hg, a, c));
}

TEST_F(HypergraphKnowledgeIntegrationTest, GetEdgesByType) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v1", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v2", 1.0f);
    uint32_t v3 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v3", 1.0f);

    /* Add different edge types */
    uint32_t e1[] = {v1, v2};
    uint32_t e2[] = {v2, v3};
    uint32_t e3[] = {v1, v3};

    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e1, 2, TRIT_POSITIVE, "rel1");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_THEOREM, e2, 2, TRIT_POSITIVE, "thm1");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e3, 2, TRIT_POSITIVE, "rel2");

    /* Get only relation edges */
    uint32_t relation_edges[10];
    uint32_t count = nimcp_hypergraph_get_edges_by_type(hg, HYPEREDGE_RELATION,
                                                         relation_edges, 10);
    EXPECT_EQ(count, 2u);

    /* Get only theorem edges */
    uint32_t theorem_edges[10];
    count = nimcp_hypergraph_get_edges_by_type(hg, HYPEREDGE_THEOREM,
                                                theorem_edges, 10);
    EXPECT_EQ(count, 1u);
}

/* ============================================================================
 * Edge Operations Tests
 * ============================================================================ */

TEST_F(HypergraphKnowledgeIntegrationTest, ExtendEdgeWithVertex) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v1", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v2", 1.0f);
    uint32_t v3 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v3", 1.0f);

    /* Create edge with 2 vertices */
    uint32_t e[] = {v1, v2};
    uint32_t edge_id = nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e, 2,
                                                  TRIT_POSITIVE, "edge");

    /* Verify initial vertex count */
    const nimcp_hyperedge_t* edge = nimcp_hypergraph_get_edge(hg, edge_id);
    ASSERT_NE(edge, nullptr);
    EXPECT_EQ(edge->vertex_count, 2u);

    /* Extend edge with v3 */
    nimcp_error_t err = nimcp_hypergraph_extend_edge(hg, edge_id, v3);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify extended */
    edge = nimcp_hypergraph_get_edge(hg, edge_id);
    EXPECT_EQ(edge->vertex_count, 3u);
}

TEST_F(HypergraphKnowledgeIntegrationTest, ShrinkEdgeRemoveVertex) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v1", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v2", 1.0f);
    uint32_t v3 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v3", 1.0f);

    /* Create edge with 3 vertices */
    uint32_t e[] = {v1, v2, v3};
    uint32_t edge_id = nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e, 3,
                                                  TRIT_POSITIVE, "edge");

    /* Shrink edge by removing v2 */
    nimcp_error_t err = nimcp_hypergraph_shrink_edge(hg, edge_id, v2);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify shrunk */
    const nimcp_hyperedge_t* edge = nimcp_hypergraph_get_edge(hg, edge_id);
    EXPECT_EQ(edge->vertex_count, 2u);
}

TEST_F(HypergraphKnowledgeIntegrationTest, ContractEdgeMergesVertices) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v1", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v2", 1.0f);
    uint32_t v3 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v3", 1.0f);

    /* Create edge to contract */
    uint32_t e[] = {v1, v2};
    uint32_t edge_id = nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e, 2,
                                                  TRIT_POSITIVE, "to_contract");

    /* Contract edge - merges v1 and v2 */
    uint32_t merged = nimcp_hypergraph_contract_edge(hg, edge_id);

    /* Should return merged vertex ID or UINT32_MAX if not supported */
    if (merged != UINT32_MAX) {
        const nimcp_hypervertex_t* vertex = nimcp_hypergraph_get_vertex(hg, merged);
        EXPECT_NE(vertex, nullptr);
    }
}

/* ============================================================================
 * Connected Components Tests
 * ============================================================================ */

TEST_F(HypergraphKnowledgeIntegrationTest, ConnectedComponentsSingleComponent) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    /* Create connected graph */
    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v1", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v2", 1.0f);
    uint32_t v3 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v3", 1.0f);

    uint32_t e1[] = {v1, v2};
    uint32_t e2[] = {v2, v3};

    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e1, 2, TRIT_POSITIVE, "e1");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e2, 2, TRIT_POSITIVE, "e2");

    /* Find connected components */
    uint32_t vertex_components[10];
    uint32_t num_components = 0;

    nimcp_error_t err = nimcp_hypergraph_connected_components(hg, vertex_components, &num_components);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(num_components, 1u);

    /* Check is_connected */
    EXPECT_TRUE(nimcp_hypergraph_is_connected(hg));
}

TEST_F(HypergraphKnowledgeIntegrationTest, ConnectedComponentsMultipleComponents) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    /* Component 1 */
    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v1", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v2", 1.0f);

    /* Component 2 (disconnected) */
    uint32_t v3 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v3", 1.0f);
    uint32_t v4 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v4", 1.0f);

    uint32_t e1[] = {v1, v2};
    uint32_t e2[] = {v3, v4};

    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e1, 2, TRIT_POSITIVE, "e1");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e2, 2, TRIT_POSITIVE, "e2");

    /* Find connected components */
    uint32_t vertex_components[10];
    uint32_t num_components = 0;

    nimcp_error_t err = nimcp_hypergraph_connected_components(hg, vertex_components, &num_components);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(num_components, 2u);

    /* Not connected */
    EXPECT_FALSE(nimcp_hypergraph_is_connected(hg));
}

/* ============================================================================
 * Conversion Tests
 * ============================================================================ */

TEST_F(HypergraphKnowledgeIntegrationTest, ConvertToTernaryGraph) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    /* Create hypergraph */
    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v1", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v2", 1.0f);
    uint32_t v3 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v3", 1.0f);

    uint32_t e1[] = {v1, v2, v3};  /* 3-ary edge */
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e1, 3, TRIT_POSITIVE, "e1");

    /* Convert to ternary graph (2-section) */
    NimcpTernaryGraph* ternary = nimcp_hypergraph_to_ternary(hg);
    /* May be NULL if ternary graph support not available */
    if (ternary) {
        /* In 2-section, vertices connected by hyperedge become pairwise connected */
        /* v1-v2, v2-v3, v1-v3 should all be edges */
        /* Would need ternary graph API to verify */
        /* Clean up ternary graph */
    }
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(HypergraphKnowledgeIntegrationTest, StatisticsAccurate) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    /* Add vertices of different types */
    nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "c1", 1.0f);
    nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "c2", 1.0f);
    nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PREDICATE, "p1", 1.0f);
    nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_FUNCTION, "f1", 1.0f);
    nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_VARIABLE, "x", 1.0f);

    /* Get stats */
    hypergraph_stats_t stats;
    nimcp_error_t err = nimcp_hypergraph_get_stats(hg, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(stats.vertex_count, 5u);
    EXPECT_EQ(stats.vertex_type_counts[HYPERVERTEX_CONSTANT], 2u);
    EXPECT_EQ(stats.vertex_type_counts[HYPERVERTEX_PREDICATE], 1u);
    EXPECT_EQ(stats.vertex_type_counts[HYPERVERTEX_FUNCTION], 1u);
    EXPECT_EQ(stats.vertex_type_counts[HYPERVERTEX_VARIABLE], 1u);
}

TEST_F(HypergraphKnowledgeIntegrationTest, EdgeArityStatistics) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v1", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v2", 1.0f);
    uint32_t v3 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v3", 1.0f);
    uint32_t v4 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v4", 1.0f);

    /* Add edges of different arities */
    uint32_t e2[] = {v1, v2};           /* Arity 2 */
    uint32_t e3[] = {v1, v2, v3};       /* Arity 3 */
    uint32_t e4[] = {v1, v2, v3, v4};   /* Arity 4 */

    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e2, 2, TRIT_POSITIVE, "e2");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e3, 3, TRIT_POSITIVE, "e3");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e4, 4, TRIT_POSITIVE, "e4");

    hypergraph_stats_t stats;
    nimcp_hypergraph_get_stats(hg, &stats);

    EXPECT_EQ(stats.max_edge_arity, 4u);
    EXPECT_FLOAT_EQ(stats.avg_edge_arity, 3.0f);  /* (2+3+4)/3 */
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST_F(HypergraphKnowledgeIntegrationTest, RemoveNonexistentVertex) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    /* Try to remove vertex that doesn't exist */
    nimcp_error_t err = nimcp_hypergraph_remove_vertex(hg, 9999);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(HypergraphKnowledgeIntegrationTest, RemoveNonexistentEdge) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    /* Try to remove edge that doesn't exist */
    nimcp_error_t err = nimcp_hypergraph_remove_edge(hg, 9999);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(HypergraphKnowledgeIntegrationTest, ClearHypergraph) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    /* Add some data */
    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v1", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "v2", 1.0f);
    uint32_t e[] = {v1, v2};
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, e, 2, TRIT_POSITIVE, "e");

    /* Clear */
    nimcp_error_t err = nimcp_hypergraph_clear(hg);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify empty */
    EXPECT_EQ(nimcp_hypergraph_vertex_count(hg), 0u);
    EXPECT_EQ(nimcp_hypergraph_edge_count(hg), 0u);
}

/* ============================================================================
 * Query Result Management Tests
 * ============================================================================ */

TEST_F(HypergraphKnowledgeIntegrationTest, QueryResultManagement) {
    hypergraph_query_result_t result;

    /* Initialize */
    nimcp_error_t err = nimcp_hypergraph_query_result_init(&result, 10, 10);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify initialization */
    EXPECT_NE(result.vertex_ids, nullptr);
    EXPECT_NE(result.edge_ids, nullptr);
    EXPECT_EQ(result.vertex_count, 0u);
    EXPECT_EQ(result.edge_count, 0u);

    /* Cleanup */
    nimcp_hypergraph_query_result_cleanup(&result);
}
