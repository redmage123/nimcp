/**
 * @file test_kg_hierarchy_graph_algorithms_integration.cpp
 * @brief Integration tests for KG Hierarchy graph algorithm functionality
 *
 * Tests integration of:
 * - Topological sort with brain module dependencies
 * - BFS/DFS traversal across module connections
 * - Shortest path computation in module graphs
 * - Connected components with brain architecture
 * - Binary search with sorted module lists
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <atomic>
#include <vector>
#include <set>

#include "core/brain/nimcp_kg_hierarchy.h"
#include "core/brain/nimcp_brain_kg.h"

// ============================================================================
// Test Fixture
// ============================================================================

class KGHierarchyGraphAlgorithmsIntegration : public ::testing::Test {
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

    // Create a realistic brain module topology for integration testing
    void create_brain_topology() {
        // Core modules (foundation layer)
        brain_kg_node_id_t core_init = add_module("core_init", BRAIN_KG_NODE_CORE);
        brain_kg_node_id_t memory_manager = add_module("memory_manager", BRAIN_KG_NODE_CORE);
        brain_kg_node_id_t event_bus = add_module("event_bus", BRAIN_KG_NODE_CORE);

        // Perception modules (depend on core)
        brain_kg_node_id_t visual_cortex = add_module("visual_cortex", BRAIN_KG_NODE_PERCEPTION);
        brain_kg_node_id_t auditory_cortex = add_module("auditory_cortex", BRAIN_KG_NODE_PERCEPTION);
        brain_kg_node_id_t somatosensory = add_module("somatosensory", BRAIN_KG_NODE_PERCEPTION);

        // Cognitive modules (depend on perception)
        brain_kg_node_id_t prefrontal = add_module("prefrontal", BRAIN_KG_NODE_COGNITIVE);
        brain_kg_node_id_t hippocampus = add_module("hippocampus", BRAIN_KG_NODE_COGNITIVE);
        brain_kg_node_id_t amygdala = add_module("amygdala", BRAIN_KG_NODE_COGNITIVE);

        // Integration modules (depend on cognitive)
        brain_kg_node_id_t global_workspace = add_module("global_workspace", BRAIN_KG_NODE_INTEGRATION);
        brain_kg_node_id_t coordinator = add_module("coordinator", BRAIN_KG_NODE_COORDINATOR);

        // Set up dependencies (startup order)
        // Core infrastructure connections
        brain_kg_add_edge(kg, core_init, memory_manager, BRAIN_KG_EDGE_DEPENDS_ON, "mem", 1.0f);
        brain_kg_add_edge(kg, core_init, event_bus, BRAIN_KG_EDGE_DEPENDS_ON, "bus", 1.0f);

        // Perception depends on core
        brain_kg_add_edge(kg, core_init, visual_cortex, BRAIN_KG_EDGE_DEPENDS_ON, "init", 1.0f);
        brain_kg_add_edge(kg, core_init, auditory_cortex, BRAIN_KG_EDGE_DEPENDS_ON, "init", 1.0f);
        brain_kg_add_edge(kg, core_init, somatosensory, BRAIN_KG_EDGE_DEPENDS_ON, "init", 1.0f);
        brain_kg_add_edge(kg, memory_manager, hippocampus, BRAIN_KG_EDGE_DEPENDS_ON, "mem", 1.0f);
        brain_kg_add_edge(kg, event_bus, visual_cortex, BRAIN_KG_EDGE_DEPENDS_ON, "events", 1.0f);
        brain_kg_add_edge(kg, event_bus, auditory_cortex, BRAIN_KG_EDGE_DEPENDS_ON, "events", 1.0f);

        // Cognitive depends on perception
        brain_kg_add_edge(kg, visual_cortex, prefrontal, BRAIN_KG_EDGE_DEPENDS_ON, "visual", 1.0f);
        brain_kg_add_edge(kg, auditory_cortex, prefrontal, BRAIN_KG_EDGE_DEPENDS_ON, "audio", 1.0f);
        brain_kg_add_edge(kg, visual_cortex, hippocampus, BRAIN_KG_EDGE_DEPENDS_ON, "mem", 1.0f);
        brain_kg_add_edge(kg, somatosensory, amygdala, BRAIN_KG_EDGE_DEPENDS_ON, "sense", 1.0f);

        // Integration depends on cognitive
        brain_kg_add_edge(kg, prefrontal, global_workspace, BRAIN_KG_EDGE_DEPENDS_ON, "pfc", 1.0f);
        brain_kg_add_edge(kg, hippocampus, global_workspace, BRAIN_KG_EDGE_DEPENDS_ON, "hip", 1.0f);
        brain_kg_add_edge(kg, amygdala, global_workspace, BRAIN_KG_EDGE_DEPENDS_ON, "amy", 1.0f);
        brain_kg_add_edge(kg, global_workspace, coordinator, BRAIN_KG_EDGE_DEPENDS_ON, "gw", 1.0f);

        // Data flow connections (bidirectional communication)
        brain_kg_add_edge(kg, visual_cortex, prefrontal, BRAIN_KG_EDGE_CONNECTS_TO, "vis->pfc", 0.8f);
        brain_kg_add_edge(kg, prefrontal, hippocampus, BRAIN_KG_EDGE_CONNECTS_TO, "pfc->hip", 0.9f);
        brain_kg_add_edge(kg, hippocampus, amygdala, BRAIN_KG_EDGE_CONNECTS_TO, "hip->amy", 0.7f);
        brain_kg_add_edge(kg, amygdala, prefrontal, BRAIN_KG_EDGE_CONNECTS_TO, "amy->pfc", 0.75f);
        brain_kg_add_edge(kg, prefrontal, global_workspace, BRAIN_KG_EDGE_CONNECTS_TO, "pfc->gw", 1.0f);
        brain_kg_add_edge(kg, hippocampus, global_workspace, BRAIN_KG_EDGE_CONNECTS_TO, "hip->gw", 0.85f);
    }
};

// ============================================================================
// Topological Sort Integration Tests
// ============================================================================

TEST_F(KGHierarchyGraphAlgorithmsIntegration, test_topological_sort_brain_startup_order) {
    create_brain_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t order[20];
    uint32_t sorted_count = 0;

    int result = kg_hierarchy_topological_sort(hier, order, 20, &sorted_count);
    EXPECT_EQ(result, 0);
    EXPECT_GT(sorted_count, 0u);

    // Verify no cycles detected
    EXPECT_FALSE(kg_hierarchy_has_dependency_cycle(hier));

    // Find key modules in sorted order
    int core_init_pos = -1;
    int visual_pos = -1;
    int prefrontal_pos = -1;
    int gw_pos = -1;

    brain_kg_node_id_t core_init = kg_hierarchy_find_module_by_name(hier, "core_init");
    brain_kg_node_id_t visual = kg_hierarchy_find_module_by_name(hier, "visual_cortex");
    brain_kg_node_id_t prefrontal = kg_hierarchy_find_module_by_name(hier, "prefrontal");
    brain_kg_node_id_t gw = kg_hierarchy_find_module_by_name(hier, "global_workspace");

    for (uint32_t i = 0; i < sorted_count; i++) {
        if (order[i] == core_init) core_init_pos = (int)i;
        if (order[i] == visual) visual_pos = (int)i;
        if (order[i] == prefrontal) prefrontal_pos = (int)i;
        if (order[i] == gw) gw_pos = (int)i;
    }

    // Verify correct startup order: core -> perception -> cognitive -> integration
    if (core_init_pos >= 0 && visual_pos >= 0) {
        EXPECT_LT(core_init_pos, visual_pos) << "Core init should start before visual cortex";
    }
    if (visual_pos >= 0 && prefrontal_pos >= 0) {
        EXPECT_LT(visual_pos, prefrontal_pos) << "Visual cortex should start before prefrontal";
    }
    if (prefrontal_pos >= 0 && gw_pos >= 0) {
        EXPECT_LT(prefrontal_pos, gw_pos) << "Prefrontal should start before global workspace";
    }
}

TEST_F(KGHierarchyGraphAlgorithmsIntegration, test_startup_phases_for_brain_modules) {
    create_brain_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t core_init = kg_hierarchy_find_module_by_name(hier, "core_init");
    brain_kg_node_id_t coordinator = kg_hierarchy_find_module_by_name(hier, "coordinator");

    int core_phase = kg_hierarchy_get_startup_phase(hier, core_init);
    int coord_phase = kg_hierarchy_get_startup_phase(hier, coordinator);

    EXPECT_GE(core_phase, 0);
    EXPECT_GE(coord_phase, 0);

    // Coordinator should be in a later phase than core_init
    EXPECT_LE(core_phase, coord_phase);
}

// ============================================================================
// BFS/DFS Traversal Integration Tests
// ============================================================================

static std::vector<brain_kg_node_id_t> g_traversal_order;
static std::set<brain_kg_node_id_t> g_visited_set;

static bool integration_visitor(brain_kg_node_id_t module_id, uint32_t depth, void* user_data) {
    g_traversal_order.push_back(module_id);
    g_visited_set.insert(module_id);
    return true;
}

TEST_F(KGHierarchyGraphAlgorithmsIntegration, test_bfs_from_core_module) {
    create_brain_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t core_init = kg_hierarchy_find_module_by_name(hier, "core_init");
    ASSERT_NE(core_init, BRAIN_KG_INVALID_NODE);

    g_traversal_order.clear();
    g_visited_set.clear();

    int result = kg_hierarchy_bfs(hier, core_init, integration_visitor, nullptr);
    EXPECT_EQ(result, 0);

    // BFS from core_init should reach multiple modules through dependency edges
    EXPECT_GE(g_traversal_order.size(), 1u);

    // Verify core_init was visited first
    EXPECT_EQ(g_traversal_order[0], core_init);
}

TEST_F(KGHierarchyGraphAlgorithmsIntegration, test_dfs_explores_deep_paths) {
    create_brain_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t prefrontal = kg_hierarchy_find_module_by_name(hier, "prefrontal");
    ASSERT_NE(prefrontal, BRAIN_KG_INVALID_NODE);

    g_traversal_order.clear();
    g_visited_set.clear();

    int result = kg_hierarchy_dfs(hier, prefrontal, integration_visitor, nullptr);
    EXPECT_EQ(result, 0);

    // DFS from prefrontal should visit connected modules
    EXPECT_GE(g_traversal_order.size(), 1u);
    EXPECT_EQ(g_traversal_order[0], prefrontal);
}

// ============================================================================
// Shortest Path Integration Tests
// ============================================================================

TEST_F(KGHierarchyGraphAlgorithmsIntegration, test_shortest_path_in_brain_topology) {
    create_brain_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t visual = kg_hierarchy_find_module_by_name(hier, "visual_cortex");
    brain_kg_node_id_t gw = kg_hierarchy_find_module_by_name(hier, "global_workspace");

    ASSERT_NE(visual, BRAIN_KG_INVALID_NODE);
    ASSERT_NE(gw, BRAIN_KG_INVALID_NODE);

    brain_kg_node_id_t path[20];
    uint32_t path_len = 0;

    int result = kg_hierarchy_shortest_path(hier, visual, gw, path, 20, &path_len);
    EXPECT_EQ(result, 0);
    EXPECT_GT(path_len, 1u);

    // Path should start at visual and end at global_workspace
    EXPECT_EQ(path[0], visual);
    EXPECT_EQ(path[path_len - 1], gw);

    // Verify path continuity (each step should have an edge)
    for (uint32_t i = 0; i < path_len - 1; i++) {
        uint32_t dist = kg_hierarchy_get_distance(hier, path[i], path[i + 1]);
        EXPECT_EQ(dist, 1u) << "Each step in path should have distance 1";
    }
}

TEST_F(KGHierarchyGraphAlgorithmsIntegration, test_distance_between_modules) {
    create_brain_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t prefrontal = kg_hierarchy_find_module_by_name(hier, "prefrontal");
    brain_kg_node_id_t hippocampus = kg_hierarchy_find_module_by_name(hier, "hippocampus");

    ASSERT_NE(prefrontal, BRAIN_KG_INVALID_NODE);
    ASSERT_NE(hippocampus, BRAIN_KG_INVALID_NODE);

    // They are directly connected via CONNECTS_TO edge
    uint32_t dist = kg_hierarchy_get_distance(hier, prefrontal, hippocampus);
    EXPECT_EQ(dist, 1u);
}

TEST_F(KGHierarchyGraphAlgorithmsIntegration, test_reachable_modules_from_prefrontal) {
    create_brain_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t prefrontal = kg_hierarchy_find_module_by_name(hier, "prefrontal");
    ASSERT_NE(prefrontal, BRAIN_KG_INVALID_NODE);

    brain_kg_node_id_t reachable[20];
    uint32_t count = kg_hierarchy_get_reachable(hier, prefrontal, reachable, 20);

    EXPECT_GT(count, 0u);

    // Prefrontal should reach global_workspace and hippocampus (directly connected)
    bool found_gw = false;
    bool found_hip = false;

    brain_kg_node_id_t gw = kg_hierarchy_find_module_by_name(hier, "global_workspace");
    brain_kg_node_id_t hip = kg_hierarchy_find_module_by_name(hier, "hippocampus");

    for (uint32_t i = 0; i < count; i++) {
        if (reachable[i] == gw) found_gw = true;
        if (reachable[i] == hip) found_hip = true;
    }

    EXPECT_TRUE(found_gw || found_hip) << "Prefrontal should reach at least some connected modules";
}

// ============================================================================
// Connected Components Integration Tests
// ============================================================================

TEST_F(KGHierarchyGraphAlgorithmsIntegration, test_connected_components_in_brain) {
    create_brain_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    uint32_t comp_ids[20];
    uint32_t num_components = 0;

    int result = kg_hierarchy_find_components(hier, comp_ids, &num_components);
    EXPECT_EQ(result, 0);

    // All brain modules should be in the same component (connected brain)
    EXPECT_EQ(num_components, 1u) << "Brain topology should be fully connected (1 component)";
}

TEST_F(KGHierarchyGraphAlgorithmsIntegration, test_isolated_module_creates_new_component) {
    create_brain_topology();

    // Add an isolated module (no connections)
    add_module("isolated_sensor", BRAIN_KG_NODE_PERCEPTION);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    uint32_t comp_ids[20];
    uint32_t num_components = 0;

    int result = kg_hierarchy_find_components(hier, comp_ids, &num_components);
    EXPECT_EQ(result, 0);

    // Now we should have 2 components: main brain + isolated module
    EXPECT_EQ(num_components, 2u);

    // Verify isolated module count
    uint32_t isolated = kg_hierarchy_count_isolated(hier);
    EXPECT_EQ(isolated, 1u);
}

TEST_F(KGHierarchyGraphAlgorithmsIntegration, test_are_connected_integration) {
    create_brain_topology();

    // Add an isolated module
    brain_kg_node_id_t isolated = add_module("isolated_module", BRAIN_KG_NODE_CORE);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t prefrontal = kg_hierarchy_find_module_by_name(hier, "prefrontal");
    brain_kg_node_id_t hippocampus = kg_hierarchy_find_module_by_name(hier, "hippocampus");

    // Prefrontal and hippocampus should be connected (part of main brain)
    EXPECT_TRUE(kg_hierarchy_are_connected(hier, prefrontal, hippocampus));

    // Isolated module should not be connected to prefrontal
    EXPECT_FALSE(kg_hierarchy_are_connected(hier, prefrontal, isolated));
}

TEST_F(KGHierarchyGraphAlgorithmsIntegration, test_largest_component) {
    create_brain_topology();

    // Add a small disconnected subgraph
    brain_kg_node_id_t sub1 = add_module("sub_module_1", BRAIN_KG_NODE_CORE);
    brain_kg_node_id_t sub2 = add_module("sub_module_2", BRAIN_KG_NODE_CORE);
    brain_kg_add_edge(kg, sub1, sub2, BRAIN_KG_EDGE_CONNECTS_TO, "test", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t largest[20];
    uint32_t count = kg_hierarchy_get_largest_component(hier, largest, 20);

    // Main brain topology has 11 modules, subgraph has 2
    EXPECT_EQ(count, 11u) << "Largest component should be the main brain (11 modules)";
}

// ============================================================================
// Binary Search Integration Tests
// ============================================================================

TEST_F(KGHierarchyGraphAlgorithmsIntegration, test_binary_search_all_modules) {
    create_brain_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t sorted[20];
    uint32_t count = kg_hierarchy_get_sorted_module_ids(hier, sorted, 20);

    EXPECT_EQ(count, 11u);  // 11 modules in brain topology
    EXPECT_TRUE(kg_hierarchy_is_sorted(sorted, count));

    // Search for each module in sorted array
    brain_kg_node_id_t prefrontal = kg_hierarchy_find_module_by_name(hier, "prefrontal");
    uint32_t idx = kg_hierarchy_binary_search_module(hier, sorted, count, prefrontal);

    EXPECT_NE(idx, UINT32_MAX);
    EXPECT_EQ(sorted[idx], prefrontal);
}

TEST_F(KGHierarchyGraphAlgorithmsIntegration, test_binary_search_performance) {
    // Create a larger graph for performance testing
    const int N = 100;
    for (int i = 0; i < N; i++) {
        char name[32];
        snprintf(name, sizeof(name), "perf_module_%03d", i);
        add_module(name, BRAIN_KG_NODE_COGNITIVE);
    }

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    brain_kg_node_id_t sorted[N + 10];
    uint32_t count = kg_hierarchy_get_sorted_module_ids(hier, sorted, N + 10);

    EXPECT_EQ(count, (uint32_t)N);
    EXPECT_TRUE(kg_hierarchy_is_sorted(sorted, count));

    // Binary search should find all modules
    for (uint32_t i = 0; i < count; i++) {
        uint32_t found_idx = kg_hierarchy_binary_search_module(hier, sorted, count, sorted[i]);
        EXPECT_EQ(found_idx, i);
    }
}

// ============================================================================
// Complex Graph Scenario Integration Tests
// ============================================================================

TEST_F(KGHierarchyGraphAlgorithmsIntegration, test_complex_dependency_graph) {
    // Create a more complex dependency graph with diamond patterns
    brain_kg_node_id_t root = add_module("root", BRAIN_KG_NODE_CORE);
    brain_kg_node_id_t left = add_module("left_branch", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t right = add_module("right_branch", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t merge = add_module("merge_point", BRAIN_KG_NODE_INTEGRATION);
    brain_kg_node_id_t final_module = add_module("final", BRAIN_KG_NODE_COORDINATOR);

    // Diamond dependency: root -> (left, right) -> merge -> final
    brain_kg_add_edge(kg, root, left, BRAIN_KG_EDGE_DEPENDS_ON, "dep", 1.0f);
    brain_kg_add_edge(kg, root, right, BRAIN_KG_EDGE_DEPENDS_ON, "dep", 1.0f);
    brain_kg_add_edge(kg, left, merge, BRAIN_KG_EDGE_DEPENDS_ON, "dep", 1.0f);
    brain_kg_add_edge(kg, right, merge, BRAIN_KG_EDGE_DEPENDS_ON, "dep", 1.0f);
    brain_kg_add_edge(kg, merge, final_module, BRAIN_KG_EDGE_DEPENDS_ON, "dep", 1.0f);

    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Verify topological sort handles diamond
    brain_kg_node_id_t order[10];
    uint32_t sorted_count = 0;

    int result = kg_hierarchy_topological_sort(hier, order, 10, &sorted_count);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(sorted_count, 5u);

    // Find positions
    int root_pos = -1, left_pos = -1, right_pos = -1, merge_pos = -1, final_pos = -1;
    for (uint32_t i = 0; i < sorted_count; i++) {
        if (order[i] == root) root_pos = (int)i;
        else if (order[i] == left) left_pos = (int)i;
        else if (order[i] == right) right_pos = (int)i;
        else if (order[i] == merge) merge_pos = (int)i;
        else if (order[i] == final_module) final_pos = (int)i;
    }

    // Verify ordering constraints
    EXPECT_LT(root_pos, left_pos);
    EXPECT_LT(root_pos, right_pos);
    EXPECT_LT(left_pos, merge_pos);
    EXPECT_LT(right_pos, merge_pos);
    EXPECT_LT(merge_pos, final_pos);
}

TEST_F(KGHierarchyGraphAlgorithmsIntegration, test_all_algorithms_together) {
    create_brain_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Run all algorithms in sequence to verify they work together

    // 1. Get sorted list
    brain_kg_node_id_t sorted[20];
    uint32_t sorted_count = kg_hierarchy_get_sorted_module_ids(hier, sorted, 20);
    EXPECT_GT(sorted_count, 0u);

    // 2. Topological sort
    brain_kg_node_id_t topo_order[20];
    uint32_t topo_count = 0;
    EXPECT_EQ(kg_hierarchy_topological_sort(hier, topo_order, 20, &topo_count), 0);

    // 3. Find components
    uint32_t comp_ids[20];
    uint32_t num_comp = 0;
    EXPECT_EQ(kg_hierarchy_find_components(hier, comp_ids, &num_comp), 0);
    EXPECT_EQ(num_comp, 1u);

    // 4. BFS traversal
    g_traversal_order.clear();
    brain_kg_node_id_t start = kg_hierarchy_find_module_by_name(hier, "prefrontal");
    EXPECT_EQ(kg_hierarchy_bfs(hier, start, integration_visitor, nullptr), 0);

    // 5. DFS traversal
    g_traversal_order.clear();
    EXPECT_EQ(kg_hierarchy_dfs(hier, start, integration_visitor, nullptr), 0);

    // 6. Shortest path
    brain_kg_node_id_t path[20];
    uint32_t path_len = 0;
    brain_kg_node_id_t dest = kg_hierarchy_find_module_by_name(hier, "hippocampus");
    EXPECT_EQ(kg_hierarchy_shortest_path(hier, start, dest, path, 20, &path_len), 0);

    // 7. Binary search
    uint32_t idx = kg_hierarchy_binary_search_module(hier, sorted, sorted_count, start);
    EXPECT_NE(idx, UINT32_MAX);

    // 8. Distance
    uint32_t dist = kg_hierarchy_get_distance(hier, start, dest);
    EXPECT_LT(dist, UINT32_MAX);

    // 9. Reachable
    brain_kg_node_id_t reachable[20];
    uint32_t reach_count = kg_hierarchy_get_reachable(hier, start, reachable, 20);
    EXPECT_GT(reach_count, 0u);

    // 10. Are connected
    EXPECT_TRUE(kg_hierarchy_are_connected(hier, start, dest));
}

// ============================================================================
// Thread Safety Integration Tests
// ============================================================================

TEST_F(KGHierarchyGraphAlgorithmsIntegration, test_concurrent_graph_algorithm_queries) {
    create_brain_topology();
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    brain_kg_node_id_t prefrontal = kg_hierarchy_find_module_by_name(hier, "prefrontal");
    brain_kg_node_id_t hippocampus = kg_hierarchy_find_module_by_name(hier, "hippocampus");

    // Launch multiple threads doing various queries
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 10; i++) {
                switch (t % 4) {
                    case 0: {
                        // Topological sort
                        brain_kg_node_id_t order[20];
                        uint32_t count = 0;
                        if (kg_hierarchy_topological_sort(hier, order, 20, &count) == 0) {
                            success_count++;
                        }
                        break;
                    }
                    case 1: {
                        // Shortest path
                        brain_kg_node_id_t path[20];
                        uint32_t len = 0;
                        if (kg_hierarchy_shortest_path(hier, prefrontal, hippocampus, path, 20, &len) == 0) {
                            success_count++;
                        }
                        break;
                    }
                    case 2: {
                        // Connected components
                        uint32_t comp_ids[20];
                        uint32_t num = 0;
                        if (kg_hierarchy_find_components(hier, comp_ids, &num) == 0) {
                            success_count++;
                        }
                        break;
                    }
                    case 3: {
                        // Binary search
                        brain_kg_node_id_t sorted[20];
                        uint32_t count = kg_hierarchy_get_sorted_module_ids(hier, sorted, 20);
                        if (count > 0) {
                            kg_hierarchy_binary_search_module(hier, sorted, count, prefrontal);
                            success_count++;
                        }
                        break;
                    }
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), 40) << "All 40 concurrent operations should succeed";
}
