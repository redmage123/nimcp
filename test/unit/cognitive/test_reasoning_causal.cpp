/**
 * @file test_reasoning_causal.cpp
 * @brief Unit tests for the causal reasoning engine (DAG + do-calculus)
 *
 * WHAT: Tests causal DAG creation, node/edge management, queries, and validation
 * WHY:  Verify causal reasoning components work correctly in isolation
 * HOW:  GTest suite testing each component independently
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_causal.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class ReasoningCausalTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(ReasoningCausalTest, CreateDestroy) {
    causal_dag_t* dag = causal_dag_create(NULL);
    ASSERT_NE(dag, nullptr);
    causal_dag_destroy(dag);
}

TEST_F(ReasoningCausalTest, CreateWithConfig) {
    causal_dag_config_t config = causal_dag_default_config();
    config.max_nodes = 128;
    config.propagation_damping = 0.8f;
    causal_dag_t* dag = causal_dag_create(&config);
    ASSERT_NE(dag, nullptr);

    causal_dag_stats_t stats;
    causal_dag_get_stats(dag, &stats);
    EXPECT_EQ(stats.num_nodes, 0u);
    EXPECT_EQ(stats.num_edges, 0u);

    causal_dag_destroy(dag);
}

TEST_F(ReasoningCausalTest, DestroyNull) {
    /* Should not crash */
    causal_dag_destroy(NULL);
}

/*=============================================================================
 * CONFIGURATION TESTS
 *===========================================================================*/

TEST_F(ReasoningCausalTest, DefaultConfig) {
    causal_dag_config_t config = causal_dag_default_config();
    EXPECT_EQ(config.max_nodes, (uint32_t)CAUSAL_MAX_NODES);
    EXPECT_EQ(config.max_edges, (uint32_t)CAUSAL_MAX_EDGES);
    EXPECT_FLOAT_EQ(config.default_prior, CAUSAL_DEFAULT_PRIOR);
    EXPECT_FLOAT_EQ(config.propagation_damping, 0.9f);
}

/*=============================================================================
 * NODE MANAGEMENT TESTS
 *===========================================================================*/

TEST_F(ReasoningCausalTest, AddNode) {
    causal_dag_t* dag = causal_dag_create(NULL);
    ASSERT_NE(dag, nullptr);

    int id = causal_dag_add_node(dag, "Smoking", 0.3f);
    EXPECT_EQ(id, 0);

    int id2 = causal_dag_add_node(dag, "Cancer", 0.1f);
    EXPECT_EQ(id2, 1);

    causal_node_t node;
    int rc = causal_dag_get_node(dag, 0, &node);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(node.id, 0u);
    EXPECT_STREQ(node.name, "Smoking");
    EXPECT_FLOAT_EQ(node.prior_probability, 0.3f);
    EXPECT_FALSE(node.is_observed);
    EXPECT_FALSE(node.is_intervened);
    EXPECT_TRUE(std::isnan(node.observed_value));

    causal_dag_destroy(dag);
}

TEST_F(ReasoningCausalTest, AddNodeMax) {
    causal_dag_config_t config = causal_dag_default_config();
    config.max_nodes = 4;
    causal_dag_t* dag = causal_dag_create(&config);
    ASSERT_NE(dag, nullptr);

    for (int i = 0; i < 4; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Node%d", i);
        int id = causal_dag_add_node(dag, name, 0.5f);
        EXPECT_EQ(id, i);
    }

    /* Should fail — at max capacity */
    int id = causal_dag_add_node(dag, "Overflow", 0.5f);
    EXPECT_EQ(id, -1);

    causal_dag_destroy(dag);
}

TEST_F(ReasoningCausalTest, AddNodeNullInputs) {
    int id = causal_dag_add_node(NULL, "Test", 0.5f);
    EXPECT_EQ(id, -1);

    causal_dag_t* dag = causal_dag_create(NULL);
    id = causal_dag_add_node(dag, NULL, 0.5f);
    EXPECT_EQ(id, -1);
    causal_dag_destroy(dag);
}

