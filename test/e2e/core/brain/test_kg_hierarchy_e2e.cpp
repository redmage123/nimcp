/**
 * @file test_kg_hierarchy_e2e.cpp
 * @brief End-to-end tests for KG Hierarchy edge metadata pipeline
 *
 * Full pipeline tests:
 * 1. Create KG -> add nodes -> add edges -> create hierarchy -> add metadata -> verify
 * 2. Multi-hemisphere graph with cross-hemisphere metadata
 * 3. Full lifecycle: init -> build graph -> metadata -> state changes -> query -> shutdown
 * 4. Stress: many edges with metadata in a realistic brain topology
 * 5. Metadata integrity after sequence of hierarchy operations
 *
 * @date 2026-02-12
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <set>
#include <chrono>

extern "C" {
#include "core/brain/nimcp_kg_hierarchy.h"
#include "core/brain/nimcp_brain_kg.h"
#include "nimcp.h"
}

// ============================================================================
// Test Fixture
// ============================================================================

class KGHierarchyEdgeMetadataE2E : public ::testing::Test {
protected:
    brain_kg_t* kg;
    kg_hierarchy_t* hier;

    void SetUp() override {
        // Initialize NIMCP library for full E2E environment
        nimcp_init();

        brain_kg_config_t kg_config;
        brain_kg_default_config(&kg_config);
        kg_config.enable_security = false;
        kg_config.enable_access_control = false;
        kg_config.enable_integrity_checks = false;
        kg = brain_kg_create(&kg_config);
        ASSERT_NE(kg, nullptr);

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
        nimcp_shutdown();
    }

    brain_kg_node_id_t add_module(const char* name, brain_kg_node_type_t type) {
        return brain_kg_add_node(kg, name, type, "E2E test module");
    }
};

// ============================================================================
// E2E Pipeline: Complete KG -> Hierarchy -> Metadata Workflow
// ============================================================================

TEST_F(KGHierarchyEdgeMetadataE2E, CompletePipeline_CreateGraphAddMetadataVerify) {
    // Stage 1: Build a brain-like knowledge graph
    brain_kg_node_id_t core = add_module("core_subsystem", BRAIN_KG_NODE_CORE);
    brain_kg_node_id_t mem  = add_module("memory_system", BRAIN_KG_NODE_CORE);
    brain_kg_node_id_t vis  = add_module("visual_cortex", BRAIN_KG_NODE_PERCEPTION);
    brain_kg_node_id_t aud  = add_module("auditory_cortex", BRAIN_KG_NODE_PERCEPTION);
    brain_kg_node_id_t pfc  = add_module("prefrontal_cortex", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t hip  = add_module("hippocampus", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t gw   = add_module("global_workspace", BRAIN_KG_NODE_INTEGRATION);
    brain_kg_node_id_t coord = add_module("coordinator", BRAIN_KG_NODE_COORDINATOR);
    brain_kg_node_id_t plast = add_module("plasticity_engine", BRAIN_KG_NODE_PLASTICITY);
    brain_kg_node_id_t immune = add_module("immune_system", BRAIN_KG_NODE_SECURITY);

    ASSERT_NE(core, BRAIN_KG_INVALID_NODE);
    ASSERT_NE(immune, BRAIN_KG_INVALID_NODE);

    // Stage 2: Add edges (connections between modules)
    brain_kg_add_edge(kg, core, mem, BRAIN_KG_EDGE_DEPENDS_ON, "init", 1.0f);
    brain_kg_add_edge(kg, core, vis, BRAIN_KG_EDGE_DEPENDS_ON, "init", 1.0f);
    brain_kg_add_edge(kg, core, aud, BRAIN_KG_EDGE_DEPENDS_ON, "init", 1.0f);
    brain_kg_add_edge(kg, mem, hip, BRAIN_KG_EDGE_PROVIDES_TO, "memory", 0.9f);
    brain_kg_add_edge(kg, vis, pfc, BRAIN_KG_EDGE_SENDS_TO, "visual", 0.8f);
    brain_kg_add_edge(kg, aud, pfc, BRAIN_KG_EDGE_SENDS_TO, "audio", 0.8f);
    brain_kg_add_edge(kg, pfc, gw, BRAIN_KG_EDGE_INTEGRATES_WITH, "cognition", 0.7f);
    brain_kg_add_edge(kg, gw, coord, BRAIN_KG_EDGE_COORDINATES_WITH, "sync", 0.9f);
    brain_kg_add_edge(kg, plast, hip, BRAIN_KG_EDGE_MODULATES, "plasticity", 0.6f);
    brain_kg_add_edge(kg, immune, core, BRAIN_KG_EDGE_MODULATES, "immune_response", 0.5f);

    // Stage 3: Create hierarchy from KG
    kg_hierarchy_config_t config;
    kg_hierarchy_default_config(&config);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Stage 4: Verify hierarchy has correct structure
    kg_brain_stats_t stats;
    EXPECT_EQ(kg_hierarchy_get_brain_stats(hier, &stats), 0);
    EXPECT_EQ(stats.total_modules, 10u);

    // Stage 5: Add metadata to every edge
    struct EdgeAnnotation {
        brain_kg_node_id_t from;
        brain_kg_node_id_t to;
        const char* key;
        int32_t value;
    };

    EdgeAnnotation annotations[] = {
        {core, mem,   "startup_order", 0},
        {core, vis,   "startup_order", 1},
        {core, aud,   "startup_order", 2},
        {mem,  hip,   "startup_order", 3},
        {vis,  pfc,   "startup_order", 4},
        {aud,  pfc,   "startup_order", 5},
        {pfc,  gw,    "startup_order", 6},
        {gw,   coord, "startup_order", 7},
        {plast, hip,  "modulation_strength", 60},
        {immune, core, "modulation_strength", 50},
    };

    for (const auto& ann : annotations) {
        EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
            hier, ann.from, ann.to, ann.key, ann.value), 0)
            << "Failed to set metadata for edge " << ann.from << " -> " << ann.to;
    }

    // Stage 6: Verify all metadata
    for (const auto& ann : annotations) {
        int32_t value = -1;
        EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
            hier, ann.from, ann.to, ann.key, &value), 0)
            << "Failed to get metadata for edge " << ann.from << " -> " << ann.to;
        EXPECT_EQ(value, ann.value)
            << "Value mismatch for edge " << ann.from << " -> " << ann.to;
    }
}

TEST_F(KGHierarchyEdgeMetadataE2E, MultihemisphereMetadataPipeline) {
    // Build a graph spanning both hemispheres, annotate cross-hemisphere edges

    // Left hemisphere modules (language, logic)
    brain_kg_node_id_t broca     = add_module("broca_area", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t wernicke  = add_module("wernicke_area", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t math_eng  = add_module("math_engine", BRAIN_KG_NODE_COGNITIVE);

    // Right hemisphere modules (spatial, creative)
    brain_kg_node_id_t spatial   = add_module("spatial_processor", BRAIN_KG_NODE_PERCEPTION);
    brain_kg_node_id_t pattern   = add_module("pattern_recognizer", BRAIN_KG_NODE_PERCEPTION);
    brain_kg_node_id_t creative  = add_module("creative_module", BRAIN_KG_NODE_COGNITIVE);

    // Bilateral modules (coordination)
    brain_kg_node_id_t coord     = add_module("coordinator", BRAIN_KG_NODE_COORDINATOR);
    brain_kg_node_id_t gw       = add_module("global_workspace", BRAIN_KG_NODE_INTEGRATION);

    // Intra-hemisphere edges
    brain_kg_add_edge(kg, broca, wernicke, BRAIN_KG_EDGE_CONNECTS_TO, "language", 0.9f);
    brain_kg_add_edge(kg, spatial, pattern, BRAIN_KG_EDGE_CONNECTS_TO, "visual", 0.8f);

    // Cross-hemisphere edges (via bilateral)
    brain_kg_add_edge(kg, broca, gw, BRAIN_KG_EDGE_INTEGRATES_WITH, "lang_to_gw", 0.7f);
    brain_kg_add_edge(kg, spatial, gw, BRAIN_KG_EDGE_INTEGRATES_WITH, "spatial_to_gw", 0.7f);
    brain_kg_add_edge(kg, gw, coord, BRAIN_KG_EDGE_COORDINATES_WITH, "sync", 0.9f);
    brain_kg_add_edge(kg, math_eng, creative, BRAIN_KG_EDGE_CONNECTS_TO, "cross_hemi", 0.5f);

    // Create hierarchy
    kg_hierarchy_config_t config;
    kg_hierarchy_default_config(&config);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Annotate all edges with latency metadata
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, broca, wernicke, "latency_us", 50), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, spatial, pattern, "latency_us", 60), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, broca, gw, "latency_us", 200), 0);   // cross-hemisphere: higher latency
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, spatial, gw, "latency_us", 210), 0);  // cross-hemisphere
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, gw, coord, "latency_us", 30), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, math_eng, creative, "latency_us", 250), 0); // cross-hemisphere: highest

    // Verify intra-hemisphere edges have lower latency than cross-hemisphere
    int32_t intra_lat = 0, cross_lat = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, broca, wernicke, "latency_us", &intra_lat), 0);
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, math_eng, creative, "latency_us", &cross_lat), 0);
    EXPECT_LT(intra_lat, cross_lat);  // Intra-hemisphere should be faster
}

TEST_F(KGHierarchyEdgeMetadataE2E, FullLifecycleWithMetadata) {
    // Full lifecycle: create graph, add metadata, simulate runtime, verify integrity

    // Step 1: Build minimal brain
    brain_kg_node_id_t input   = add_module("input_layer", BRAIN_KG_NODE_PERCEPTION);
    brain_kg_node_id_t hidden  = add_module("hidden_layer", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t output  = add_module("output_layer", BRAIN_KG_NODE_CORE);

    brain_kg_add_edge(kg, input, hidden, BRAIN_KG_EDGE_SENDS_TO, "forward", 0.8f);
    brain_kg_add_edge(kg, hidden, output, BRAIN_KG_EDGE_SENDS_TO, "forward", 0.8f);
    brain_kg_add_edge(kg, output, hidden, BRAIN_KG_EDGE_SENDS_TO, "feedback", 0.3f);

    // Step 2: Create hierarchy
    kg_hierarchy_config_t config;
    kg_hierarchy_default_config(&config);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Step 3: Set initial metadata (connection weights in metadata)
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, input, hidden, "msg_count", 0), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, hidden, output, "msg_count", 0), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, output, hidden, "msg_count", 0), 0);

    // Step 4: Simulate runtime - update modules and metadata
    kg_hierarchy_report_state_change(hier, input,
        KG_MODULE_STATE_RUNNING, "started");
    kg_hierarchy_report_state_change(hier, hidden,
        KG_MODULE_STATE_RUNNING, "started");
    kg_hierarchy_report_state_change(hier, output,
        KG_MODULE_STATE_RUNNING, "started");

    // Simulate message passing - update metadata counters
    for (int cycle = 1; cycle <= 10; cycle++) {
        EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
            hier, input, hidden, "msg_count", cycle * 100), 0);
        EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
            hier, hidden, output, "msg_count", cycle * 80), 0);
        EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
            hier, output, hidden, "msg_count", cycle * 20), 0);

        // Report message stats
        kg_hierarchy_report_message_stats(hier, input,
            (uint64_t)(cycle * 100), (uint64_t)(cycle * 50));
        kg_hierarchy_report_message_stats(hier, hidden,
            (uint64_t)(cycle * 80), (uint64_t)(cycle * 120));
    }

    // Step 5: Verify final metadata state
    int32_t input_hidden = 0, hidden_output = 0, output_hidden = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, input, hidden, "msg_count", &input_hidden), 0);
    EXPECT_EQ(input_hidden, 1000);  // 10 * 100

    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, hidden, output, "msg_count", &hidden_output), 0);
    EXPECT_EQ(hidden_output, 800);  // 10 * 80

    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, output, hidden, "msg_count", &output_hidden), 0);
    EXPECT_EQ(output_hidden, 200);  // 10 * 20

    // Step 6: Verify hierarchy health is still queryable
    bio_module_health_t health = kg_hierarchy_get_brain_health(hier);
    (void)health;  // Just verify it doesn't crash

    kg_brain_stats_t stats;
    EXPECT_EQ(kg_hierarchy_get_brain_stats(hier, &stats), 0);
    EXPECT_EQ(stats.total_modules, 3u);
}

TEST_F(KGHierarchyEdgeMetadataE2E, StressTestManyEdgesWithMetadata) {
    // Create a larger graph and annotate all edges with metadata
    const int NUM_MODULES = 20;
    std::vector<brain_kg_node_id_t> modules;

    // Create modules
    for (int i = 0; i < NUM_MODULES; i++) {
        char name[64];
        snprintf(name, sizeof(name), "module_%02d", i);
        brain_kg_node_type_t type = (brain_kg_node_type_t)(i % (int)BRAIN_KG_NODE_TYPE_COUNT);
        brain_kg_node_id_t id = add_module(name, type);
        ASSERT_NE(id, BRAIN_KG_INVALID_NODE) << "Failed to add module " << i;
        modules.push_back(id);
    }

    // Create a chain of edges: 0->1->2->...->19
    for (int i = 0; i < NUM_MODULES - 1; i++) {
        brain_kg_add_edge(kg, modules[i], modules[i + 1],
                         BRAIN_KG_EDGE_SENDS_TO, "chain", 0.8f);
    }

    // Create some skip connections: 0->5, 5->10, 10->15, 0->19
    brain_kg_add_edge(kg, modules[0], modules[5],
                     BRAIN_KG_EDGE_CONNECTS_TO, "skip", 0.5f);
    brain_kg_add_edge(kg, modules[5], modules[10],
                     BRAIN_KG_EDGE_CONNECTS_TO, "skip", 0.5f);
    brain_kg_add_edge(kg, modules[10], modules[15],
                     BRAIN_KG_EDGE_CONNECTS_TO, "skip", 0.5f);
    brain_kg_add_edge(kg, modules[0], modules[19],
                     BRAIN_KG_EDGE_CONNECTS_TO, "skip", 0.3f);

    // Create hierarchy
    kg_hierarchy_config_t config;
    kg_hierarchy_default_config(&config);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Annotate chain edges with position metadata
    for (int i = 0; i < NUM_MODULES - 1; i++) {
        EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
            hier, modules[i], modules[i + 1], "position", i), 0)
            << "Failed at chain edge " << i;
        EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
            hier, modules[i], modules[i + 1], "bandwidth", 1000 - i * 50), 0)
            << "Failed at chain edge bandwidth " << i;
    }

    // Annotate skip connections
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, modules[0], modules[5], "skip_distance", 5), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, modules[5], modules[10], "skip_distance", 5), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, modules[10], modules[15], "skip_distance", 5), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, modules[0], modules[19], "skip_distance", 19), 0);

    // Verify all chain metadata
    for (int i = 0; i < NUM_MODULES - 1; i++) {
        int32_t pos = -1, bw = -1;
        EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
            hier, modules[i], modules[i + 1], "position", &pos), 0)
            << "Failed to read position at " << i;
        EXPECT_EQ(pos, i);

        EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
            hier, modules[i], modules[i + 1], "bandwidth", &bw), 0)
            << "Failed to read bandwidth at " << i;
        EXPECT_EQ(bw, 1000 - i * 50);
    }

    // Verify skip connection metadata
    int32_t skip = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, modules[0], modules[19], "skip_distance", &skip), 0);
    EXPECT_EQ(skip, 19);
}

TEST_F(KGHierarchyEdgeMetadataE2E, MetadataIntegrityAfterHierarchyOperations) {
    // Build graph, add metadata, perform various hierarchy operations,
    // then verify metadata is still intact

    brain_kg_node_id_t a = add_module("module_alpha", BRAIN_KG_NODE_CORE);
    brain_kg_node_id_t b = add_module("module_beta", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t c = add_module("module_gamma", BRAIN_KG_NODE_PERCEPTION);

    brain_kg_add_edge(kg, a, b, BRAIN_KG_EDGE_DEPENDS_ON, "dep", 1.0f);
    brain_kg_add_edge(kg, b, c, BRAIN_KG_EDGE_SENDS_TO, "data", 0.8f);
    brain_kg_add_edge(kg, a, c, BRAIN_KG_EDGE_CONNECTS_TO, "direct", 0.6f);

    kg_hierarchy_config_t config;
    kg_hierarchy_default_config(&config);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Set metadata on all edges
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(hier, a, b, "integrity_check", 111), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(hier, b, c, "integrity_check", 222), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(hier, a, c, "integrity_check", 333), 0);

    // Perform a sequence of hierarchy operations
    // 1. Query brain stats
    kg_brain_stats_t stats;
    kg_hierarchy_get_brain_stats(hier, &stats);

    // 2. Query hemispheres
    kg_hemisphere_info_t hemispheres[KG_HEMISPHERE_COUNT];
    kg_hierarchy_get_hemispheres(hier, hemispheres);

    // 3. Query layers
    kg_layer_info_t layers[KG_LAYER_COUNT];
    kg_hierarchy_get_layers(hier, layers);

    // 4. Report state changes
    kg_hierarchy_report_state_change(hier, a, KG_MODULE_STATE_RUNNING, "start");
    kg_hierarchy_report_state_change(hier, b, KG_MODULE_STATE_RUNNING, "start");
    kg_hierarchy_report_state_change(hier, c, KG_MODULE_STATE_RUNNING, "start");

    // 5. Report health
    kg_hierarchy_report_health_change(hier, a, BIO_MODULE_HEALTH_HEALTHY);
    kg_hierarchy_report_health_change(hier, b, BIO_MODULE_HEALTH_HEALTHY);
    kg_hierarchy_report_health_change(hier, c, BIO_MODULE_HEALTH_DEGRADED);

    // 6. Report anomaly
    kg_hierarchy_report_anomaly(hier, c, true);
    kg_hierarchy_report_anomaly(hier, c, false);

    // 7. Report message stats
    kg_hierarchy_report_message_stats(hier, a, 100, 50);
    kg_hierarchy_report_message_stats(hier, b, 200, 150);

    // 8. Invalidate and rebuild
    kg_hierarchy_invalidate(hier);
    kg_hierarchy_rebuild(hier);

    // 9. Query module info
    kg_module_info_t mod_info;
    kg_hierarchy_get_module_info(hier, a, &mod_info);

    // 10. Read lock/unlock
    kg_hierarchy_read_lock(hier);
    kg_hierarchy_read_unlock(hier);

    // VERIFY: All metadata should be intact after all these operations
    int32_t v_ab = 0, v_bc = 0, v_ac = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, a, b, "integrity_check", &v_ab), 0);
    EXPECT_EQ(v_ab, 111);

    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, b, c, "integrity_check", &v_bc), 0);
    EXPECT_EQ(v_bc, 222);

    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, a, c, "integrity_check", &v_ac), 0);
    EXPECT_EQ(v_ac, 333);
}

TEST_F(KGHierarchyEdgeMetadataE2E, MetadataWithTraversalOperations) {
    // Build graph, add metadata, then use traversal APIs and verify metadata

    brain_kg_node_id_t nodes[5];
    nodes[0] = add_module("start", BRAIN_KG_NODE_CORE);
    nodes[1] = add_module("mid_a", BRAIN_KG_NODE_COGNITIVE);
    nodes[2] = add_module("mid_b", BRAIN_KG_NODE_COGNITIVE);
    nodes[3] = add_module("mid_c", BRAIN_KG_NODE_PERCEPTION);
    nodes[4] = add_module("end", BRAIN_KG_NODE_INTEGRATION);

    // Create a diamond-shaped graph: start -> mid_a -> end, start -> mid_b -> end
    // Plus: start -> mid_c -> end
    brain_kg_add_edge(kg, nodes[0], nodes[1], BRAIN_KG_EDGE_SENDS_TO, "path_a", 0.9f);
    brain_kg_add_edge(kg, nodes[0], nodes[2], BRAIN_KG_EDGE_SENDS_TO, "path_b", 0.7f);
    brain_kg_add_edge(kg, nodes[0], nodes[3], BRAIN_KG_EDGE_SENDS_TO, "path_c", 0.5f);
    brain_kg_add_edge(kg, nodes[1], nodes[4], BRAIN_KG_EDGE_SENDS_TO, "merge_a", 0.8f);
    brain_kg_add_edge(kg, nodes[2], nodes[4], BRAIN_KG_EDGE_SENDS_TO, "merge_b", 0.6f);
    brain_kg_add_edge(kg, nodes[3], nodes[4], BRAIN_KG_EDGE_SENDS_TO, "merge_c", 0.4f);

    kg_hierarchy_config_t config;
    kg_hierarchy_default_config(&config);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Add metadata: label each edge with a path cost
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(hier, nodes[0], nodes[1], "cost", 10), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(hier, nodes[0], nodes[2], "cost", 30), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(hier, nodes[0], nodes[3], "cost", 50), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(hier, nodes[1], nodes[4], "cost", 15), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(hier, nodes[2], nodes[4], "cost", 25), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(hier, nodes[3], nodes[4], "cost", 35), 0);

    // Use shortest path API (hop count)
    brain_kg_node_id_t path[10];
    uint32_t path_len = 0;
    int path_result = kg_hierarchy_shortest_path(
        hier, nodes[0], nodes[4], path, 10, &path_len);
    // Should find a path of length 2 (start -> mid_x -> end)
    if (path_result == 0) {
        EXPECT_EQ(path_len, 3u);  // 3 nodes in path: start, mid_x, end
    }

    // Get distance
    uint32_t dist = kg_hierarchy_get_distance(hier, nodes[0], nodes[4]);
    EXPECT_LE(dist, 2u);  // At most 2 hops

    // Metadata should still be intact after traversal
    int32_t cost = 0;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, nodes[0], nodes[1], "cost", &cost), 0);
    EXPECT_EQ(cost, 10);

    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, nodes[2], nodes[4], "cost", &cost), 0);
    EXPECT_EQ(cost, 25);
}

TEST_F(KGHierarchyEdgeMetadataE2E, MetadataWithTopologicalSort) {
    // Build a DAG with dependency edges, add metadata, perform topo sort

    brain_kg_node_id_t init   = add_module("init_module", BRAIN_KG_NODE_CORE);
    brain_kg_node_id_t logger = add_module("logger", BRAIN_KG_NODE_UTILITY);
    brain_kg_node_id_t net    = add_module("network", BRAIN_KG_NODE_CORE);
    brain_kg_node_id_t app    = add_module("application", BRAIN_KG_NODE_COGNITIVE);

    // init -> logger, init -> net, logger -> app, net -> app
    brain_kg_add_edge(kg, init, logger, BRAIN_KG_EDGE_DEPENDS_ON, "dep", 1.0f);
    brain_kg_add_edge(kg, init, net, BRAIN_KG_EDGE_DEPENDS_ON, "dep", 1.0f);
    brain_kg_add_edge(kg, logger, app, BRAIN_KG_EDGE_DEPENDS_ON, "dep", 1.0f);
    brain_kg_add_edge(kg, net, app, BRAIN_KG_EDGE_DEPENDS_ON, "dep", 1.0f);

    kg_hierarchy_config_t config;
    kg_hierarchy_default_config(&config);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Add boot order metadata
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(hier, init, logger, "boot_phase", 0), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(hier, init, net, "boot_phase", 0), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(hier, logger, app, "boot_phase", 1), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(hier, net, app, "boot_phase", 1), 0);

    // Perform topological sort
    brain_kg_node_id_t order[10];
    uint32_t sorted_count = 0;
    int sort_result = kg_hierarchy_topological_sort(hier, order, 10, &sorted_count);
    EXPECT_EQ(sort_result, 0);
    EXPECT_EQ(sorted_count, 4u);

    // Verify no dependency cycles
    EXPECT_FALSE(kg_hierarchy_has_dependency_cycle(hier));

    // Metadata intact after sort
    int32_t phase = -1;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, init, logger, "boot_phase", &phase), 0);
    EXPECT_EQ(phase, 0);

    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, logger, app, "boot_phase", &phase), 0);
    EXPECT_EQ(phase, 1);
}

TEST_F(KGHierarchyEdgeMetadataE2E, MetadataWithConnectedComponents) {
    // Create a graph with multiple disconnected components, add metadata per component

    // Component 1: A -> B
    brain_kg_node_id_t a1 = add_module("comp1_a", BRAIN_KG_NODE_CORE);
    brain_kg_node_id_t b1 = add_module("comp1_b", BRAIN_KG_NODE_CORE);
    brain_kg_add_edge(kg, a1, b1, BRAIN_KG_EDGE_CONNECTS_TO, "link", 1.0f);

    // Component 2: C -> D -> E
    brain_kg_node_id_t c2 = add_module("comp2_c", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t d2 = add_module("comp2_d", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_node_id_t e2 = add_module("comp2_e", BRAIN_KG_NODE_COGNITIVE);
    brain_kg_add_edge(kg, c2, d2, BRAIN_KG_EDGE_SENDS_TO, "link", 0.8f);
    brain_kg_add_edge(kg, d2, e2, BRAIN_KG_EDGE_SENDS_TO, "link", 0.8f);

    // Isolated node (Component 3)
    brain_kg_node_id_t f3 = add_module("isolated_f", BRAIN_KG_NODE_PERCEPTION);

    kg_hierarchy_config_t config;
    kg_hierarchy_default_config(&config);
    hier = kg_hierarchy_create(kg, &config);
    ASSERT_NE(hier, nullptr);

    // Add component-identifying metadata
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, a1, b1, "component_id", 1), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, c2, d2, "component_id", 2), 0);
    EXPECT_EQ(kg_hierarchy_set_edge_metadata_int(
        hier, d2, e2, "component_id", 2), 0);

    // Find components
    uint32_t component_ids[10];
    uint32_t num_components = 0;
    int comp_result = kg_hierarchy_find_components(hier, component_ids, &num_components);
    if (comp_result == 0) {
        // Should have at least 2 components (isolated node may or may not count
        // depending on implementation)
        EXPECT_GE(num_components, 2u);
    }

    // Verify metadata by component
    int32_t comp1_id = -1, comp2_a_id = -1, comp2_b_id = -1;
    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, a1, b1, "component_id", &comp1_id), 0);
    EXPECT_EQ(comp1_id, 1);

    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, c2, d2, "component_id", &comp2_a_id), 0);
    EXPECT_EQ(comp2_a_id, 2);

    EXPECT_EQ(kg_hierarchy_get_edge_metadata_int(
        hier, d2, e2, "component_id", &comp2_b_id), 0);
    EXPECT_EQ(comp2_b_id, 2);

    // Suppress unused variable warning
    (void)f3;
}
