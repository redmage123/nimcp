/**
 * @file test_hypergraph.cpp
 * @brief Unit tests for Hypergraph Data Structure
 *
 * Tests the hypergraph implementation which supports n-ary relations
 * for knowledge representation and mathematical reasoning.
 */

#include <gtest/gtest.h>
#include "utils/nimcp_test_base.h"

extern "C" {
#include "cognitive/neuro_symbolic/nimcp_hypergraph.h"
}

/**
 * @brief Test fixture for Hypergraph tests
 */
class HypergraphTest : public NimcpTestBase {
protected:
    nimcp_hypergraph_t* hg;
    hypergraph_config_t config;

    void SetUp() override {
        NimcpTestBase::SetUp();
        hg = NULL;
        memset(&config, 0, sizeof(config));
        nimcp_hypergraph_get_default_config(&config);
    }

    void TearDown() override {
        if (hg) {
            nimcp_hypergraph_destroy(hg);
            hg = NULL;
        }
        NimcpTestBase::TearDown();
    }
};

// ============================================================================
// Lifecycle Tests
// ============================================================================

TEST_F(HypergraphTest, CreateSucceeds) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);
}

TEST_F(HypergraphTest, CreateWithConfigSucceeds) {
    hg = nimcp_hypergraph_create_with_config(&config);
    ASSERT_NE(hg, nullptr);
}

TEST_F(HypergraphTest, CreateWithNullConfigSucceeds) {
    hg = nimcp_hypergraph_create_with_config(NULL);
    EXPECT_NE(hg, nullptr);
}

TEST_F(HypergraphTest, DestroyNullIsNoOp) {
    nimcp_hypergraph_destroy(NULL);
    SUCCEED();
}

TEST_F(HypergraphTest, CreateDestroyMultipleTimesSucceeds) {
    for (int i = 0; i < 5; i++) {
        hg = nimcp_hypergraph_create();
        ASSERT_NE(hg, nullptr) << "Failed on iteration " << i;
        nimcp_hypergraph_destroy(hg);
        hg = NULL;
    }
}

TEST_F(HypergraphTest, ClearNullReturnsError) {
    nimcp_error_t err = nimcp_hypergraph_clear(NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(HypergraphTest, ClearEmptySucceeds) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    nimcp_error_t err = nimcp_hypergraph_clear(hg);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

// ============================================================================
// Default Configuration Tests
// ============================================================================

TEST_F(HypergraphTest, GetDefaultConfigSucceeds) {
    hypergraph_config_t cfg;
    nimcp_error_t err = nimcp_hypergraph_get_default_config(&cfg);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(HypergraphTest, GetDefaultConfigNullReturnsError) {
    nimcp_error_t err = nimcp_hypergraph_get_default_config(NULL);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(HypergraphTest, DefaultConfigHasReasonableValues) {
    hypergraph_config_t cfg;
    nimcp_hypergraph_get_default_config(&cfg);

    EXPECT_GT(cfg.initial_vertex_capacity, 0u);
    EXPECT_GT(cfg.initial_edge_capacity, 0u);
    EXPECT_GT(cfg.default_confidence, 0.0f);
    EXPECT_LE(cfg.default_confidence, 1.0f);
}

// ============================================================================
// Vertex Tests
// ============================================================================

TEST_F(HypergraphTest, AddVertexNullReturnsInvalid) {
    uint32_t id = nimcp_hypergraph_add_vertex(NULL, HYPERVERTEX_CONSTANT, "A", 1.0f);
    EXPECT_EQ(id, UINT32_MAX);
}

TEST_F(HypergraphTest, AddVertexSucceeds) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    uint32_t id = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "Pi", 1.0f);
    EXPECT_NE(id, UINT32_MAX);
}

TEST_F(HypergraphTest, AddMultipleVerticesSucceeds) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    uint32_t id1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "A", 1.0f);
    uint32_t id2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_VARIABLE, "x", 0.9f);
    uint32_t id3 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_FUNCTION, "f", 0.8f);

    EXPECT_NE(id1, UINT32_MAX);
    EXPECT_NE(id2, UINT32_MAX);
    EXPECT_NE(id3, UINT32_MAX);

    // IDs should be unique
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);
}

TEST_F(HypergraphTest, AddVertexWithDataSucceeds) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    float data = 3.14159f;
    uint32_t id = nimcp_hypergraph_add_vertex_with_data(
        hg, HYPERVERTEX_CONSTANT, "Pi", &data, sizeof(data), 1.0f);
    EXPECT_NE(id, UINT32_MAX);
}

TEST_F(HypergraphTest, GetVertexSucceeds) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    uint32_t id = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PREDICATE, "IsPrime", 0.95f);
    ASSERT_NE(id, UINT32_MAX);

    const nimcp_hypervertex_t* v = nimcp_hypergraph_get_vertex(hg, id);
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->id, id);
    EXPECT_EQ(v->type, HYPERVERTEX_PREDICATE);
    EXPECT_STREQ(v->label, "IsPrime");
    EXPECT_FLOAT_EQ(v->confidence, 0.95f);
}

