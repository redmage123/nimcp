//=============================================================================
// test_hypergraph_init.cpp - Hypergraph Initialization and Mutex Tests
//=============================================================================
//
// WHAT: Verify proper hypergraph initialization, particularly mutex creation
//       with MUTEX_TYPE_RECURSIVE attribute
// WHY:  The mutex_attr_t initializer had trailing commas with no other fields,
//       relying on undefined zero-initialization behavior from calloc.
//       The fix ensures clean designated initializer syntax.
// HOW:  Create hypergraphs with thread safety enabled and verify:
//       1. Mutex is properly initialized (operations don't crash)
//       2. Thread-safe operations work correctly
//       3. Recursive mutex behavior works (re-entrant lock patterns)
//       4. Basic graph operations work after proper initialization
//
// REGRESSION: Fixes trailing comma in mutex_attr_t initializer at
//             nimcp_hypergraph.c lines 205-209
//=============================================================================

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/neuro_symbolic/nimcp_hypergraph.h"
#include "utils/exception/nimcp_exception.h"

class HypergraphInitTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_exception_clear_current();
    }

    void TearDown() override {
        nimcp_exception_clear_current();
    }
};

//=============================================================================
// MUTEX INITIALIZATION TESTS
//=============================================================================

TEST_F(HypergraphInitTest, CreateWithThreadSafety_Succeeds) {
    // Create hypergraph with thread safety enabled (triggers mutex creation)
    hypergraph_config_t config;
    nimcp_error_t err = nimcp_hypergraph_get_default_config(&config);
    ASSERT_EQ(err, NIMCP_OK);

    config.enable_thread_safety = true;

    nimcp_hypergraph_t* hg = nimcp_hypergraph_create_with_config(&config);
    ASSERT_NE(hg, nullptr)
        << "Failed to create hypergraph with thread safety enabled. "
        << "This may indicate mutex initialization failure.";

    nimcp_hypergraph_destroy(hg);
}

TEST_F(HypergraphInitTest, CreateWithoutThreadSafety_Succeeds) {
    // Create without thread safety (no mutex needed)
    hypergraph_config_t config;
    nimcp_error_t err = nimcp_hypergraph_get_default_config(&config);
    ASSERT_EQ(err, NIMCP_OK);

    config.enable_thread_safety = false;

    nimcp_hypergraph_t* hg = nimcp_hypergraph_create_with_config(&config);
    ASSERT_NE(hg, nullptr);

    nimcp_hypergraph_destroy(hg);
}

TEST_F(HypergraphInitTest, CreateDefault_Succeeds) {
    // Create with default config (nimcp_hypergraph_create)
    nimcp_hypergraph_t* hg = nimcp_hypergraph_create();
    ASSERT_NE(hg, nullptr);

    nimcp_hypergraph_destroy(hg);
}

//=============================================================================
// THREAD-SAFE OPERATIONS AFTER INIT - Verify mutex works correctly
//=============================================================================

TEST_F(HypergraphInitTest, ThreadSafe_AddVertex_Works) {
    hypergraph_config_t config;
    nimcp_hypergraph_get_default_config(&config);
    config.enable_thread_safety = true;

    nimcp_hypergraph_t* hg = nimcp_hypergraph_create_with_config(&config);
    ASSERT_NE(hg, nullptr);

    // Add vertices - these should lock/unlock the recursive mutex
    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "A", 1.0f);
    EXPECT_NE(v1, UINT32_MAX);

    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_VARIABLE, "x", 0.5f);
    EXPECT_NE(v2, UINT32_MAX);

    uint32_t v3 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_PREDICATE, "P", 0.8f);
    EXPECT_NE(v3, UINT32_MAX);

    // Verify counts
    EXPECT_EQ(nimcp_hypergraph_vertex_count(hg), 3u);

    nimcp_hypergraph_destroy(hg);
}

TEST_F(HypergraphInitTest, ThreadSafe_AddEdge_Works) {
    hypergraph_config_t config;
    nimcp_hypergraph_get_default_config(&config);
    config.enable_thread_safety = true;

    nimcp_hypergraph_t* hg = nimcp_hypergraph_create_with_config(&config);
    ASSERT_NE(hg, nullptr);

    // Add vertices
    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "A", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "B", 1.0f);
    uint32_t v3 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "C", 1.0f);
    ASSERT_NE(v1, UINT32_MAX);
    ASSERT_NE(v2, UINT32_MAX);
    ASSERT_NE(v3, UINT32_MAX);

    // Add hyperedge connecting all 3 vertices (n-ary relation)
    uint32_t vertices[] = { v1, v2, v3 };
    uint32_t edge_id = nimcp_hypergraph_add_edge(
        hg, HYPEREDGE_RELATION, vertices, 3, TRIT_POSITIVE, "Between");
    EXPECT_NE(edge_id, UINT32_MAX);

    // Verify edge count
    EXPECT_EQ(nimcp_hypergraph_edge_count(hg), 1u);

    nimcp_hypergraph_destroy(hg);
}

TEST_F(HypergraphInitTest, ThreadSafe_QueryOperations_Work) {
    hypergraph_config_t config;
    nimcp_hypergraph_get_default_config(&config);
    config.enable_thread_safety = true;
    config.enable_incidence_index = true;

    nimcp_hypergraph_t* hg = nimcp_hypergraph_create_with_config(&config);
    ASSERT_NE(hg, nullptr);

    // Build a small graph
    uint32_t v1 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "X", 1.0f);
    uint32_t v2 = nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "Y", 1.0f);
    ASSERT_NE(v1, UINT32_MAX);
    ASSERT_NE(v2, UINT32_MAX);

    uint32_t verts[] = { v1, v2 };
    uint32_t e1 = nimcp_hypergraph_add_edge(
        hg, HYPEREDGE_RELATION, verts, 2, TRIT_POSITIVE, "connected");
    EXPECT_NE(e1, UINT32_MAX);

    // Query: check connectivity (exercises read-lock path)
    bool connected = nimcp_hypergraph_are_connected(hg, v1, v2);
    EXPECT_TRUE(connected);

    // Get vertex (read operation)
    const nimcp_hypervertex_t* vertex = nimcp_hypergraph_get_vertex(hg, v1);
    EXPECT_NE(vertex, nullptr);
    if (vertex) {
        EXPECT_STREQ(vertex->label, "X");
    }

    nimcp_hypergraph_destroy(hg);
}

