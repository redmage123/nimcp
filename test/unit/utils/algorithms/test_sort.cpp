/**
 * @file test_sort.cpp
 * @brief Unit tests for consolidated sorting and graph algorithms (nimcp_sort.h)
 * @version 1.0.0
 * @date 2026-01-03
 *
 * WHAT: Comprehensive tests for topological sort, comparison sort, and graph traversal
 * WHY:  Ensure correctness of consolidated algorithm implementations
 * HOW:  GTest test cases covering normal operation, edge cases, and error handling
 */

#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include <random>

#include "utils/algorithms/nimcp_sort.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class TopologicalSortTest : public ::testing::Test {
protected:
    /* Simple graph: 0 -> 1 -> 2 -> 3 (linear chain) */
    static uint32_t linear_get_dep_count(uint32_t node, void* data) {
        (void)data;
        return (node == 0) ? 0 : 1;  /* All nodes except 0 have one dependency */
    }

    static uint32_t linear_get_dep(uint32_t node, uint32_t dep_idx, void* data) {
        (void)data;
        (void)dep_idx;
        return (node == 0) ? UINT32_MAX : node - 1;
    }

    /* Diamond graph: 0 -> 1,2 -> 3 */
    static uint32_t diamond_get_dep_count(uint32_t node, void* data) {
        (void)data;
        switch (node) {
            case 0: return 0;
            case 1:
            case 2: return 1;
            case 3: return 2;
            default: return UINT32_MAX;
        }
    }

    static uint32_t diamond_get_dep(uint32_t node, uint32_t dep_idx, void* data) {
        (void)data;
        switch (node) {
            case 0: return UINT32_MAX;
            case 1:
            case 2: return 0;
            case 3: return (dep_idx == 0) ? 1 : 2;
            default: return UINT32_MAX;
        }
    }

    /* Cyclic graph: 0 -> 1 -> 2 -> 0 */
    static uint32_t cyclic_get_dep_count(uint32_t node, void* data) {
        (void)data;
        return 1;  /* Each node has exactly one dependency */
    }

    static uint32_t cyclic_get_dep(uint32_t node, uint32_t dep_idx, void* data) {
        (void)data;
        (void)dep_idx;
        /* 0 depends on 2, 1 depends on 0, 2 depends on 1 */
        return (node + 2) % 3;
    }
};

class ComparisonSortTest : public ::testing::Test {
protected:
    static int compare_int_asc(const void* a, const void* b) {
        int ia = *(const int*)a;
        int ib = *(const int*)b;
        return (ia > ib) - (ia < ib);
    }

    static int compare_int_desc(const void* a, const void* b) {
        int ia = *(const int*)a;
        int ib = *(const int*)b;
        return (ib > ia) - (ib < ia);
    }
};

class GraphTraversalTest : public ::testing::Test {
protected:
    /* Graph structure for tests */
    struct TestGraph {
        uint32_t num_nodes;
        std::vector<std::vector<uint32_t>> adjacency;
    };

    static uint32_t get_neighbor_count(uint32_t node, void* data) {
        TestGraph* g = (TestGraph*)data;
        if (node >= g->num_nodes) return UINT32_MAX;
        return (uint32_t)g->adjacency[node].size();
    }

    static uint32_t get_neighbor(uint32_t node, uint32_t idx, void* data) {
        TestGraph* g = (TestGraph*)data;
        if (node >= g->num_nodes) return UINT32_MAX;
        if (idx >= g->adjacency[node].size()) return UINT32_MAX;
        return g->adjacency[node][idx];
    }

    static bool collect_visit(uint32_t node, uint32_t depth, void* data) {
        std::vector<std::pair<uint32_t, uint32_t>>* visits =
            (std::vector<std::pair<uint32_t, uint32_t>>*)data;
        visits->push_back({node, depth});
        return true;
    }
};

/* ============================================================================
 * Topological Sort Tests
 * ============================================================================ */