/*=============================================================================
 * EDGE MANAGEMENT TESTS
 *===========================================================================*/

TEST_F(ReasoningCausalTest, AddEdge) {
    causal_dag_t* dag = causal_dag_create(NULL);
    causal_dag_add_node(dag, "A", 0.5f);
    causal_dag_add_node(dag, "B", 0.5f);

    int rc = causal_dag_add_edge(dag, 0, 1, 0.8f);
    EXPECT_EQ(rc, 0);

    causal_dag_stats_t stats;
    causal_dag_get_stats(dag, &stats);
    EXPECT_EQ(stats.num_edges, 1u);

    /* Verify parent/child */
    uint32_t children[CAUSAL_MAX_PARENTS];
    uint32_t count = 0;
    causal_dag_get_children(dag, 0, children, &count);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(children[0], 1u);

    uint32_t parents[CAUSAL_MAX_PARENTS];
    count = 0;
    causal_dag_get_parents(dag, 1, parents, &count);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(parents[0], 0u);

    causal_dag_destroy(dag);
}

TEST_F(ReasoningCausalTest, AddEdgeSelfLoop) {
    causal_dag_t* dag = causal_dag_create(NULL);
    causal_dag_add_node(dag, "A", 0.5f);

    int rc = causal_dag_add_edge(dag, 0, 0, 0.5f);
    EXPECT_EQ(rc, -1);  /* Self-loops rejected */

    causal_dag_destroy(dag);
}

TEST_F(ReasoningCausalTest, AddEdgeCycle) {
    causal_dag_t* dag = causal_dag_create(NULL);
    causal_dag_add_node(dag, "A", 0.5f);
    causal_dag_add_node(dag, "B", 0.5f);
    causal_dag_add_node(dag, "C", 0.5f);

    EXPECT_EQ(causal_dag_add_edge(dag, 0, 1, 0.5f), 0);  /* A -> B */
    EXPECT_EQ(causal_dag_add_edge(dag, 1, 2, 0.5f), 0);  /* B -> C */
    EXPECT_EQ(causal_dag_add_edge(dag, 2, 0, 0.5f), -1); /* C -> A would create cycle */

    /* Verify the DAG is still valid after rejected edge */
    EXPECT_EQ(causal_dag_validate(dag), 0);

    causal_dag_stats_t stats;
    causal_dag_get_stats(dag, &stats);
    EXPECT_EQ(stats.num_edges, 2u);

    causal_dag_destroy(dag);
}

TEST_F(ReasoningCausalTest, RemoveEdge) {
    causal_dag_t* dag = causal_dag_create(NULL);
    causal_dag_add_node(dag, "A", 0.5f);
    causal_dag_add_node(dag, "B", 0.5f);
    causal_dag_add_edge(dag, 0, 1, 0.8f);

    int rc = causal_dag_remove_edge(dag, 0, 1);
    EXPECT_EQ(rc, 0);

    causal_dag_stats_t stats;
    causal_dag_get_stats(dag, &stats);
    EXPECT_EQ(stats.num_edges, 0u);

    /* Remove non-existent edge */
    rc = causal_dag_remove_edge(dag, 0, 1);
    EXPECT_EQ(rc, -1);

    causal_dag_destroy(dag);
}

/*=============================================================================
 * OBSERVATION AND INTERVENTION TESTS
 *===========================================================================*/

TEST_F(ReasoningCausalTest, ObserveNode) {
    causal_dag_t* dag = causal_dag_create(NULL);
    causal_dag_add_node(dag, "X", 0.5f);

    int rc = causal_dag_observe(dag, 0, 0.75f);
    EXPECT_EQ(rc, 0);

    causal_node_t node;
    causal_dag_get_node(dag, 0, &node);
    EXPECT_TRUE(node.is_observed);
    EXPECT_FLOAT_EQ(node.observed_value, 0.75f);

    causal_dag_destroy(dag);
}

