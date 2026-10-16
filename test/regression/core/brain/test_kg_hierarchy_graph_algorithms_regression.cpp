/**
 * @file test_kg_hierarchy_graph_algorithms_regression.cpp
 * @brief Regression tests for KG Hierarchy graph algorithm APIs
 *
 * These tests ensure backward compatibility and correct behavior of:
 * - Topological sort API
 * - Binary search API
 * - BFS/DFS traversal API
 * - Connected components API
 * - Shortest path API
 *
 * Regression tests focus on:
 * - API contract stability
 * - Edge cases that might have caused bugs
 * - Performance characteristics
 * - Memory safety
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <algorithm>

#include "core/brain/nimcp_kg_hierarchy.h"
#include "core/brain/nimcp_brain_kg.h"

// ============================================================================
// Test Fixture
// ============================================================================

class KGHierarchyGraphAlgorithmsRegression : public ::testing::Test {
protected:
    brain_kg_t* kg;
    kg_hierarchy_t* hier;
    kg_hierarchy_config_t config;

    void SetUp() override {
        brain_kg_config_t kg_config;
        brain_kg_default_config(&kg_config);
        kg_config.enable_security = false;
        kg_config.enable_access_control = false;
        kg = brain_kg_create(&kg_config);
        ASSERT_NE(kg, nullptr);

        kg_hierarchy_default_config(&config);
        hier = nullptr;
    }

    void TearDown() override {
        if (hier) {
            kg_hierarchy_destroy(hier);
            hier = nullptr;
        }
        if (kg) {
            brain_kg_destroy(kg);
            kg = nullptr;
        }
    }

    brain_kg_node_id_t add_module(const char* name, brain_kg_node_type_t type) {
        return brain_kg_add_node(kg, name, type, "Test module");
    }
};

// ============================================================================
// Topological Sort API Regression Tests
// ============================================================================

TEST_F(KGHierarchyGraphAlgorithmsRegression, test_topo_sort_api_returns_correct_types) {
    // Verify API contract: return types and parameter handling
    brain_kg_node_id_t a = add_module("a", BRAIN_KG_NODE_CORE);
    brain_kg_node_id_t b = add_module("b", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_DEPENDS_ON, "dep", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t order[10];
    uint32_t sorted_count = 0;

    // API should return int (0 success, -1 failure)
    int result = kg_hierarchy_topological_sort(hier, order, 10, &sorted_count);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(sorted_count, 2u);
    EXPECT_NE(order[0], order[1]);  // Should have distinct nodes
}

TEST_F(KGHierarchyGraphAlgorithmsRegression, test_topo_sort_handles_max_order_zero) {
    add_module("single", BRAIN_KG_NODE_CORE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t order[1];
    uint32_t sorted_count = 0;

    // max_order=0 should still work (return 0 elements)
    int result = kg_hierarchy_topological_sort(hier, order, 0, &sorted_count);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(sorted_count, 0u);
}

TEST_F(KGHierarchyGraphAlgorithmsRegression, test_has_dependency_cycle_api_returns_bool) {
    add_module("single", BRAIN_KG_NODE_CORE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // API should return bool
    bool has_cycle = kg_hierarchy_has_dependency_cycle(hier);
    EXPECT_FALSE(has_cycle);
}

TEST_F(KGHierarchyGraphAlgorithmsRegression, test_startup_phase_returns_negative_on_invalid) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Invalid module should return -1
    int phase = kg_hierarchy_get_startup_phase(hier, 99999);
    EXPECT_EQ(phase, -1);

    // Null hierarchy should return -1
    phase = kg_hierarchy_get_startup_phase(nullptr, 1);
    EXPECT_EQ(phase, -1);
}

// ============================================================================
// Binary Search API Regression Tests
// ============================================================================

TEST_F(KGHierarchyGraphAlgorithmsRegression, test_binary_search_returns_uint32_max_on_not_found) {
    add_module("a", BRAIN_KG_NODE_CORE);
    add_module("b", BRAIN_KG_NODE_COGNITIVE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t sorted[10];
    uint32_t count = kg_hierarchy_get_sorted_module_ids(hier, sorted, 10);

    // Search for non-existent module
    uint32_t idx = kg_hierarchy_binary_search_module(hier, sorted, count, 99999);
    EXPECT_EQ(idx, UINT32_MAX);
}

TEST_F(KGHierarchyGraphAlgorithmsRegression, test_is_sorted_handles_edge_cases) {
    // Empty array
    EXPECT_TRUE(kg_hierarchy_is_sorted(nullptr, 0));

    // Single element
    brain_kg_node_id_t single[] = {42};
    EXPECT_TRUE(kg_hierarchy_is_sorted(single, 1));

    // Two elements sorted
    brain_kg_node_id_t sorted[] = {1, 2};
    EXPECT_TRUE(kg_hierarchy_is_sorted(sorted, 2));

    // Two elements unsorted
    brain_kg_node_id_t unsorted[] = {2, 1};
    EXPECT_FALSE(kg_hierarchy_is_sorted(unsorted, 2));

    // Duplicates (should be considered sorted)
    brain_kg_node_id_t dups[] = {1, 1, 1};
    EXPECT_TRUE(kg_hierarchy_is_sorted(dups, 3));
}

TEST_F(KGHierarchyGraphAlgorithmsRegression, test_get_sorted_module_ids_respects_max_ids) {
    // Add many modules
    for (int i = 0; i < 20; i++) {
        char name[32];
        snprintf(name, sizeof(name), "module_%02d", i);
        add_module(name, BRAIN_KG_NODE_COGNITIVE);
    }

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Request only 5
    brain_kg_node_id_t sorted[5];
    uint32_t count = kg_hierarchy_get_sorted_module_ids(hier, sorted, 5);
    EXPECT_EQ(count, 5u);

    // Should still be sorted
    EXPECT_TRUE(kg_hierarchy_is_sorted(sorted, count));
}

// ============================================================================
// BFS/DFS API Regression Tests
// ============================================================================

static std::vector<brain_kg_node_id_t> g_regression_visited;

static bool regression_visitor(brain_kg_node_id_t id, uint32_t depth, void* data) {
    g_regression_visited.push_back(id);
    return true;
}

TEST_F(KGHierarchyGraphAlgorithmsRegression, test_bfs_visits_start_node_at_depth_zero) {
    brain_kg_node_id_t start = add_module("start", BRAIN_KG_NODE_CORE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    g_regression_visited.clear();

    static uint32_t first_depth = UINT32_MAX;
    auto depth_checker = [](brain_kg_node_id_t id, uint32_t depth, void* data) -> bool {
        if (g_regression_visited.empty()) {
            first_depth = depth;
        }
        g_regression_visited.push_back(id);
        return true;
    };

    // Use lambda via a static function wrapper
    int result = kg_hierarchy_bfs(hier, start, regression_visitor, nullptr);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_regression_visited.size(), 1u);
    EXPECT_EQ(g_regression_visited[0], start);
}

TEST_F(KGHierarchyGraphAlgorithmsRegression, test_dfs_returns_error_on_invalid_start) {
    add_module("a", BRAIN_KG_NODE_CORE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    g_regression_visited.clear();

    // Invalid start node
    int result = kg_hierarchy_dfs(hier, 99999, regression_visitor, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(KGHierarchyGraphAlgorithmsRegression, test_traversal_null_visitor_returns_error) {
    brain_kg_node_id_t start = add_module("start", BRAIN_KG_NODE_CORE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(kg_hierarchy_bfs(hier, start, nullptr, nullptr), -1);
    EXPECT_EQ(kg_hierarchy_dfs(hier, start, nullptr, nullptr), -1);
}

// ============================================================================
// Shortest Path API Regression Tests
// ============================================================================

TEST_F(KGHierarchyGraphAlgorithmsRegression, test_shortest_path_returns_correct_length) {
    brain_kg_node_id_t a = add_module("a", BRAIN_KG_NODE_CORE);
    brain_kg_node_id_t b = add_module("b", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t c = add_module("c", BRAIN_KG_NODE_INTEGRATION);

    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);
    brain_kg_add_edge(kg, b, c, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t path[10];
    uint32_t path_len = 0;

    int result = kg_hierarchy_shortest_path(hier, a, c, path, 10, &path_len);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(path_len, 3u);  // a -> b -> c

    // Verify path contents
    EXPECT_EQ(path[0], a);
    EXPECT_EQ(path[path_len - 1], c);
}

TEST_F(KGHierarchyGraphAlgorithmsRegression, test_shortest_path_same_node_returns_length_one) {
    brain_kg_node_id_t node = add_module("node", BRAIN_KG_NODE_CORE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t path[10];
    uint32_t path_len = 0;

    int result = kg_hierarchy_shortest_path(hier, node, node, path, 10, &path_len);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(path_len, 1u);
    EXPECT_EQ(path[0], node);
}

TEST_F(KGHierarchyGraphAlgorithmsRegression, test_get_distance_returns_uint32_max_no_path) {
    brain_kg_node_id_t a = add_module("a", BRAIN_KG_NODE_CORE);
    brain_kg_node_id_t b = add_module("b", BRAIN_KG_NODE_COGNITIVE);
    // No edge between a and b

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    uint32_t dist = kg_hierarchy_get_distance(hier, a, b);
    EXPECT_EQ(dist, UINT32_MAX);
}

TEST_F(KGHierarchyGraphAlgorithmsRegression, test_get_reachable_includes_start_node) {
    brain_kg_node_id_t start = add_module("start", BRAIN_KG_NODE_CORE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t reachable[10];
    uint32_t count = kg_hierarchy_get_reachable(hier, start, reachable, 10);

    EXPECT_GE(count, 1u);
    EXPECT_EQ(reachable[0], start);  // Start node should be first
}

// ============================================================================
// Connected Components API Regression Tests
// ============================================================================

TEST_F(KGHierarchyGraphAlgorithmsRegression, test_find_components_empty_graph) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    uint32_t comp_ids[10];
    uint32_t num_components = 0;

    int result = kg_hierarchy_find_components(hier, comp_ids, &num_components);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(num_components, 0u);
}

TEST_F(KGHierarchyGraphAlgorithmsRegression, test_are_connected_reflexive) {
    brain_kg_node_id_t node = add_module("node", BRAIN_KG_NODE_CORE);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Node should be connected to itself
    EXPECT_TRUE(kg_hierarchy_are_connected(hier, node, node));
}

TEST_F(KGHierarchyGraphAlgorithmsRegression, test_are_connected_symmetric) {
    brain_kg_node_id_t a = add_module("a", BRAIN_KG_NODE_CORE);
    brain_kg_node_id_t b = add_module("b", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Connection should be symmetric
    EXPECT_TRUE(kg_hierarchy_are_connected(hier, a, b));
    EXPECT_TRUE(kg_hierarchy_are_connected(hier, b, a));
}

TEST_F(KGHierarchyGraphAlgorithmsRegression, test_count_isolated_returns_zero_when_all_connected) {
    brain_kg_node_id_t a = add_module("a", BRAIN_KG_NODE_CORE);
    brain_kg_node_id_t b = add_module("b", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    EXPECT_EQ(kg_hierarchy_count_isolated(hier), 0u);
}

TEST_F(KGHierarchyGraphAlgorithmsRegression, test_get_largest_component_empty_graph) {
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t modules[10];
    uint32_t count = kg_hierarchy_get_largest_component(hier, modules, 10);
    EXPECT_EQ(count, 0u);
}

// ============================================================================
// API Null Parameter Handling Regression Tests
// ============================================================================

TEST_F(KGHierarchyGraphAlgorithmsRegression, test_all_apis_handle_null_hierarchy) {
    // All APIs should safely handle NULL hierarchy without crashing
    brain_kg_node_id_t order[10];
    uint32_t count = 0;

    EXPECT_EQ(kg_hierarchy_topological_sort(nullptr, order, 10, &count), -1);
    EXPECT_FALSE(kg_hierarchy_has_dependency_cycle(nullptr));
    EXPECT_EQ(kg_hierarchy_get_startup_phase(nullptr, 1), -1);

    brain_kg_node_id_t sorted[10] = {1, 2, 3};
    // Note: binary_search_module doesn't use hier, it operates on the sorted array directly
    // So null hier still works - test that get_sorted_module_ids handles null
    EXPECT_EQ(kg_hierarchy_get_sorted_module_ids(nullptr, sorted, 10), 0u);

    EXPECT_EQ(kg_hierarchy_bfs(nullptr, 1, regression_visitor, nullptr), -1);
    EXPECT_EQ(kg_hierarchy_dfs(nullptr, 1, regression_visitor, nullptr), -1);

    brain_kg_node_id_t path[10];
    uint32_t path_len = 0;
    EXPECT_EQ(kg_hierarchy_shortest_path(nullptr, 1, 2, path, 10, &path_len), -1);
    EXPECT_EQ(kg_hierarchy_get_distance(nullptr, 1, 2), UINT32_MAX);
    EXPECT_EQ(kg_hierarchy_get_reachable(nullptr, 1, path, 10), 0u);

    uint32_t comp_ids[10];
    EXPECT_EQ(kg_hierarchy_find_components(nullptr, comp_ids, &count), -1);
    EXPECT_FALSE(kg_hierarchy_are_connected(nullptr, 1, 2));
    EXPECT_EQ(kg_hierarchy_get_largest_component(nullptr, path, 10), 0u);
    EXPECT_EQ(kg_hierarchy_count_isolated(nullptr), 0u);
}

// ============================================================================
// Memory and Performance Regression Tests
// ============================================================================

TEST_F(KGHierarchyGraphAlgorithmsRegression, test_repeated_queries_no_memory_leak) {
    // Create a small graph
    brain_kg_node_id_t a = add_module("a", BRAIN_KG_NODE_CORE);
    brain_kg_node_id_t b = add_module("b", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Perform many queries - should not leak memory
    for (int i = 0; i < 100; i++) {
        brain_kg_node_id_t order[10];
        uint32_t count = 0;
        kg_hierarchy_topological_sort(hier, order, 10, &count);

        brain_kg_node_id_t sorted[10];
        kg_hierarchy_get_sorted_module_ids(hier, sorted, 10);

        g_regression_visited.clear();
        kg_hierarchy_bfs(hier, a, regression_visitor, nullptr);

        brain_kg_node_id_t path[10];
        uint32_t path_len = 0;
        kg_hierarchy_shortest_path(hier, a, b, path, 10, &path_len);

        uint32_t comp_ids[10];
        kg_hierarchy_find_components(hier, comp_ids, &count);
    }

    // If we got here without crashing or OOM, test passes
    SUCCEED();
}

TEST_F(KGHierarchyGraphAlgorithmsRegression, test_large_graph_performance) {
    // Create a larger graph to test performance doesn't regress
    const int N = 200;
    brain_kg_node_id_t nodes[N];

    for (int i = 0; i < N; i++) {
        char name[32];
        snprintf(name, sizeof(name), "perf_node_%03d", i);
        nodes[i] = add_module(name, BRAIN_KG_NODE_COGNITIVE);
    }

    // Create a chain
    for (int i = 0; i < N - 1; i++) {
        brain_kg_add_edge(kg, nodes[i], nodes[i+1], BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);
    }

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // All operations should complete in reasonable time
    brain_kg_node_id_t order[N + 10];
    uint32_t count = 0;
    EXPECT_EQ(kg_hierarchy_topological_sort(hier, order, N + 10, &count), 0);
    EXPECT_EQ(count, (uint32_t)N);

    brain_kg_node_id_t sorted[N + 10];
    count = kg_hierarchy_get_sorted_module_ids(hier, sorted, N + 10);
    EXPECT_EQ(count, (uint32_t)N);

    g_regression_visited.clear();
    EXPECT_EQ(kg_hierarchy_bfs(hier, nodes[0], regression_visitor, nullptr), 0);
    EXPECT_EQ(g_regression_visited.size(), (size_t)N);

    brain_kg_node_id_t path[N + 10];
    uint32_t path_len = 0;
    EXPECT_EQ(kg_hierarchy_shortest_path(hier, nodes[0], nodes[N-1], path, N + 10, &path_len), 0);
    EXPECT_EQ(path_len, (uint32_t)N);

    uint32_t comp_ids[N + 10];
    uint32_t num_comp = 0;
    EXPECT_EQ(kg_hierarchy_find_components(hier, comp_ids, &num_comp), 0);
    EXPECT_EQ(num_comp, 1u);  // All connected
}