TEST_F(TopologicalSortTest, LinearChain) {
    nimcp_topo_config_t config = {
        .node_count = 4,
        .user_data = nullptr,
        .get_dep_count = linear_get_dep_count,
        .get_dep = linear_get_dep,
        .get_dependent_count = nullptr,
        .get_dependent = nullptr
    };

    uint32_t order[4];
    uint32_t sorted_count = 0;

    nimcp_sort_result_t result = nimcp_topological_sort(&config, order, 4, &sorted_count);

    EXPECT_EQ(result, NIMCP_SORT_OK);
    EXPECT_EQ(sorted_count, 4u);

    /* Order must be 0, 1, 2, 3 */
    EXPECT_EQ(order[0], 0u);
    EXPECT_EQ(order[1], 1u);
    EXPECT_EQ(order[2], 2u);
    EXPECT_EQ(order[3], 3u);
}

TEST_F(TopologicalSortTest, DiamondGraph) {
    nimcp_topo_config_t config = {
        .node_count = 4,
        .user_data = nullptr,
        .get_dep_count = diamond_get_dep_count,
        .get_dep = diamond_get_dep,
        .get_dependent_count = nullptr,
        .get_dependent = nullptr
    };

    uint32_t order[4];
    uint32_t sorted_count = 0;

    nimcp_sort_result_t result = nimcp_topological_sort(&config, order, 4, &sorted_count);

    EXPECT_EQ(result, NIMCP_SORT_OK);
    EXPECT_EQ(sorted_count, 4u);

    /* 0 must come first, 3 must come last, 1 and 2 in between */
    EXPECT_EQ(order[0], 0u);
    EXPECT_EQ(order[3], 3u);
    EXPECT_TRUE(order[1] == 1 || order[1] == 2);
    EXPECT_TRUE(order[2] == 1 || order[2] == 2);
}

TEST_F(TopologicalSortTest, CycleDetection) {
    nimcp_topo_config_t config = {
        .node_count = 3,
        .user_data = nullptr,
        .get_dep_count = cyclic_get_dep_count,
        .get_dep = cyclic_get_dep,
        .get_dependent_count = nullptr,
        .get_dependent = nullptr
    };

    uint32_t order[3];
    uint32_t sorted_count = 0;

    nimcp_sort_result_t result = nimcp_topological_sort(&config, order, 3, &sorted_count);

    EXPECT_EQ(result, NIMCP_SORT_ERROR_CYCLE);
    EXPECT_LT(sorted_count, 3u);
}

TEST_F(TopologicalSortTest, EmptyGraph) {
    nimcp_topo_config_t config = {
        .node_count = 0,
        .user_data = nullptr,
        .get_dep_count = linear_get_dep_count,
        .get_dep = linear_get_dep,
        .get_dependent_count = nullptr,
        .get_dependent = nullptr
    };

    uint32_t order[1];
    uint32_t sorted_count = 99;

    nimcp_sort_result_t result = nimcp_topological_sort(&config, order, 1, &sorted_count);

    EXPECT_EQ(result, NIMCP_SORT_OK);
    EXPECT_EQ(sorted_count, 0u);
}

TEST_F(TopologicalSortTest, NullConfigReturnsError) {
    uint32_t order[4];
    uint32_t sorted_count = 0;

    nimcp_sort_result_t result = nimcp_topological_sort(nullptr, order, 4, &sorted_count);

    EXPECT_EQ(result, NIMCP_SORT_ERROR_NULL);
}

TEST_F(TopologicalSortTest, HasCycle) {
    nimcp_topo_config_t config = {
        .node_count = 3,
        .user_data = nullptr,
        .get_dep_count = cyclic_get_dep_count,
        .get_dep = cyclic_get_dep,
        .get_dependent_count = nullptr,
        .get_dependent = nullptr
    };

    EXPECT_TRUE(nimcp_has_cycle(&config));
}

TEST_F(TopologicalSortTest, NoCycle) {
    nimcp_topo_config_t config = {
        .node_count = 4,
        .user_data = nullptr,
        .get_dep_count = linear_get_dep_count,
        .get_dep = linear_get_dep,
        .get_dependent_count = nullptr,
        .get_dependent = nullptr
    };

    EXPECT_FALSE(nimcp_has_cycle(&config));
}