TEST_F(ReasoningCausalTest, InterveneNode) {
    causal_dag_t* dag = causal_dag_create(NULL);
    causal_dag_add_node(dag, "X", 0.5f);

    int rc = causal_dag_intervene(dag, 0, 1.0f);
    EXPECT_EQ(rc, 0);

    causal_node_t node;
    causal_dag_get_node(dag, 0, &node);
    EXPECT_TRUE(node.is_intervened);
    EXPECT_FLOAT_EQ(node.intervened_value, 1.0f);

    causal_dag_destroy(dag);
}

TEST_F(ReasoningCausalTest, ClearIntervention) {
    causal_dag_t* dag = causal_dag_create(NULL);
    causal_dag_add_node(dag, "X", 0.5f);
    causal_dag_intervene(dag, 0, 1.0f);

    int rc = causal_dag_clear_intervention(dag, 0);
    EXPECT_EQ(rc, 0);

    causal_node_t node;
    causal_dag_get_node(dag, 0, &node);
    EXPECT_FALSE(node.is_intervened);

    causal_dag_destroy(dag);
}

/*=============================================================================
 * GET NODE TESTS
 *===========================================================================*/

TEST_F(ReasoningCausalTest, GetNode) {
    causal_dag_t* dag = causal_dag_create(NULL);
    causal_dag_add_node(dag, "TestNode", 0.42f);

    causal_node_t node;
    int rc = causal_dag_get_node(dag, 0, &node);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(node.id, 0u);
    EXPECT_STREQ(node.name, "TestNode");
    EXPECT_FLOAT_EQ(node.prior_probability, 0.42f);

    /* Invalid ID */
    rc = causal_dag_get_node(dag, 999, &node);
    EXPECT_EQ(rc, -1);

    causal_dag_destroy(dag);
}

/*=============================================================================
 * GRAPH TRAVERSAL TESTS
 *===========================================================================*/

TEST_F(ReasoningCausalTest, GetParentsAndChildren) {
    causal_dag_t* dag = causal_dag_create(NULL);
    causal_dag_add_node(dag, "A", 0.5f);  /* 0 */
    causal_dag_add_node(dag, "B", 0.5f);  /* 1 */
    causal_dag_add_node(dag, "C", 0.5f);  /* 2 */
    causal_dag_add_edge(dag, 0, 2, 0.7f); /* A -> C */
    causal_dag_add_edge(dag, 1, 2, 0.6f); /* B -> C */

    uint32_t parents[CAUSAL_MAX_PARENTS];
    uint32_t count = 0;
    causal_dag_get_parents(dag, 2, parents, &count);
    EXPECT_EQ(count, 2u);

    uint32_t children[CAUSAL_MAX_NODES];
    count = 0;
    causal_dag_get_children(dag, 0, children, &count);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(children[0], 2u);

    causal_dag_destroy(dag);
}

TEST_F(ReasoningCausalTest, FindPathDirect) {
    causal_dag_t* dag = causal_dag_create(NULL);
    causal_dag_add_node(dag, "A", 0.5f);
    causal_dag_add_node(dag, "B", 0.5f);
    causal_dag_add_edge(dag, 0, 1, 0.9f);

    uint32_t path[CAUSAL_MAX_NODES];
    uint32_t path_len = 0;
    int rc = causal_dag_find_path(dag, 0, 1, path, &path_len);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(path_len, 2u);  /* A, B */
    EXPECT_EQ(path[0], 0u);
    EXPECT_EQ(path[1], 1u);

    causal_dag_destroy(dag);
}

