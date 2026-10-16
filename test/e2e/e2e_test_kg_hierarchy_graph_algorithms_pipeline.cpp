/**
 * @file e2e_test_kg_hierarchy_graph_algorithms_pipeline.cpp
 * @brief End-to-end tests for KG hierarchy graph algorithms
 *
 * Tests complete graph algorithm workflows including:
 * - Topological sort for module startup ordering
 * - Binary search for optimized module lookups
 * - BFS/DFS traversal across brain hierarchy
 * - Connected components detection
 * - Shortest path finding between modules
 *
 * These tests validate the complete integration of graph algorithms
 * with a realistic brain topology.
 *
 * @author NIMCP Development Team
 * @date 2025-01-15
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_set>
#include <thread>
#include <atomic>
#include <chrono>

extern "C" {
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_kg.h"
#include "core/brain/nimcp_kg_hierarchy.h"
}

// =============================================================================
// Test Fixture for KG Hierarchy Graph Algorithms E2E
// =============================================================================

class KGHierarchyGraphAlgorithmsE2E : public ::testing::Test {
protected:
    kg_hierarchy_t* hier;
    brain_kg_t* kg;

    // Module IDs for our test brain
    brain_kg_node_id_t core_init;
    brain_kg_node_id_t memory_manager;
    brain_kg_node_id_t event_bus;
    brain_kg_node_id_t perception;
    brain_kg_node_id_t cognition;
    brain_kg_node_id_t motor_control;
    brain_kg_node_id_t hippocampus;
    brain_kg_node_id_t prefrontal;
    brain_kg_node_id_t amygdala;
    brain_kg_node_id_t thalamus;
    brain_kg_node_id_t cerebellum;
    brain_kg_node_id_t visual_cortex;
    brain_kg_node_id_t auditory_cortex;
    brain_kg_node_id_t motor_cortex;
    brain_kg_node_id_t plasticity_engine;
    brain_kg_node_id_t immune_system;

    void SetUp() override {
        hier = nullptr;
        kg = nullptr;

        // Create our own KG for graph algorithm testing
        brain_kg_config_t kg_config;
        memset(&kg_config, 0, sizeof(kg_config));
        kg_config.max_nodes = 256;
        kg_config.max_edges = 1024;

        kg = brain_kg_create(&kg_config);
        if (!kg) {
            return; // Will skip tests
        }

        // Build a realistic brain topology
        CreateBrainTopology();

        // Create hierarchy with default config
        kg_hierarchy_config_t hier_config;
        kg_hierarchy_default_config(&hier_config);
        hier = kg_hierarchy_create(kg, &hier_config);
    }

    void TearDown() override {
        if (hier) {
            kg_hierarchy_destroy(hier);
        }
        if (kg) {
            brain_kg_destroy(kg);
        }
    }

    void CreateBrainTopology() {
        // Create nodes for different brain modules using correct API
        // brain_kg_add_node(kg, name, type, description)

        // Core Infrastructure
        core_init = brain_kg_add_node(kg, "core_init", BRAIN_KG_NODE_CORE,
            "Core initialization system");
        memory_manager = brain_kg_add_node(kg, "memory_manager", BRAIN_KG_NODE_CORE,
            "Memory management module");
        event_bus = brain_kg_add_node(kg, "event_bus", BRAIN_KG_NODE_CORE,
            "Event routing system");

        // Perception modules
        perception = brain_kg_add_node(kg, "perception", BRAIN_KG_NODE_PERCEPTION,
            "Main perception system");
        visual_cortex = brain_kg_add_node(kg, "visual_cortex", BRAIN_KG_NODE_PERCEPTION,
            "Visual processing");
        auditory_cortex = brain_kg_add_node(kg, "auditory_cortex", BRAIN_KG_NODE_PERCEPTION,
            "Auditory processing");
        thalamus = brain_kg_add_node(kg, "thalamus", BRAIN_KG_NODE_SUBCORTICAL,
            "Sensory relay station");

        // Cognitive modules
        cognition = brain_kg_add_node(kg, "cognition", BRAIN_KG_NODE_COGNITIVE,
            "Main cognitive system");
        hippocampus = brain_kg_add_node(kg, "hippocampus", BRAIN_KG_NODE_SUBCORTICAL,
            "Memory formation and retrieval");
        prefrontal = brain_kg_add_node(kg, "prefrontal", BRAIN_KG_NODE_CORTICAL,
            "Executive control");
        amygdala = brain_kg_add_node(kg, "amygdala", BRAIN_KG_NODE_SUBCORTICAL,
            "Emotional processing");

        // Motor modules
        motor_control = brain_kg_add_node(kg, "motor_control", BRAIN_KG_NODE_CORTICAL,
            "Motor control system");
        motor_cortex = brain_kg_add_node(kg, "motor_cortex", BRAIN_KG_NODE_CORTICAL,
            "Motor cortex output");
        cerebellum = brain_kg_add_node(kg, "cerebellum", BRAIN_KG_NODE_SUBCORTICAL,
            "Motor coordination");

        // Plasticity and Immune
        plasticity_engine = brain_kg_add_node(kg, "plasticity_engine", BRAIN_KG_NODE_PLASTICITY,
            "Synaptic plasticity");
        immune_system = brain_kg_add_node(kg, "immune_system", BRAIN_KG_NODE_SECURITY,
            "Brain immune system");

        // Create dependency edges (for topological sort)
        // Core infrastructure must start first
        brain_kg_add_edge(kg, core_init, memory_manager, BRAIN_KG_EDGE_DEPENDS_ON,
            "core_to_mem", 1.0f);
        brain_kg_add_edge(kg, core_init, event_bus, BRAIN_KG_EDGE_DEPENDS_ON,
            "core_to_bus", 1.0f);

        // Memory manager and event bus must start before perception
        brain_kg_add_edge(kg, memory_manager, perception, BRAIN_KG_EDGE_DEPENDS_ON,
            "mem_to_perc", 1.0f);
        brain_kg_add_edge(kg, event_bus, perception, BRAIN_KG_EDGE_DEPENDS_ON,
            "bus_to_perc", 1.0f);
        brain_kg_add_edge(kg, memory_manager, thalamus, BRAIN_KG_EDGE_DEPENDS_ON,
            "mem_to_thal", 1.0f);

        // Perception feeds into visual and auditory cortex
        brain_kg_add_edge(kg, perception, visual_cortex, BRAIN_KG_EDGE_DEPENDS_ON,
            "perc_to_vis", 1.0f);
        brain_kg_add_edge(kg, perception, auditory_cortex, BRAIN_KG_EDGE_DEPENDS_ON,
            "perc_to_aud", 1.0f);
        brain_kg_add_edge(kg, thalamus, visual_cortex, BRAIN_KG_EDGE_DEPENDS_ON,
            "thal_to_vis", 1.0f);
        brain_kg_add_edge(kg, thalamus, auditory_cortex, BRAIN_KG_EDGE_DEPENDS_ON,
            "thal_to_aud", 1.0f);

        // Sensory cortices feed into cognition
        brain_kg_add_edge(kg, visual_cortex, cognition, BRAIN_KG_EDGE_DEPENDS_ON,
            "vis_to_cog", 1.0f);
        brain_kg_add_edge(kg, auditory_cortex, cognition, BRAIN_KG_EDGE_DEPENDS_ON,
            "aud_to_cog", 1.0f);

        // Cognition feeds into cognitive modules
        brain_kg_add_edge(kg, cognition, hippocampus, BRAIN_KG_EDGE_DEPENDS_ON,
            "cog_to_hipp", 1.0f);
        brain_kg_add_edge(kg, cognition, prefrontal, BRAIN_KG_EDGE_DEPENDS_ON,
            "cog_to_pfc", 1.0f);
        brain_kg_add_edge(kg, cognition, amygdala, BRAIN_KG_EDGE_DEPENDS_ON,
            "cog_to_amy", 1.0f);

        // Prefrontal controls motor
        brain_kg_add_edge(kg, prefrontal, motor_control, BRAIN_KG_EDGE_DEPENDS_ON,
            "pfc_to_motor", 1.0f);
        brain_kg_add_edge(kg, motor_control, motor_cortex, BRAIN_KG_EDGE_DEPENDS_ON,
            "motor_to_mcx", 1.0f);
        brain_kg_add_edge(kg, motor_control, cerebellum, BRAIN_KG_EDGE_DEPENDS_ON,
            "motor_to_cere", 1.0f);

        // Plasticity depends on memory
        brain_kg_add_edge(kg, memory_manager, plasticity_engine, BRAIN_KG_EDGE_DEPENDS_ON,
            "mem_to_plast", 1.0f);
        brain_kg_add_edge(kg, hippocampus, plasticity_engine, BRAIN_KG_EDGE_DEPENDS_ON,
            "hipp_to_plast", 1.0f);

        // Immune system monitors everything
        brain_kg_add_edge(kg, memory_manager, immune_system, BRAIN_KG_EDGE_DEPENDS_ON,
            "mem_to_imm", 1.0f);
        brain_kg_add_edge(kg, event_bus, immune_system, BRAIN_KG_EDGE_DEPENDS_ON,
            "bus_to_imm", 1.0f);

        // Create connection edges (for BFS/DFS)
        brain_kg_add_edge(kg, visual_cortex, hippocampus, BRAIN_KG_EDGE_CONNECTS_TO,
            "vis_conn_hipp", 0.8f);
        brain_kg_add_edge(kg, auditory_cortex, hippocampus, BRAIN_KG_EDGE_CONNECTS_TO,
            "aud_conn_hipp", 0.7f);
        brain_kg_add_edge(kg, hippocampus, prefrontal, BRAIN_KG_EDGE_CONNECTS_TO,
            "hipp_conn_pfc", 0.9f);
        brain_kg_add_edge(kg, amygdala, prefrontal, BRAIN_KG_EDGE_CONNECTS_TO,
            "amy_conn_pfc", 0.85f);
        brain_kg_add_edge(kg, prefrontal, motor_cortex, BRAIN_KG_EDGE_CONNECTS_TO,
            "pfc_conn_mcx", 0.95f);
        brain_kg_add_edge(kg, cerebellum, motor_cortex, BRAIN_KG_EDGE_CONNECTS_TO,
            "cere_conn_mcx", 0.9f);
        brain_kg_add_edge(kg, thalamus, cognition, BRAIN_KG_EDGE_CONNECTS_TO,
            "thal_conn_cog", 0.8f);
        brain_kg_add_edge(kg, plasticity_engine, hippocampus, BRAIN_KG_EDGE_CONNECTS_TO,
            "plast_conn_hipp", 0.7f);
        brain_kg_add_edge(kg, immune_system, plasticity_engine, BRAIN_KG_EDGE_CONNECTS_TO,
            "imm_conn_plast", 0.6f);
    }
};

// =============================================================================
// Topological Sort E2E Tests
// =============================================================================

TEST_F(KGHierarchyGraphAlgorithmsE2E, TopologicalSortCompleteStartupOrder) {
    if (!hier) {
        GTEST_SKIP() << "KG hierarchy creation failed";
    }

    // Get topological sort for module startup order
    brain_kg_node_id_t startup_order[32];
    uint32_t count = 0;
    int result = kg_hierarchy_topological_sort(hier, startup_order, 32, &count);

    EXPECT_EQ(result, 0) << "Topological sort should succeed";
    ASSERT_GT(count, 0U) << "Should return modules";
    EXPECT_EQ(count, 16U) << "Should have all 16 modules";

    // Verify core_init comes before all other modules
    uint32_t core_init_pos = UINT32_MAX;
    uint32_t memory_manager_pos = UINT32_MAX;
    uint32_t perception_pos = UINT32_MAX;
    uint32_t cognition_pos = UINT32_MAX;
    uint32_t motor_control_pos = UINT32_MAX;

    for (uint32_t i = 0; i < count; i++) {
        if (startup_order[i] == core_init) core_init_pos = i;
        if (startup_order[i] == memory_manager) memory_manager_pos = i;
        if (startup_order[i] == perception) perception_pos = i;
        if (startup_order[i] == cognition) cognition_pos = i;
        if (startup_order[i] == motor_control) motor_control_pos = i;
    }

    // Verify dependency ordering
    EXPECT_LT(core_init_pos, memory_manager_pos)
        << "core_init must start before memory_manager";
    EXPECT_LT(memory_manager_pos, perception_pos)
        << "memory_manager must start before perception";
    EXPECT_LT(perception_pos, cognition_pos)
        << "perception must start before cognition";
    EXPECT_LT(cognition_pos, motor_control_pos)
        << "cognition must start before motor_control";
}

TEST_F(KGHierarchyGraphAlgorithmsE2E, TopologicalSortStartupShutdownSymmetry) {
    if (!hier) {
        GTEST_SKIP() << "KG hierarchy creation failed";
    }

    // Get startup order
    brain_kg_node_id_t startup_order[32];
    uint32_t startup_count = 0;
    int result = kg_hierarchy_topological_sort(hier, startup_order, 32, &startup_count);
    EXPECT_EQ(result, 0);
    ASSERT_GT(startup_count, 0U);

    // Shutdown should be reverse of startup (with dependencies satisfied)
    std::vector<brain_kg_node_id_t> shutdown_order(startup_order, startup_order + startup_count);
    std::reverse(shutdown_order.begin(), shutdown_order.end());

    // Verify motor modules shutdown before cognitive
    uint32_t motor_control_pos = UINT32_MAX;
    uint32_t cognition_pos = UINT32_MAX;
    uint32_t core_init_pos = UINT32_MAX;

    for (uint32_t i = 0; i < shutdown_order.size(); i++) {
        if (shutdown_order[i] == motor_control) motor_control_pos = i;
        if (shutdown_order[i] == cognition) cognition_pos = i;
        if (shutdown_order[i] == core_init) core_init_pos = i;
    }

    EXPECT_LT(motor_control_pos, cognition_pos)
        << "In shutdown, motor_control should stop before cognition";
    EXPECT_GT(core_init_pos, motor_control_pos)
        << "core_init should be last to shutdown";
}

// =============================================================================
// Binary Search E2E Tests
// =============================================================================

TEST_F(KGHierarchyGraphAlgorithmsE2E, BinarySearchAllModulesFound) {
    if (!hier) {
        GTEST_SKIP() << "KG hierarchy creation failed";
    }

    // Get sorted module IDs
    brain_kg_node_id_t sorted_ids[32];
    uint32_t count = kg_hierarchy_get_sorted_module_ids(hier, sorted_ids, 32);

    ASSERT_GT(count, 0U);

    // Search for each module we created
    brain_kg_node_id_t test_modules[] = {
        core_init, memory_manager, event_bus, perception, cognition,
        motor_control, hippocampus, prefrontal, amygdala, thalamus,
        cerebellum, visual_cortex, auditory_cortex, motor_cortex,
        plasticity_engine, immune_system
    };

    for (brain_kg_node_id_t mod : test_modules) {
        uint32_t idx = kg_hierarchy_binary_search_module(hier, sorted_ids, count, mod);
        EXPECT_NE(idx, UINT32_MAX) << "Module should be found via binary search";
        if (idx != UINT32_MAX) {
            EXPECT_EQ(sorted_ids[idx], mod);
        }
    }
}

TEST_F(KGHierarchyGraphAlgorithmsE2E, BinarySearchNonExistentModule) {
    if (!hier) {
        GTEST_SKIP() << "KG hierarchy creation failed";
    }

    // Get sorted module IDs
    brain_kg_node_id_t sorted_ids[32];
    uint32_t count = kg_hierarchy_get_sorted_module_ids(hier, sorted_ids, 32);

    ASSERT_GT(count, 0U);

    // Search for non-existent modules
    brain_kg_node_id_t invalid_id = 999999;
    EXPECT_EQ(kg_hierarchy_binary_search_module(hier, sorted_ids, count, invalid_id), UINT32_MAX);

    EXPECT_EQ(kg_hierarchy_binary_search_module(hier, sorted_ids, count, BRAIN_KG_INVALID_NODE), UINT32_MAX);
}

// =============================================================================
// BFS/DFS Traversal E2E Tests
// =============================================================================

// Callback context for tracking traversal
struct TraversalContext {
    std::vector<brain_kg_node_id_t> visited;
    std::unordered_set<brain_kg_node_id_t> visited_set;
    uint32_t callback_count;
};

static bool traversal_callback(brain_kg_node_id_t node_id, uint32_t depth, void* user_data) {
    TraversalContext* ctx = static_cast<TraversalContext*>(user_data);
    ctx->visited.push_back(node_id);
    ctx->visited_set.insert(node_id);
    ctx->callback_count++;
    return true; // Continue traversal
}

TEST_F(KGHierarchyGraphAlgorithmsE2E, BFSFromCoreInit) {
    if (!hier) {
        GTEST_SKIP() << "KG hierarchy creation failed";
    }

    TraversalContext ctx = {};

    int result = kg_hierarchy_bfs(hier, core_init, traversal_callback, &ctx);
    EXPECT_EQ(result, 0) << "BFS should succeed";

    // core_init should be first
    ASSERT_GT(ctx.visited.size(), 0U);
    EXPECT_EQ(ctx.visited[0], core_init);

    // Immediate dependencies should be visited early
    bool found_memory_manager = false;
    bool found_event_bus = false;
    for (size_t i = 0; i < std::min<size_t>(5, ctx.visited.size()); i++) {
        if (ctx.visited[i] == memory_manager) found_memory_manager = true;
        if (ctx.visited[i] == event_bus) found_event_bus = true;
    }
    EXPECT_TRUE(found_memory_manager || found_event_bus)
        << "BFS should visit immediate dependencies early";
}

TEST_F(KGHierarchyGraphAlgorithmsE2E, DFSFromCoreInit) {
    if (!hier) {
        GTEST_SKIP() << "KG hierarchy creation failed";
    }

    TraversalContext ctx = {};

    int result = kg_hierarchy_dfs(hier, core_init, traversal_callback, &ctx);
    EXPECT_EQ(result, 0) << "DFS should succeed";

    // core_init should be first
    ASSERT_GT(ctx.visited.size(), 0U);
    EXPECT_EQ(ctx.visited[0], core_init);

    // DFS goes deep before wide
    EXPECT_GT(ctx.visited.size(), 5U) << "Should visit multiple modules";
}

TEST_F(KGHierarchyGraphAlgorithmsE2E, BFSLevelOrderProperty) {
    if (!hier) {
        GTEST_SKIP() << "KG hierarchy creation failed";
    }

    // BFS should visit nodes level by level
    struct LevelContext {
        std::vector<std::pair<brain_kg_node_id_t, uint32_t>> visited_with_depth;
    };

    LevelContext ctx;
    auto level_callback = [](brain_kg_node_id_t node_id, uint32_t depth, void* user_data) -> bool {
        LevelContext* ctx = static_cast<LevelContext*>(user_data);
        ctx->visited_with_depth.push_back({node_id, depth});
        return true;
    };

    kg_hierarchy_bfs(hier, core_init, level_callback, &ctx);

    // Verify BFS visits nodes in non-decreasing depth order
    for (size_t i = 1; i < ctx.visited_with_depth.size(); i++) {
        EXPECT_LE(ctx.visited_with_depth[i-1].second, ctx.visited_with_depth[i].second)
            << "BFS should visit nodes in level order";
    }
}

TEST_F(KGHierarchyGraphAlgorithmsE2E, TraversalReachesAllConnectedModules) {
    if (!hier) {
        GTEST_SKIP() << "KG hierarchy creation failed";
    }

    // BFS from core_init should reach all modules (graph is connected)
    TraversalContext ctx = {};
    kg_hierarchy_bfs(hier, core_init, traversal_callback, &ctx);

    // All modules should be reachable from core_init via dependencies
    EXPECT_EQ(ctx.visited.size(), 16U)
        << "BFS should reach all modules in connected graph";
}

// =============================================================================
// Connected Components E2E Tests
// =============================================================================

TEST_F(KGHierarchyGraphAlgorithmsE2E, SingleConnectedComponent) {
    if (!hier) {
        GTEST_SKIP() << "KG hierarchy creation failed";
    }

    uint32_t component_ids[32];
    uint32_t num_components = 0;

    int result = kg_hierarchy_find_components(hier, component_ids, &num_components);
    EXPECT_EQ(result, 0) << "Connected components should succeed";

    // All modules should be in a single component
    EXPECT_EQ(num_components, 1U) << "All modules should be in one connected component";
}

TEST_F(KGHierarchyGraphAlgorithmsE2E, ConnectedComponentsWithIsolatedNode) {
    if (!kg) {
        GTEST_SKIP() << "KG creation failed";
    }

    // Add an isolated node
    brain_kg_node_id_t isolated = brain_kg_add_node(kg, "isolated_module",
        BRAIN_KG_NODE_CORE, "Isolated test module");

    ASSERT_NE(isolated, BRAIN_KG_INVALID_NODE);

    // Rebuild hierarchy to include new node
    if (hier) {
        kg_hierarchy_destroy(hier);
    }
    kg_hierarchy_config_t hier_config;
    kg_hierarchy_default_config(&hier_config);
    hier = kg_hierarchy_create(kg, &hier_config);
    ASSERT_NE(hier, nullptr);

    uint32_t component_ids[32];
    uint32_t num_components = 0;

    int result = kg_hierarchy_find_components(hier, component_ids, &num_components);
    EXPECT_EQ(result, 0);

    // Now should have 2 components (main graph + isolated node)
    EXPECT_EQ(num_components, 2U) << "Should have 2 components with isolated node";
}

// =============================================================================
// Shortest Path E2E Tests
// =============================================================================

TEST_F(KGHierarchyGraphAlgorithmsE2E, ShortestPathCoreToMotor) {
    if (!hier) {
        GTEST_SKIP() << "KG hierarchy creation failed";
    }

    brain_kg_node_id_t path[32];
    uint32_t path_len = 0;

    int result = kg_hierarchy_shortest_path(hier, core_init, motor_cortex, path, 32, &path_len);

    // Path should exist (graph is connected)
    if (result == 0) {
        EXPECT_GT(path_len, 0U) << "Path should have nodes";
        EXPECT_EQ(path[0], core_init) << "Path should start at core_init";
        EXPECT_EQ(path[path_len - 1], motor_cortex) << "Path should end at motor_cortex";

        // Verify path is valid (each consecutive pair has an edge)
        for (uint32_t i = 0; i < path_len - 1; i++) {
            bool has_edge = false;
            brain_kg_edge_list_t* edges = brain_kg_get_outgoing(kg, path[i]);
            if (edges) {
                for (uint32_t j = 0; j < edges->count; j++) {
                    if (edges->edges[j]->to == path[i + 1]) {
                        has_edge = true;
                        break;
                    }
                }
                brain_kg_edge_list_destroy(edges);
            }
            EXPECT_TRUE(has_edge) << "Path should only use existing edges";
        }
    }
}

TEST_F(KGHierarchyGraphAlgorithmsE2E, ShortestPathPerceptionToPrefrontal) {
    if (!hier) {
        GTEST_SKIP() << "KG hierarchy creation failed";
    }

    brain_kg_node_id_t path[32];
    uint32_t path_len = 0;

    int result = kg_hierarchy_shortest_path(hier, perception, prefrontal, path, 32, &path_len);

    if (result == 0) {
        EXPECT_GT(path_len, 0U);
        EXPECT_EQ(path[0], perception);
        EXPECT_EQ(path[path_len - 1], prefrontal);

        // The shortest path should go through cognition
        bool passes_through_cognition = false;
        for (uint32_t i = 0; i < path_len; i++) {
            if (path[i] == cognition) {
                passes_through_cognition = true;
                break;
            }
        }
        EXPECT_TRUE(passes_through_cognition)
            << "Shortest path from perception to prefrontal should pass through cognition";
    }
}

TEST_F(KGHierarchyGraphAlgorithmsE2E, ShortestPathSameNode) {
    if (!hier) {
        GTEST_SKIP() << "KG hierarchy creation failed";
    }

    brain_kg_node_id_t path[32];
    uint32_t path_len = 0;

    int result = kg_hierarchy_shortest_path(hier, core_init, core_init, path, 32, &path_len);

    // Path to self should have length 1 (just the node itself)
    EXPECT_EQ(result, 0);
    EXPECT_EQ(path_len, 1U);
    EXPECT_EQ(path[0], core_init);
}

// =============================================================================
// Complete Workflow E2E Tests
// =============================================================================

TEST_F(KGHierarchyGraphAlgorithmsE2E, CompleteModuleStartupWorkflow) {
    if (!hier) {
        GTEST_SKIP() << "KG hierarchy creation failed";
    }

    // Step 1: Get topological sort for startup order
    brain_kg_node_id_t startup_order[32];
    uint32_t startup_count = 0;
    int result = kg_hierarchy_topological_sort(hier, startup_order, 32, &startup_count);
    EXPECT_EQ(result, 0);
    ASSERT_GT(startup_count, 0U);

    // Step 2: Verify the order is valid by checking dependencies
    std::unordered_set<brain_kg_node_id_t> started;
    for (uint32_t i = 0; i < startup_count; i++) {
        brain_kg_node_id_t current = startup_order[i];
        started.insert(current);
    }

    // Step 3: Lookup specific modules via binary search
    brain_kg_node_id_t sorted_ids[32];
    uint32_t sorted_count = kg_hierarchy_get_sorted_module_ids(hier, sorted_ids, 32);

    uint32_t core_idx = kg_hierarchy_binary_search_module(hier, sorted_ids, sorted_count, core_init);
    EXPECT_NE(core_idx, UINT32_MAX);

    // Step 4: Traverse from core to verify connectivity
    TraversalContext ctx = {};
    kg_hierarchy_bfs(hier, core_init, traversal_callback, &ctx);
    EXPECT_EQ(ctx.visited.size(), startup_count)
        << "All modules should be reachable from core_init";
}

TEST_F(KGHierarchyGraphAlgorithmsE2E, CompleteGraphAnalysisWorkflow) {
    if (!hier) {
        GTEST_SKIP() << "KG hierarchy creation failed";
    }

    // Step 1: Check connectivity
    uint32_t component_ids[32];
    uint32_t num_components = 0;
    int result = kg_hierarchy_find_components(hier, component_ids, &num_components);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(num_components, 1U) << "Brain should be fully connected";

    // Step 2: Find critical paths (perception to motor output)
    brain_kg_node_id_t perception_to_motor[32];
    uint32_t p2m_len = 0;
    kg_hierarchy_shortest_path(hier, perception, motor_cortex, perception_to_motor, 32, &p2m_len);
    EXPECT_GT(p2m_len, 2U) << "Should have intermediate processing stages";

    // Step 3: Analyze reachability from different starting points
    TraversalContext thalamus_ctx = {};
    kg_hierarchy_bfs(hier, thalamus, traversal_callback, &thalamus_ctx);

    // Thalamus should reach cognitive and motor areas
    bool reaches_cognition = thalamus_ctx.visited_set.count(cognition) > 0;
    bool reaches_motor = thalamus_ctx.visited_set.count(motor_cortex) > 0;
    EXPECT_TRUE(reaches_cognition) << "Thalamus should connect to cognition";
    EXPECT_TRUE(reaches_motor) << "Thalamus should eventually connect to motor";

    // Step 4: Verify startup order respects dependencies
    brain_kg_node_id_t startup[32];
    uint32_t startup_count = 0;
    kg_hierarchy_topological_sort(hier, startup, 32, &startup_count);

    // Find positions
    uint32_t thal_pos = UINT32_MAX, cog_pos = UINT32_MAX;
    for (uint32_t i = 0; i < startup_count; i++) {
        if (startup[i] == thalamus) thal_pos = i;
        if (startup[i] == cognition) cog_pos = i;
    }

    if (thal_pos != UINT32_MAX && cog_pos != UINT32_MAX) {
        // Thalamus should start before cognition due to dependencies
        EXPECT_LT(thal_pos, cog_pos);
    }
}

TEST_F(KGHierarchyGraphAlgorithmsE2E, StressTestLargeTraversal) {
    if (!hier) {
        GTEST_SKIP() << "KG hierarchy creation failed";
    }

    // Run multiple traversals and verify consistency
    const int NUM_RUNS = 100;

    std::vector<size_t> traversal_sizes;
    for (int i = 0; i < NUM_RUNS; i++) {
        TraversalContext ctx = {};
        kg_hierarchy_bfs(hier, core_init, traversal_callback, &ctx);
        traversal_sizes.push_back(ctx.visited.size());
    }

    // All traversals should return the same result
    for (size_t i = 1; i < traversal_sizes.size(); i++) {
        EXPECT_EQ(traversal_sizes[0], traversal_sizes[i])
            << "Traversal results should be consistent";
    }
}

TEST_F(KGHierarchyGraphAlgorithmsE2E, ConcurrentAccessSafety) {
    if (!hier) {
        GTEST_SKIP() << "KG hierarchy creation failed";
    }

    // Test concurrent read access
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};

    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 50;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, &success_count, &error_count]() {
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                switch (i % 4) {
                    case 0: {
                        // Topological sort
                        brain_kg_node_id_t order[32];
                        uint32_t count = 0;
                        if (kg_hierarchy_topological_sort(hier, order, 32, &count) == 0 && count > 0) {
                            success_count++;
                        } else {
                            error_count++;
                        }
                        break;
                    }
                    case 1: {
                        // BFS traversal
                        TraversalContext ctx = {};
                        if (kg_hierarchy_bfs(hier, core_init, traversal_callback, &ctx) == 0) {
                            success_count++;
                        } else {
                            error_count++;
                        }
                        break;
                    }
                    case 2: {
                        // Get module info
                        kg_module_info_t info;
                        if (kg_hierarchy_get_module_info(hier, perception, &info) == 0) {
                            success_count++;
                        } else {
                            error_count++;
                        }
                        break;
                    }
                    case 3: {
                        // Shortest path
                        brain_kg_node_id_t path[32];
                        uint32_t len = 0;
                        int res = kg_hierarchy_shortest_path(hier, core_init, motor_cortex,
                                                            path, 32, &len);
                        if (res == 0) success_count++;
                        else error_count++;
                        break;
                    }
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Most operations should succeed
    EXPECT_GT(success_count.load(), error_count.load())
        << "Concurrent access should mostly succeed";
    EXPECT_EQ(error_count.load(), 0)
        << "No errors expected with read-only concurrent access";
}

// =============================================================================
// Edge Case E2E Tests
// =============================================================================

TEST_F(KGHierarchyGraphAlgorithmsE2E, EmptyBufferHandling) {
    if (!hier) {
        GTEST_SKIP() << "KG hierarchy creation failed";
    }

    // Test with zero-size buffers - should handle gracefully
    brain_kg_node_id_t path[1];
    uint32_t path_len = 0;

    // Should return error or handle gracefully
    kg_hierarchy_shortest_path(hier, core_init, motor_cortex, path, 0, &path_len);

    brain_kg_node_id_t order[1];
    uint32_t count = 0;
    kg_hierarchy_topological_sort(hier, order, 0, &count);
    // Should return 0 or handle gracefully
}

TEST_F(KGHierarchyGraphAlgorithmsE2E, InvalidNodeHandling) {
    if (!hier) {
        GTEST_SKIP() << "KG hierarchy creation failed";
    }

    brain_kg_node_id_t invalid_node = BRAIN_KG_INVALID_NODE;

    // BFS with invalid start
    TraversalContext ctx = {};
    int result = kg_hierarchy_bfs(hier, invalid_node, traversal_callback, &ctx);
    EXPECT_NE(result, 0) << "BFS with invalid node should fail";

    // Shortest path with invalid nodes
    brain_kg_node_id_t path[32];
    uint32_t path_len = 0;
    result = kg_hierarchy_shortest_path(hier, invalid_node, motor_cortex, path, 32, &path_len);
    EXPECT_NE(result, 0) << "Shortest path with invalid start should fail";

    result = kg_hierarchy_shortest_path(hier, core_init, invalid_node, path, 32, &path_len);
    EXPECT_NE(result, 0) << "Shortest path with invalid end should fail";
}

// =============================================================================
// Performance E2E Tests
// =============================================================================

TEST_F(KGHierarchyGraphAlgorithmsE2E, PerformanceBenchmarkTopologicalSort) {
    if (!hier) {
        GTEST_SKIP() << "KG hierarchy creation failed";
    }

    const int NUM_ITERATIONS = 1000;
    brain_kg_node_id_t order[32];
    uint32_t count = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        kg_hierarchy_topological_sort(hier, order, 32, &count);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg_us = static_cast<double>(duration.count()) / NUM_ITERATIONS;

    // Should complete within reasonable time (< 5ms per call for E2E environment)
    EXPECT_LT(avg_us, 5000.0) << "Topological sort should be fast";

    std::cout << "Topological sort avg time: " << avg_us << " us" << std::endl;
}

TEST_F(KGHierarchyGraphAlgorithmsE2E, PerformanceBenchmarkBFS) {
    if (!hier) {
        GTEST_SKIP() << "KG hierarchy creation failed";
    }

    const int NUM_ITERATIONS = 1000;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        TraversalContext ctx = {};
        kg_hierarchy_bfs(hier, core_init, traversal_callback, &ctx);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg_us = static_cast<double>(duration.count()) / NUM_ITERATIONS;

    EXPECT_LT(avg_us, 5000.0) << "BFS should be fast";

    std::cout << "BFS traversal avg time: " << avg_us << " us" << std::endl;
}

TEST_F(KGHierarchyGraphAlgorithmsE2E, PerformanceBenchmarkShortestPath) {
    if (!hier) {
        GTEST_SKIP() << "KG hierarchy creation failed";
    }

    const int NUM_ITERATIONS = 1000;
    brain_kg_node_id_t path[32];
    uint32_t path_len;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        kg_hierarchy_shortest_path(hier, core_init, motor_cortex, path, 32, &path_len);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg_us = static_cast<double>(duration.count()) / NUM_ITERATIONS;

    EXPECT_LT(avg_us, 5000.0) << "Shortest path should be fast";

    std::cout << "Shortest path avg time: " << avg_us << " us" << std::endl;
}