TEST_F(TopologicalSortTest, FindCycleNodes) {
    nimcp_topo_config_t config = {
        .node_count = 3,
        .user_data = nullptr,
        .get_dep_count = cyclic_get_dep_count,
        .get_dep = cyclic_get_dep,
        .get_dependent_count = nullptr,
        .get_dependent = nullptr
    };

    uint32_t cycle_nodes[3];
    uint32_t cycle_count = 0;

    nimcp_sort_result_t result = nimcp_find_cycle_nodes(&config, cycle_nodes, 3, &cycle_count);

    EXPECT_EQ(result, NIMCP_SORT_OK);
    EXPECT_EQ(cycle_count, 3u);  /* All nodes in cycle */
}

/* ============================================================================
 * Comparison Sort Tests
 * ============================================================================ */

TEST_F(ComparisonSortTest, SortIntegers) {
    int arr[] = {5, 2, 8, 1, 9, 3, 7, 4, 6, 0};
    int expected[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    nimcp_sort(arr, 10, sizeof(int), compare_int_asc);

    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(arr[i], expected[i]);
    }
}

TEST_F(ComparisonSortTest, SortDescending) {
    int arr[] = {5, 2, 8, 1, 9};
    int expected[] = {9, 8, 5, 2, 1};

    nimcp_sort(arr, 5, sizeof(int), compare_int_desc);

    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(arr[i], expected[i]);
    }
}

TEST_F(ComparisonSortTest, AlreadySorted) {
    int arr[] = {1, 2, 3, 4, 5};

    nimcp_sort(arr, 5, sizeof(int), compare_int_asc);

    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(arr[i], i + 1);
    }
}

TEST_F(ComparisonSortTest, ReverseSorted) {
    int arr[] = {5, 4, 3, 2, 1};
    int expected[] = {1, 2, 3, 4, 5};

    nimcp_sort(arr, 5, sizeof(int), compare_int_asc);

    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(arr[i], expected[i]);
    }
}

TEST_F(ComparisonSortTest, SingleElement) {
    int arr[] = {42};

    nimcp_sort(arr, 1, sizeof(int), compare_int_asc);

    EXPECT_EQ(arr[0], 42);
}

TEST_F(ComparisonSortTest, EmptyArray) {
    int arr[1] = {0};

    /* Should not crash */
    nimcp_sort(arr, 0, sizeof(int), compare_int_asc);
}

TEST_F(ComparisonSortTest, InsertionSortSmallArray) {
    int arr[] = {5, 2, 8, 1, 3};
    int expected[] = {1, 2, 3, 5, 8};

    nimcp_insertion_sort(arr, 5, sizeof(int), compare_int_asc);

    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(arr[i], expected[i]);
    }
}

TEST_F(ComparisonSortTest, LargeRandomArray) {
    std::vector<int> arr(1000);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 10000);

    for (int& x : arr) {
        x = dis(gen);
    }

    nimcp_sort(arr.data(), arr.size(), sizeof(int), compare_int_asc);

    for (size_t i = 1; i < arr.size(); i++) {
        EXPECT_LE(arr[i-1], arr[i]);
    }
}

/* ============================================================================
 * Graph Traversal Tests
 * ============================================================================ */