TEST_F(ReasoningCausalTest, FindPathIndirect) {
    causal_dag_t* dag = causal_dag_create(NULL);
    causal_dag_add_node(dag, "A", 0.5f);  /* 0 */
    causal_dag_add_node(dag, "B", 0.5f);  /* 1 */
    causal_dag_add_node(dag, "C", 0.5f);  /* 2 */
    causal_dag_add_edge(dag, 0, 1, 0.8f);
    causal_dag_add_edge(dag, 1, 2, 0.7f);

    uint32_t path[CAUSAL_MAX_NODES];
    uint32_t path_len = 0;
    int rc = causal_dag_find_path(dag, 0, 2, path, &path_len);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(path_len, 3u);  /* A, B, C */
    EXPECT_EQ(path[0], 0u);
    EXPECT_EQ(path[1], 1u);
    EXPECT_EQ(path[2], 2u);

    causal_dag_destroy(dag);
}

TEST_F(ReasoningCausalTest, FindPathNoPath) {
    causal_dag_t* dag = causal_dag_create(NULL);
    causal_dag_add_node(dag, "A", 0.5f);
    causal_dag_add_node(dag, "B", 0.5f);
    /* No edge between A and B */

    uint32_t path[CAUSAL_MAX_NODES];
    uint32_t path_len = 0;
    int rc = causal_dag_find_path(dag, 0, 1, path, &path_len);
    EXPECT_EQ(rc, -1);
    EXPECT_EQ(path_len, 0u);

    causal_dag_destroy(dag);
}

TEST_F(ReasoningCausalTest, IsAncestor) {
    causal_dag_t* dag = causal_dag_create(NULL);
    causal_dag_add_node(dag, "A", 0.5f);  /* 0 */
    causal_dag_add_node(dag, "B", 0.5f);  /* 1 */
    causal_dag_add_node(dag, "C", 0.5f);  /* 2 */
    causal_dag_add_edge(dag, 0, 1, 0.8f);
    causal_dag_add_edge(dag, 1, 2, 0.7f);

    EXPECT_TRUE(causal_dag_is_ancestor(dag, 0, 2));   /* A is ancestor of C */
    EXPECT_TRUE(causal_dag_is_ancestor(dag, 0, 1));   /* A is ancestor of B */
    EXPECT_FALSE(causal_dag_is_ancestor(dag, 2, 0));  /* C is NOT ancestor of A */
    EXPECT_FALSE(causal_dag_is_ancestor(dag, 1, 0));  /* B is NOT ancestor of A */

    causal_dag_destroy(dag);
}

/*=============================================================================
 * VALIDATION TESTS
 *===========================================================================*/

TEST_F(ReasoningCausalTest, ValidateAcyclic) {
    causal_dag_t* dag = causal_dag_create(NULL);
    causal_dag_add_node(dag, "A", 0.5f);
    causal_dag_add_node(dag, "B", 0.5f);
    causal_dag_add_node(dag, "C", 0.5f);
    causal_dag_add_edge(dag, 0, 1, 0.8f);
    causal_dag_add_edge(dag, 1, 2, 0.7f);

    EXPECT_EQ(causal_dag_validate(dag), 0);

    causal_dag_destroy(dag);
}

TEST_F(ReasoningCausalTest, ValidateNull) {
    EXPECT_EQ(causal_dag_validate(NULL), -1);
}

/*=============================================================================
 * ASSOCIATION QUERY TESTS
 *===========================================================================*/

TEST_F(ReasoningCausalTest, AssociationQuery) {
    /* A -> B with strength 0.8 */
    causal_dag_t* dag = causal_dag_create(NULL);
    causal_dag_add_node(dag, "A", 0.5f);
    causal_dag_add_node(dag, "B", 0.3f);
    causal_dag_add_edge(dag, 0, 1, 0.8f);

    causal_query_t query;
    memset(&query, 0, sizeof(query));
    query.type = CAUSAL_QUERY_ASSOCIATION;
    query.target_id = 1;  /* B */
    query.condition_ids[0] = 0;  /* given A */
    query.condition_values[0] = 1.0f;
    query.num_conditions = 1;

    causal_result_t result;
    int rc = causal_dag_query(dag, &query, &result);
    EXPECT_EQ(rc, 0);

    /* P(B|A) = prior(B) + (1 - prior(B)) * strength * damping */
    /* 0.3 + 0.7 * 0.8 * 0.9 = 0.3 + 0.504 = 0.804 */
    EXPECT_GT(result.probability, 0.3f);
    EXPECT_LE(result.probability, 1.0f);
    EXPECT_FALSE(result.is_causal);  /* Association is not causal */
    EXPECT_EQ(result.path_length, 1u);
    EXPECT_GT(strlen(result.explanation), 0u);

    causal_dag_destroy(dag);
}