TEST_F(HypergraphTest, GetVertexNullReturnsNull) {
    const nimcp_hypervertex_t* v = nimcp_hypergraph_get_vertex(NULL, 0);
    EXPECT_EQ(v, nullptr);
}

TEST_F(HypergraphTest, GetVertexInvalidIdReturnsNull) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    const nimcp_hypervertex_t* v = nimcp_hypergraph_get_vertex(hg, UINT32_MAX);
    EXPECT_EQ(v, nullptr);
}

TEST_F(HypergraphTest, FindVertexSucceeds) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    uint32_t original_id = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "E", 1.0f);
    ASSERT_NE(original_id, UINT32_MAX);

    uint32_t found_id = nimcp_hypergraph_find_vertex(hg, "E");
    EXPECT_EQ(found_id, original_id);
}

TEST_F(HypergraphTest, FindVertexNotFoundReturnsInvalid) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    uint32_t id = nimcp_hypergraph_find_vertex(hg, "NonExistent");
    EXPECT_EQ(id, UINT32_MAX);
}

TEST_F(HypergraphTest, RemoveVertexSucceeds) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    uint32_t id = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "ToRemove", 1.0f);
    ASSERT_NE(id, UINT32_MAX);

    nimcp_error_t err = nimcp_hypergraph_remove_vertex(hg, id);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Should no longer be findable
    const nimcp_hypervertex_t* v = nimcp_hypergraph_get_vertex(hg, id);
    EXPECT_EQ(v, nullptr);
}

TEST_F(HypergraphTest, UpdateVertexConfidenceSucceeds) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    uint32_t id = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_VARIABLE, "x", 0.5f);
    ASSERT_NE(id, UINT32_MAX);

    nimcp_error_t err = nimcp_hypergraph_update_vertex_confidence(hg, id, 0.9f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    const nimcp_hypervertex_t* v = nimcp_hypergraph_get_vertex(hg, id);
    ASSERT_NE(v, nullptr);
    EXPECT_FLOAT_EQ(v->confidence, 0.9f);
}

// ============================================================================
// Edge Tests
// ============================================================================

TEST_F(HypergraphTest, AddEdgeNullReturnsInvalid) {
    uint32_t vertices[] = {0, 1};
    uint32_t id = nimcp_hypergraph_add_edge(NULL, HYPEREDGE_RELATION, vertices, 2, TRIT_POSITIVE, "R");
    EXPECT_EQ(id, UINT32_MAX);
}

TEST_F(HypergraphTest, AddBinaryEdgeSucceeds) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    // Add two vertices
    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "A", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "B", 1.0f);
    ASSERT_NE(v1, UINT32_MAX);
    ASSERT_NE(v2, UINT32_MAX);

    // Add edge connecting them
    uint32_t vertices[] = {v1, v2};
    uint32_t edge_id = nimcp_hypergraph_add_edge(
        hg, HYPEREDGE_RELATION, vertices, 2, TRIT_POSITIVE, "Connected");
    EXPECT_NE(edge_id, UINT32_MAX);
}

TEST_F(HypergraphTest, AddTernaryEdgeSucceeds) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    // Add three vertices
    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "A", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "B", 1.0f);
    uint32_t v3 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "C", 1.0f);

    // Add ternary edge (e.g., Between(A, B, C))
    uint32_t vertices[] = {v1, v2, v3};
    uint32_t edge_id = nimcp_hypergraph_add_edge(
        hg, HYPEREDGE_RELATION, vertices, 3, TRIT_POSITIVE, "Between");
    EXPECT_NE(edge_id, UINT32_MAX);
}

TEST_F(HypergraphTest, AddNaryEdgeSucceeds) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    // Add 5 vertices
    uint32_t vertices[5];
    for (int i = 0; i < 5; i++) {
        char label[8];
        snprintf(label, sizeof(label), "V%d", i);
        vertices[i] = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, label, 1.0f);
        ASSERT_NE(vertices[i], UINT32_MAX);
    }

    // Add 5-ary edge
    uint32_t edge_id = nimcp_hypergraph_add_edge(
        hg, HYPEREDGE_CONSTRAINT, vertices, 5, TRIT_POSITIVE, "Constraint5");
    EXPECT_NE(edge_id, UINT32_MAX);
}

TEST_F(HypergraphTest, GetEdgeSucceeds) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "P", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "Q", 1.0f);

    uint32_t vertices[] = {v1, v2};
    uint32_t edge_id = nimcp_hypergraph_add_edge(
        hg, HYPEREDGE_THEOREM, vertices, 2, TRIT_POSITIVE, "Implies");
    ASSERT_NE(edge_id, UINT32_MAX);

    const nimcp_hyperedge_t* e = nimcp_hypergraph_get_edge(hg, edge_id);
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->id, edge_id);
    EXPECT_EQ(e->type, HYPEREDGE_THEOREM);
    EXPECT_EQ(e->vertex_count, 2u);
    EXPECT_STREQ(e->label, "Implies");
}