TEST_F(GraphTraversalTest, BFSLinearGraph) {
    TestGraph g;
    g.num_nodes = 4;
    g.adjacency = {{1}, {2}, {3}, {}};

    std::vector<std::pair<uint32_t, uint32_t>> visits;

    nimcp_traversal_config_t config = {
        .node_count = g.num_nodes,
        .start_node = 0,
        .user_data = &g,
        .get_neighbor_count = get_neighbor_count,
        .get_neighbor = get_neighbor,
        .visit = collect_visit,
        .visit_user_data = &visits
    };

    nimcp_sort_result_t result = nimcp_bfs(&config);

    EXPECT_EQ(result, NIMCP_SORT_OK);
    EXPECT_EQ(visits.size(), 4u);

    /* Verify BFS order and depths */
    EXPECT_EQ(visits[0].first, 0u);
    EXPECT_EQ(visits[0].second, 0u);  /* depth 0 */
    EXPECT_EQ(visits[1].first, 1u);
    EXPECT_EQ(visits[1].second, 1u);  /* depth 1 */
    EXPECT_EQ(visits[2].first, 2u);
    EXPECT_EQ(visits[2].second, 2u);  /* depth 2 */
    EXPECT_EQ(visits[3].first, 3u);
    EXPECT_EQ(visits[3].second, 3u);  /* depth 3 */
}

TEST_F(GraphTraversalTest, DFSLinearGraph) {
    TestGraph g;
    g.num_nodes = 4;
    g.adjacency = {{1}, {2}, {3}, {}};

    std::vector<std::pair<uint32_t, uint32_t>> visits;

    nimcp_traversal_config_t config = {
        .node_count = g.num_nodes,
        .start_node = 0,
        .user_data = &g,
        .get_neighbor_count = get_neighbor_count,
        .get_neighbor = get_neighbor,
        .visit = collect_visit,
        .visit_user_data = &visits
    };

    nimcp_sort_result_t result = nimcp_dfs(&config);

    EXPECT_EQ(result, NIMCP_SORT_OK);
    EXPECT_EQ(visits.size(), 4u);

    /* All nodes visited */
    std::set<uint32_t> visited_nodes;
    for (const auto& v : visits) {
        visited_nodes.insert(v.first);
    }
    EXPECT_EQ(visited_nodes.size(), 4u);
}

TEST_F(GraphTraversalTest, FindComponentsSingleComponent) {
    TestGraph g;
    g.num_nodes = 4;
    g.adjacency = {{1}, {0, 2}, {1, 3}, {2}};

    uint32_t component_ids[4];
    uint32_t num_components = 0;

    nimcp_traversal_config_t config = {
        .node_count = g.num_nodes,
        .start_node = 0,
        .user_data = &g,
        .get_neighbor_count = get_neighbor_count,
        .get_neighbor = get_neighbor,
        .visit = nullptr,
        .visit_user_data = nullptr
    };

    nimcp_sort_result_t result = nimcp_find_components(&config, component_ids, &num_components);

    EXPECT_EQ(result, NIMCP_SORT_OK);
    EXPECT_EQ(num_components, 1u);

    /* All nodes in same component */
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(component_ids[i], 0u);
    }
}