/*=============================================================================
 * INTERVENTION QUERY TESTS
 *===========================================================================*/

TEST_F(ReasoningCausalTest, InterventionQuery) {
    /* Smoking -> Cancer with strength 0.7 */
    causal_dag_t* dag = causal_dag_create(NULL);
    causal_dag_add_node(dag, "Smoking", 0.3f);
    causal_dag_add_node(dag, "Cancer", 0.1f);
    causal_dag_add_edge(dag, 0, 1, 0.7f);

    causal_query_t query;
    memset(&query, 0, sizeof(query));
    query.type = CAUSAL_QUERY_INTERVENTION;
    query.target_id = 1;  /* Cancer */
    query.intervention_id = 0;  /* do(Smoking) */
    query.intervention_value = 1.0f;

    causal_result_t result;
    int rc = causal_dag_query(dag, &query, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_TRUE(result.is_causal);  /* Intervention identifies causation */
    EXPECT_GT(result.probability, 0.1f);  /* Greater than prior */
    EXPECT_GT(result.causal_strength, 0.0f);
    EXPECT_EQ(result.path_length, 1u);

    causal_dag_destroy(dag);
}

/*=============================================================================
 * COUNTERFACTUAL QUERY TESTS
 *===========================================================================*/

TEST_F(ReasoningCausalTest, CounterfactualQuery) {
    causal_dag_t* dag = causal_dag_create(NULL);
    causal_dag_add_node(dag, "Treatment", 0.5f);
    causal_dag_add_node(dag, "Recovery", 0.3f);
    causal_dag_add_edge(dag, 0, 1, 0.8f);

    causal_query_t query;
    memset(&query, 0, sizeof(query));
    query.type = CAUSAL_QUERY_COUNTERFACTUAL;
    query.target_id = 1;  /* Recovery */
    query.intervention_id = 0;  /* What if Treatment=1? */
    query.intervention_value = 1.0f;

    causal_result_t result;
    int rc = causal_dag_query(dag, &query, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(result.probability, 0.0f);
    /* Counterfactual confidence should be lower than intervention (0.7x) */
    EXPECT_LT(result.confidence, 0.8f);

    causal_dag_destroy(dag);
}

/*=============================================================================
 * KEY TEST: INTERVENTION VS ASSOCIATION
 *===========================================================================*/

TEST_F(ReasoningCausalTest, InterventionVsAssociation) {
    /*
     * Classic confounder example:
     *   Genetics (G) -> Smoking (S)
     *   Genetics (G) -> Cancer (C)
     *   Smoking (S) -> Cancer (C)
     *
     * Association P(C|S) includes both direct and confounded paths.
     * Intervention P(C|do(S)) removes the G->S edge (do-operator),
     * so only the direct S->C path contributes.
     *
     * This test verifies that intervention and association give different
     * results when a confounder exists.
     */
    causal_dag_t* dag = causal_dag_create(NULL);
    int g = causal_dag_add_node(dag, "Genetics", 0.2f);
    int s = causal_dag_add_node(dag, "Smoking", 0.3f);
    int c = causal_dag_add_node(dag, "Cancer", 0.05f);

    causal_dag_add_edge(dag, (uint32_t)g, (uint32_t)s, 0.6f);  /* G -> S */
    causal_dag_add_edge(dag, (uint32_t)g, (uint32_t)c, 0.5f);  /* G -> C */
    causal_dag_add_edge(dag, (uint32_t)s, (uint32_t)c, 0.4f);  /* S -> C */

    /* Association: P(C|S) — includes confounder path */
    causal_query_t assoc_query;
    memset(&assoc_query, 0, sizeof(assoc_query));
    assoc_query.type = CAUSAL_QUERY_ASSOCIATION;
    assoc_query.target_id = (uint32_t)c;
    assoc_query.condition_ids[0] = (uint32_t)s;
    assoc_query.condition_values[0] = 1.0f;
    assoc_query.num_conditions = 1;

    causal_result_t assoc_result;
    causal_dag_query(dag, &assoc_query, &assoc_result);

    /* Intervention: P(C|do(S)) — only direct causal path */
    causal_query_t interv_query;
    memset(&interv_query, 0, sizeof(interv_query));
    interv_query.type = CAUSAL_QUERY_INTERVENTION;
    interv_query.target_id = (uint32_t)c;
    interv_query.intervention_id = (uint32_t)s;
    interv_query.intervention_value = 1.0f;

    causal_result_t interv_result;
    causal_dag_query(dag, &interv_query, &interv_result);

    /* Association should NOT be causal; intervention IS causal */
    EXPECT_FALSE(assoc_result.is_causal);
    EXPECT_TRUE(interv_result.is_causal);

    /* Both should produce non-zero probabilities */
    EXPECT_GT(assoc_result.probability, 0.05f);
    EXPECT_GT(interv_result.probability, 0.05f);

    causal_dag_destroy(dag);
}

/*=============================================================================
 * PROPAGATION DAMPING TEST
 *===========================================================================*/

TEST_F(ReasoningCausalTest, PropagationDamping) {
    /*
     * Longer paths should produce weaker signals due to damping.
     * A -> B -> C -> D  (3 edges, damping applied 3 times)
     * vs A -> B (1 edge, damping applied once)
     */
    causal_dag_t* dag = causal_dag_create(NULL);
    causal_dag_add_node(dag, "A", 0.5f);
    causal_dag_add_node(dag, "B", 0.5f);
    causal_dag_add_node(dag, "C", 0.5f);
    causal_dag_add_node(dag, "D", 0.5f);

    causal_dag_add_edge(dag, 0, 1, 1.0f);
    causal_dag_add_edge(dag, 1, 2, 1.0f);
    causal_dag_add_edge(dag, 2, 3, 1.0f);

    /* Query A -> B (1 edge) */
    causal_query_t q1;
    memset(&q1, 0, sizeof(q1));
    q1.type = CAUSAL_QUERY_INTERVENTION;
    q1.target_id = 1;
    q1.intervention_id = 0;
    q1.intervention_value = 1.0f;

    causal_result_t r1;
    causal_dag_query(dag, &q1, &r1);

    /* Query A -> D (3 edges) */
    causal_query_t q2;
    memset(&q2, 0, sizeof(q2));
    q2.type = CAUSAL_QUERY_INTERVENTION;
    q2.target_id = 3;
    q2.intervention_id = 0;
    q2.intervention_value = 1.0f;

    causal_result_t r2;
    causal_dag_query(dag, &q2, &r2);

    /* Short path should have stronger causal effect */
    EXPECT_GT(r1.causal_strength, r2.causal_strength);
    EXPECT_GT(r1.probability, r2.probability);

    causal_dag_destroy(dag);
}

/*=============================================================================
 * STATISTICS TESTS
 *===========================================================================*/

TEST_F(ReasoningCausalTest, GetStats) {
    causal_dag_t* dag = causal_dag_create(NULL);
    causal_dag_add_node(dag, "A", 0.5f);
    causal_dag_add_node(dag, "B", 0.5f);
    causal_dag_add_edge(dag, 0, 1, 0.8f);

    causal_dag_stats_t stats;
    int rc = causal_dag_get_stats(dag, &stats);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(stats.num_nodes, 2u);
    EXPECT_EQ(stats.num_edges, 1u);
    EXPECT_EQ(stats.num_queries, 0u);

    /* Run a query */
    causal_query_t query;
    memset(&query, 0, sizeof(query));
    query.type = CAUSAL_QUERY_ASSOCIATION;
    query.target_id = 1;
    query.condition_ids[0] = 0;
    query.num_conditions = 1;

    causal_result_t result;
    causal_dag_query(dag, &query, &result);

    causal_dag_get_stats(dag, &stats);
    EXPECT_EQ(stats.num_queries, 1u);

    causal_dag_destroy(dag);
}

/*=============================================================================
 * NULL INPUT TESTS
 *===========================================================================*/

TEST_F(ReasoningCausalTest, NullInputs) {
    causal_dag_t* dag = causal_dag_create(NULL);
    causal_dag_add_node(dag, "A", 0.5f);

    /* NULL dag */
    EXPECT_EQ(causal_dag_add_node(NULL, "X", 0.5f), -1);
    EXPECT_EQ(causal_dag_add_edge(NULL, 0, 1, 0.5f), -1);
    EXPECT_EQ(causal_dag_remove_edge(NULL, 0, 1), -1);
    EXPECT_EQ(causal_dag_observe(NULL, 0, 0.5f), -1);
    EXPECT_EQ(causal_dag_intervene(NULL, 0, 0.5f), -1);
    EXPECT_EQ(causal_dag_clear_intervention(NULL, 0), -1);

    causal_query_t query;
    memset(&query, 0, sizeof(query));
    causal_result_t result;
    EXPECT_EQ(causal_dag_query(NULL, &query, &result), -1);
    EXPECT_EQ(causal_dag_query(dag, NULL, &result), -1);
    EXPECT_EQ(causal_dag_query(dag, &query, NULL), -1);

    uint32_t path[CAUSAL_MAX_NODES];
    uint32_t path_len = 0;
    EXPECT_EQ(causal_dag_find_path(NULL, 0, 1, path, &path_len), -1);
    EXPECT_EQ(causal_dag_find_path(dag, 0, 1, NULL, &path_len), -1);

    EXPECT_FALSE(causal_dag_is_ancestor(NULL, 0, 1));

    uint32_t parents[CAUSAL_MAX_PARENTS];
    uint32_t count = 0;
    EXPECT_EQ(causal_dag_get_parents(NULL, 0, parents, &count), -1);
    EXPECT_EQ(causal_dag_get_children(NULL, 0, parents, &count), -1);

    causal_dag_stats_t stats;
    EXPECT_EQ(causal_dag_get_stats(NULL, &stats), -1);
    EXPECT_EQ(causal_dag_get_stats(dag, NULL), -1);

    causal_node_t node;
    EXPECT_EQ(causal_dag_get_node(NULL, 0, &node), -1);
    EXPECT_EQ(causal_dag_get_node(dag, 0, NULL), -1);

    /* Invalid node IDs */
    EXPECT_EQ(causal_dag_observe(dag, 999, 0.5f), -1);
    EXPECT_EQ(causal_dag_intervene(dag, 999, 0.5f), -1);
    EXPECT_EQ(causal_dag_clear_intervention(dag, 999), -1);

    causal_dag_destroy(dag);
}

/*=============================================================================
 * STEP TYPE NAME TEST
 *===========================================================================*/

TEST_F(ReasoningCausalTest, StepTypeName) {
    const char* name = reasoning_step_type_name(REASONING_STEP_CAUSAL);
    EXPECT_STREQ(name, "CAUSAL");
}

/*=============================================================================
 * CONFIG INTEGRATION TEST
 *===========================================================================*/

TEST_F(ReasoningCausalTest, DefaultConfigCausalDisabled) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    EXPECT_FALSE(config.enable_causal_reasoning);
}