//=============================================================================
// RECURSIVE MUTEX BEHAVIOR - Verify MUTEX_TYPE_RECURSIVE is set
//=============================================================================

TEST_F(HypergraphInitTest, ThreadSafe_NestedOperations_NoDeadlock) {
    // Operations that internally may call other locking functions
    // (testing that the recursive mutex allows re-entrant locking)
    hypergraph_config_t config;
    nimcp_hypergraph_get_default_config(&config);
    config.enable_thread_safety = true;
    config.enable_incidence_index = true;

    nimcp_hypergraph_t* hg = nimcp_hypergraph_create_with_config(&config);
    ASSERT_NE(hg, nullptr);

    // Add vertices and edges in a pattern that exercises nested operations
    uint32_t ids[10];
    for (int i = 0; i < 10; i++) {
        char label[16];
        snprintf(label, sizeof(label), "V%d", i);
        ids[i] = nimcp_hypergraph_add_vertex(
            hg, HYPERVERTEX_CONSTANT, label, 1.0f);
        ASSERT_NE(ids[i], UINT32_MAX);
    }

    // Add edges with various arities
    for (int i = 0; i < 8; i++) {
        uint32_t verts[3] = { ids[i], ids[i+1], ids[i+2] };
        uint32_t eid = nimcp_hypergraph_add_edge(
            hg, HYPEREDGE_RELATION, verts, 3, TRIT_POSITIVE, "triple");
        EXPECT_NE(eid, UINT32_MAX);
    }

    // Contract an edge (internally removes edge and merges vertices - nested ops)
    // Get the first edge
    const nimcp_hyperedge_t* edge = nimcp_hypergraph_get_edge(hg, 1);
    if (edge) {
        // This exercises nested locking if the implementation acquires lock
        // during both remove and merge operations
        uint32_t merged = nimcp_hypergraph_contract_edge(hg, 1);
        // May succeed or fail depending on implementation, but should not deadlock
        (void)merged;
    }

    // Verify we can still operate on the graph (no corruption)
    uint32_t vc = nimcp_hypergraph_vertex_count(hg);
    EXPECT_GT(vc, 0u);

    nimcp_hypergraph_destroy(hg);
}

//=============================================================================
// STATISTICS AFTER INIT - Verify counters initialized correctly
//=============================================================================

TEST_F(HypergraphInitTest, FreshGraph_HasZeroCounts) {
    hypergraph_config_t config;
    nimcp_hypergraph_get_default_config(&config);
    config.enable_thread_safety = true;

    nimcp_hypergraph_t* hg = nimcp_hypergraph_create_with_config(&config);
    ASSERT_NE(hg, nullptr);

    EXPECT_EQ(nimcp_hypergraph_vertex_count(hg), 0u);
    EXPECT_EQ(nimcp_hypergraph_edge_count(hg), 0u);

    hypergraph_stats_t stats;
    nimcp_error_t err = nimcp_hypergraph_get_stats(hg, &stats);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_EQ(stats.vertex_count, 0u);
    EXPECT_EQ(stats.edge_count, 0u);
    EXPECT_EQ(stats.total_incidences, 0u);

    nimcp_hypergraph_destroy(hg);
}

TEST_F(HypergraphInitTest, Clear_ResetsToZero) {
    hypergraph_config_t config;
    nimcp_hypergraph_get_default_config(&config);
    config.enable_thread_safety = true;

    nimcp_hypergraph_t* hg = nimcp_hypergraph_create_with_config(&config);
    ASSERT_NE(hg, nullptr);

    // Add some data
    nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "A", 1.0f);
    nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, "B", 1.0f);
    EXPECT_EQ(nimcp_hypergraph_vertex_count(hg), 2u);

    // Clear
    nimcp_error_t err = nimcp_hypergraph_clear(hg);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_EQ(nimcp_hypergraph_vertex_count(hg), 0u);
    EXPECT_EQ(nimcp_hypergraph_edge_count(hg), 0u);

    nimcp_hypergraph_destroy(hg);
}

//=============================================================================
// DESTROY SAFETY
//=============================================================================

TEST_F(HypergraphInitTest, DestroyNull_NoSegfault) {
    // Should handle NULL gracefully
    nimcp_hypergraph_destroy(nullptr);
    SUCCEED();
}

TEST_F(HypergraphInitTest, DestroyThreadSafe_CleansUpMutex) {
    hypergraph_config_t config;
    nimcp_hypergraph_get_default_config(&config);
    config.enable_thread_safety = true;

    nimcp_hypergraph_t* hg = nimcp_hypergraph_create_with_config(&config);
    ASSERT_NE(hg, nullptr);

    // Add some data
    for (int i = 0; i < 5; i++) {
        char label[16];
        snprintf(label, sizeof(label), "V%d", i);
        nimcp_hypergraph_add_vertex(hg, HYPERVERTEX_CONSTANT, label, 1.0f);
    }

    // Destroy should clean up mutex without issues
    nimcp_hypergraph_destroy(hg);
    SUCCEED();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