TEST_F(GraphTraversalTest, FindComponentsMultipleComponents) {
    TestGraph g;
    g.num_nodes = 6;
    g.adjacency = {
        {1}, {0},      /* Component 0: nodes 0, 1 */
        {3}, {2},      /* Component 1: nodes 2, 3 */
        {5}, {4}       /* Component 2: nodes 4, 5 */
    };

    uint32_t component_ids[6];
    uint32_t num_components = 0;

    nimcp_traversal_config_t config = {
        .node_count = g.num_nodes,
        .start_node = 0,
        .user_data = &g,
        .get_neighbor_count = get_neighbor_count,
        .get_neighbor = get_neighbor,
        .visit = nullptr,
        .visit_user_data = nullptr
    };

    nimcp_sort_result_t result = nimcp_find_components(&config, component_ids, &num_components);

    EXPECT_EQ(result, NIMCP_SORT_OK);
    EXPECT_EQ(num_components, 3u);

    /* Nodes 0,1 in same component */
    EXPECT_EQ(component_ids[0], component_ids[1]);
    /* Nodes 2,3 in same component */
    EXPECT_EQ(component_ids[2], component_ids[3]);
    /* Nodes 4,5 in same component */
    EXPECT_EQ(component_ids[4], component_ids[5]);
    /* Different components */
    EXPECT_NE(component_ids[0], component_ids[2]);
    EXPECT_NE(component_ids[2], component_ids[4]);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST(UtilityTest, ReverseArray) {
    uint32_t arr[] = {1, 2, 3, 4, 5};

    nimcp_reverse_u32(arr, 5);

    EXPECT_EQ(arr[0], 5u);
    EXPECT_EQ(arr[1], 4u);
    EXPECT_EQ(arr[2], 3u);
    EXPECT_EQ(arr[3], 2u);
    EXPECT_EQ(arr[4], 1u);
}

TEST(UtilityTest, ReverseEvenLength) {
    uint32_t arr[] = {1, 2, 3, 4};

    nimcp_reverse_u32(arr, 4);

    EXPECT_EQ(arr[0], 4u);
    EXPECT_EQ(arr[1], 3u);
    EXPECT_EQ(arr[2], 2u);
    EXPECT_EQ(arr[3], 1u);
}

TEST(UtilityTest, ReverseSingleElement) {
    uint32_t arr[] = {42};

    nimcp_reverse_u32(arr, 1);

    EXPECT_EQ(arr[0], 42u);
}

TEST(UtilityTest, BinarySearchFound) {
    uint32_t arr[] = {1, 3, 5, 7, 9, 11, 13};

    EXPECT_EQ(nimcp_binary_search_u32(arr, 7, 1), 0u);
    EXPECT_EQ(nimcp_binary_search_u32(arr, 7, 7), 3u);
    EXPECT_EQ(nimcp_binary_search_u32(arr, 7, 13), 6u);
}

TEST(UtilityTest, BinarySearchNotFound) {
    uint32_t arr[] = {1, 3, 5, 7, 9};

    EXPECT_EQ(nimcp_binary_search_u32(arr, 5, 0), UINT32_MAX);
    EXPECT_EQ(nimcp_binary_search_u32(arr, 5, 4), UINT32_MAX);
    EXPECT_EQ(nimcp_binary_search_u32(arr, 5, 100), UINT32_MAX);
}

TEST(UtilityTest, IsSorted) {
    uint32_t sorted[] = {1, 2, 3, 4, 5};
    uint32_t unsorted[] = {1, 3, 2, 4, 5};
    uint32_t equal[] = {5, 5, 5, 5, 5};

    EXPECT_TRUE(nimcp_is_sorted_u32(sorted, 5));
    EXPECT_FALSE(nimcp_is_sorted_u32(unsorted, 5));
    EXPECT_TRUE(nimcp_is_sorted_u32(equal, 5));
}

TEST(UtilityTest, IsSortedEmpty) {
    uint32_t arr[] = {1};

    EXPECT_TRUE(nimcp_is_sorted_u32(arr, 0));
    EXPECT_TRUE(nimcp_is_sorted_u32(arr, 1));
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(TopologicalSortTest, BufferTooSmall) {
    nimcp_topo_config_t config = {
        .node_count = 4,
        .user_data = nullptr,
        .get_dep_count = linear_get_dep_count,
        .get_dep = linear_get_dep,
        .get_dependent_count = nullptr,
        .get_dependent = nullptr
    };

    uint32_t order[2];  /* Too small */
    uint32_t sorted_count = 0;

    nimcp_sort_result_t result = nimcp_topological_sort(&config, order, 2, &sorted_count);

    EXPECT_EQ(result, NIMCP_SORT_ERROR_OVERFLOW);
}

TEST_F(GraphTraversalTest, InvalidStartNode) {
    TestGraph g;
    g.num_nodes = 4;
    g.adjacency = {{1}, {2}, {3}, {}};

    std::vector<std::pair<uint32_t, uint32_t>> visits;

    nimcp_traversal_config_t config = {
        .node_count = g.num_nodes,
        .start_node = 10,  /* Invalid */
        .user_data = &g,
        .get_neighbor_count = get_neighbor_count,
        .get_neighbor = get_neighbor,
        .visit = collect_visit,
        .visit_user_data = &visits
    };

    nimcp_sort_result_t result = nimcp_bfs(&config);

    EXPECT_EQ(result, NIMCP_SORT_ERROR_INVALID);
}