TEST_F(HypergraphTest, RemoveEdgeSucceeds) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "A", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "B", 1.0f);

    uint32_t vertices[] = {v1, v2};
    uint32_t edge_id = nimcp_hypergraph_add_edge(
        hg, HYPEREDGE_RELATION, vertices, 2, TRIT_POSITIVE, "ToRemove");
    ASSERT_NE(edge_id, UINT32_MAX);

    nimcp_error_t err = nimcp_hypergraph_remove_edge(hg, edge_id);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    const nimcp_hyperedge_t* e = nimcp_hypergraph_get_edge(hg, edge_id);
    EXPECT_EQ(e, nullptr);
}

TEST_F(HypergraphTest, GetIncidentEdgesSucceeds) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    // Create a vertex and connect it to multiple edges
    uint32_t center = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "Center", 1.0f);
    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "V1", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "V2", 1.0f);
    uint32_t v3 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "V3", 1.0f);

    // Add edges incident to center
    uint32_t verts1[] = {center, v1};
    uint32_t verts2[] = {center, v2};
    uint32_t verts3[] = {center, v1, v3};  // center is in this edge too

    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, verts1, 2, TRIT_POSITIVE, "E1");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, verts2, 2, TRIT_POSITIVE, "E2");
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, verts3, 3, TRIT_POSITIVE, "E3");

    // Get incident edges
    uint32_t incident[10];
    uint32_t count = nimcp_hypergraph_get_incident_edges(hg, center, incident, 10);
    EXPECT_EQ(count, 3u);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(HypergraphTest, GetStatsNullReturnsError) {
    hypergraph_stats_t stats;
    nimcp_error_t err = nimcp_hypergraph_get_stats(NULL, &stats);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(HypergraphTest, GetStatsReflectsAdditions) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    // Add vertices
    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "A", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_VARIABLE, "x", 1.0f);
    uint32_t v3 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_FUNCTION, "f", 1.0f);

    // Add edge
    uint32_t vertices[] = {v1, v2, v3};
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, vertices, 3, TRIT_POSITIVE, "R");

    hypergraph_stats_t stats;
    nimcp_error_t err = nimcp_hypergraph_get_stats(hg, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.vertex_count, 3u);
    EXPECT_EQ(stats.edge_count, 1u);
    EXPECT_EQ(stats.max_edge_arity, 3u);
}

// ============================================================================
// Clear Tests
// ============================================================================

TEST_F(HypergraphTest, ClearRemovesAllContent) {
    hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    // Add some content
    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "A", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "B", 1.0f);
    uint32_t vertices[] = {v1, v2};
    nimcp_hypergraph_add_edge(hg, HYPEREDGE_RELATION, vertices, 2, TRIT_POSITIVE, "E");

    // Clear
    nimcp_error_t err = nimcp_hypergraph_clear(hg);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Stats should show empty
    hypergraph_stats_t stats;
    nimcp_hypergraph_get_stats(hg, &stats);
    EXPECT_EQ(stats.vertex_count, 0u);
    EXPECT_EQ(stats.edge_count, 0u);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(HypergraphTest, FullWorkflowSucceeds) {
    // Create with config
    config.enable_bio_async = false;
    hg = nimcp_hypergraph_create_with_config(&config);
    ASSERT_NE(hg, nullptr);

    // Add vertices representing a simple logical structure
    uint32_t p = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PREDICATE, "P", 1.0f);
    uint32_t q = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PREDICATE, "Q", 1.0f);
    uint32_t r = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PREDICATE, "R", 1.0f);

    // Add P -> Q (implication as directed edge)
    uint32_t impl1[] = {p, q};
    uint32_t e1 = nimcp_hypergraph_add_edge_full(
        hg, HYPEREDGE_RULE, impl1, 2, TRIT_POSITIVE, 1.0f, true, "P_implies_Q");
    EXPECT_NE(e1, UINT32_MAX);

    // Add Q -> R
    uint32_t impl2[] = {q, r};
    uint32_t e2 = nimcp_hypergraph_add_edge_full(
        hg, HYPEREDGE_RULE, impl2, 2, TRIT_POSITIVE, 1.0f, true, "Q_implies_R");
    EXPECT_NE(e2, UINT32_MAX);

    // Verify structure
    hypergraph_stats_t stats;
    nimcp_hypergraph_get_stats(hg, &stats);
    EXPECT_EQ(stats.vertex_count, 3u);
    EXPECT_EQ(stats.edge_count, 2u);

    // Find vertex by label
    uint32_t found_p = nimcp_hypergraph_find_vertex(hg, "P");
    EXPECT_EQ(found_p, p);

    // Get incident edges for Q (should be 2 - one incoming, one outgoing)
    uint32_t incident[10];
    uint32_t incident_count = nimcp_hypergraph_get_incident_edges(hg, q, incident, 10);
    EXPECT_EQ(incident_count, 2u);

    // Clear and verify
    nimcp_hypergraph_clear(hg);
    nimcp_hypergraph_get_stats(hg, &stats);
    EXPECT_EQ(stats.vertex_count, 0u);
}
