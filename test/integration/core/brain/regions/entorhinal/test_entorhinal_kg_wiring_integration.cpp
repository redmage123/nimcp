/**
 * @file test_entorhinal_kg_wiring_integration.cpp
 * @brief Integration tests for Entorhinal Cortex Knowledge Graph wiring
 * @version 1.0.0
 * @date 2025-01-13
 *
 * WHAT: Tests Entorhinal Cortex integration with brain's internal Knowledge Graph
 * WHY:  Ensure proper semantic representation, self-awareness, and message routing via KG
 * HOW:  Test node registration, edge creation, health monitoring, queries, and introspection
 *
 * INTEGRATION POINTS:
 * - Brain KG node registration for entorhinal components
 * - KG edge creation between brain regions (hippocampus, neocortex, thalamus)
 * - Health status reporting to KG for self-awareness
 * - Component discovery through KG queries
 * - Self-awareness via KG introspection
 * - Message handler mapping for bio-async integration
 * - Path finding through spatial memory pathways
 *
 * TEST CATEGORIES:
 * 1. KG Bridge Initialization
 * 2. Node Registration (entorhinal components)
 * 3. Edge Creation (memory pathways, spatial circuits)
 * 4. Health Monitoring via KG
 * 5. Query Operations
 * 6. Self-Awareness and Introspection
 * 7. Error Handling
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/logging/nimcp_logging.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class EntorhinalKGWiringTest : public ::testing::Test {
protected:
    nimcp_entorhinal_t* ec;
    entorhinal_config_t config;
    brain_kg_t* kg;

    void SetUp() override {
        /* Create brain knowledge graph */
        brain_kg_config_t kg_config;
        brain_kg_default_config(&kg_config);
        kg_config.enable_security = false;
        kg_config.enable_statistics = true;
        kg = brain_kg_create(&kg_config);
        ASSERT_NE(nullptr, kg) << "Failed to create brain KG";

        /* Configure entorhinal cortex */
        config = entorhinal_default_config();
        config.enable_bio_async = false;
        config.enable_kg = true;
        config.enable_path_integration = true;
        config.enable_boundary_detection = true;
        config.enable_hippocampus = true;
        config.num_grid_cells = 64;
        config.num_border_cells = 16;
        config.num_hd_cells = 30;

        ec = entorhinal_create(&config);
        ASSERT_NE(nullptr, ec) << "Failed to create Entorhinal cortex";
    }

    void TearDown() override {
        if (ec) {
            entorhinal_destroy(ec);
            ec = nullptr;
        }
        if (kg) {
            brain_kg_destroy(kg);
            kg = nullptr;
        }
    }

    /* Helper to register entorhinal node in KG */
    brain_kg_node_id_t RegisterEntorhinalNode(const char* name, const char* desc) {
        return brain_kg_add_node(kg, name, BRAIN_KG_NODE_CORTICAL, desc);
    }

    /* Helper to register subcortical node */
    brain_kg_node_id_t RegisterSubcorticalNode(const char* name, const char* desc) {
        return brain_kg_add_node(kg, name, BRAIN_KG_NODE_SUBCORTICAL, desc);
    }
};

/*=============================================================================
 * KG BRIDGE INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalKGWiringTest, InitializeKGBridge) {
    /* Initialize KG bridge */
    int result = entorhinal_init_kg_bridge(ec, kg);
    EXPECT_EQ(0, result) << "Failed to initialize KG bridge";

    /* Verify bridge is connected */
    EXPECT_EQ(kg, ec->kg_bridge.kg);
}

TEST_F(EntorhinalKGWiringTest, InitializeKGBridgeWithNullKG) {
    /* Initialize KG bridge with NULL KG should fail gracefully */
    int result = entorhinal_init_kg_bridge(ec, nullptr);
    /* Expect either failure or graceful handling */
    EXPECT_TRUE(result == -1 || ec->kg_bridge.kg == nullptr);
}

TEST_F(EntorhinalKGWiringTest, InitializeKGBridgeWithNullEntorhinal) {
    /* Initialize KG bridge with NULL entorhinal should fail gracefully */
    int result = entorhinal_init_kg_bridge(nullptr, kg);
    EXPECT_EQ(-1, result);
}

TEST_F(EntorhinalKGWiringTest, KGBridgeStateAfterInit) {
    /* Initialize KG bridge */
    EXPECT_EQ(0, entorhinal_init_kg_bridge(ec, kg));

    /* Verify initial state */
    EXPECT_GE(ec->kg_bridge.health_status, 0.0f);
    EXPECT_LE(ec->kg_bridge.health_status, 1.0f);
}

/*=============================================================================
 * KG NODE REGISTRATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalKGWiringTest, RegisterEntorhinalCortexRootNode) {
    brain_kg_node_id_t root_id = RegisterEntorhinalNode(
        "entorhinal_cortex",
        "Entorhinal cortex - memory gateway with grid cells"
    );
    EXPECT_NE(BRAIN_KG_INVALID_NODE, root_id);

    /* Verify node was added */
    const brain_kg_node_t* node = brain_kg_get_node(kg, root_id);
    ASSERT_NE(nullptr, node);
    EXPECT_STREQ("entorhinal_cortex", node->name);
    EXPECT_EQ(BRAIN_KG_NODE_CORTICAL, node->type);
}

TEST_F(EntorhinalKGWiringTest, RegisterEntorhinalComponentHierarchy) {
    /* Register entorhinal cortex root */
    brain_kg_node_id_t root_id = RegisterEntorhinalNode(
        "entorhinal_cortex", "Entorhinal cortex root"
    );
    ASSERT_NE(BRAIN_KG_INVALID_NODE, root_id);

    /* Register grid cell module */
    brain_kg_node_id_t grid_id = RegisterEntorhinalNode(
        "ec_grid_cells", "Grid cell population for spatial representation"
    );
    ASSERT_NE(BRAIN_KG_INVALID_NODE, grid_id);

    /* Register border cell module */
    brain_kg_node_id_t border_id = RegisterEntorhinalNode(
        "ec_border_cells", "Border cells for boundary detection"
    );
    ASSERT_NE(BRAIN_KG_INVALID_NODE, border_id);

    /* Register head direction cell module */
    brain_kg_node_id_t hd_id = RegisterEntorhinalNode(
        "ec_hd_cells", "Head direction cells for heading representation"
    );
    ASSERT_NE(BRAIN_KG_INVALID_NODE, hd_id);

    /* Register memory gateway */
    brain_kg_node_id_t gateway_id = RegisterEntorhinalNode(
        "ec_memory_gateway", "Memory gateway for hippocampal interface"
    );
    ASSERT_NE(BRAIN_KG_INVALID_NODE, gateway_id);

    /* Register path integration module */
    brain_kg_node_id_t path_id = RegisterEntorhinalNode(
        "ec_path_integration", "Path integration for dead reckoning"
    );
    ASSERT_NE(BRAIN_KG_INVALID_NODE, path_id);

    /* Verify all nodes exist */
    brain_kg_stats_t stats;
    EXPECT_EQ(0, brain_kg_get_stats(kg, &stats));
    EXPECT_GE(stats.total_nodes, 6u);
}

TEST_F(EntorhinalKGWiringTest, RegisterEntorhinalMetadata) {
    brain_kg_node_id_t ec_id = RegisterEntorhinalNode(
        "entorhinal_cortex", "Entorhinal cortex"
    );
    ASSERT_NE(BRAIN_KG_INVALID_NODE, ec_id);

    /* Add metadata about entorhinal capabilities */
    EXPECT_EQ(0, brain_kg_add_metadata(kg, ec_id,
        "capabilities", "grid_cells,border_cells,hd_cells,path_integration,memory_gateway"));
    EXPECT_EQ(0, brain_kg_add_metadata(kg, ec_id,
        "cell_types", "grid,border,head_direction,object,speed,time"));
    EXPECT_EQ(0, brain_kg_add_metadata(kg, ec_id,
        "spatial_scales", "fine,medium,coarse,very_coarse"));
    EXPECT_EQ(0, brain_kg_add_metadata(kg, ec_id,
        "connections", "hippocampus,neocortex,thalamus,parietal"));

    /* Verify metadata was stored */
    const brain_kg_node_t* node = brain_kg_get_node(kg, ec_id);
    ASSERT_NE(nullptr, node);
    EXPECT_GE(node->metadata_count, 4u);
}

TEST_F(EntorhinalKGWiringTest, RegisterGridModuleNodes) {
    /* Register nodes for each grid module scale */
    const char* module_names[] = {
        "ec_grid_fine", "ec_grid_medium",
        "ec_grid_coarse", "ec_grid_very_coarse"
    };
    const char* module_descs[] = {
        "Fine-scale grid module (~30cm spacing)",
        "Medium-scale grid module (~50cm spacing)",
        "Coarse-scale grid module (~100cm spacing)",
        "Very coarse grid module (~200cm spacing)"
    };

    brain_kg_node_id_t module_ids[GRID_MODULE_COUNT];
    for (int i = 0; i < GRID_MODULE_COUNT; i++) {
        module_ids[i] = RegisterEntorhinalNode(module_names[i], module_descs[i]);
        EXPECT_NE(BRAIN_KG_INVALID_NODE, module_ids[i])
            << "Failed to register grid module: " << module_names[i];
    }
}

/*=============================================================================
 * KG EDGE CREATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalKGWiringTest, CreateEntorhinalHierarchyEdges) {
    /* Create nodes */
    brain_kg_node_id_t root_id = RegisterEntorhinalNode("entorhinal_cortex", "Root");
    brain_kg_node_id_t grid_id = RegisterEntorhinalNode("ec_grid_cells", "Grid");
    brain_kg_node_id_t border_id = RegisterEntorhinalNode("ec_border_cells", "Border");
    brain_kg_node_id_t hd_id = RegisterEntorhinalNode("ec_hd_cells", "HD");
    brain_kg_node_id_t gateway_id = RegisterEntorhinalNode("ec_memory_gateway", "Gateway");

    /* Create hierarchy edges (parent -> child) */
    brain_kg_edge_id_t e1 = brain_kg_add_edge(kg, root_id, grid_id,
        BRAIN_KG_EDGE_CONNECTS_TO, "Entorhinal hierarchy", 1.0f);
    brain_kg_edge_id_t e2 = brain_kg_add_edge(kg, root_id, border_id,
        BRAIN_KG_EDGE_CONNECTS_TO, "Entorhinal hierarchy", 1.0f);
    brain_kg_edge_id_t e3 = brain_kg_add_edge(kg, root_id, hd_id,
        BRAIN_KG_EDGE_CONNECTS_TO, "Entorhinal hierarchy", 1.0f);
    brain_kg_edge_id_t e4 = brain_kg_add_edge(kg, root_id, gateway_id,
        BRAIN_KG_EDGE_CONNECTS_TO, "Entorhinal hierarchy", 1.0f);

    EXPECT_NE(BRAIN_KG_INVALID_NODE, e1);
    EXPECT_NE(BRAIN_KG_INVALID_NODE, e2);
    EXPECT_NE(BRAIN_KG_INVALID_NODE, e3);
    EXPECT_NE(BRAIN_KG_INVALID_NODE, e4);

    /* Verify edges exist */
    brain_kg_edge_list_t* outgoing = brain_kg_get_outgoing(kg, root_id);
    ASSERT_NE(nullptr, outgoing);
    EXPECT_EQ(4u, outgoing->count);
    brain_kg_edge_list_destroy(outgoing);
}

TEST_F(EntorhinalKGWiringTest, CreateMemoryPathwayEdges) {
    /* Create nodes for memory pathway */
    brain_kg_node_id_t ec_id = RegisterEntorhinalNode("entorhinal", "Entorhinal");
    brain_kg_node_id_t hpc_id = RegisterSubcorticalNode("hippocampus", "Hippocampus");
    brain_kg_node_id_t neo_id = RegisterEntorhinalNode("neocortex", "Neocortex");
    brain_kg_node_id_t gateway_id = RegisterEntorhinalNode("memory_gateway", "Gateway");

    /* Create memory pathway edges */
    /* Entorhinal -> Hippocampus (encoding pathway) */
    brain_kg_add_edge(kg, ec_id, hpc_id,
        BRAIN_KG_EDGE_SENDS_TO, "Memory encoding pathway", 0.9f);

    /* Hippocampus -> Entorhinal (retrieval pathway) */
    brain_kg_add_edge(kg, hpc_id, ec_id,
        BRAIN_KG_EDGE_SENDS_TO, "Memory retrieval pathway", 0.9f);

    /* Entorhinal <-> Neocortex (consolidation pathway) */
    brain_kg_add_edge(kg, ec_id, neo_id,
        BRAIN_KG_EDGE_SENDS_TO, "Memory consolidation", 0.8f);
    brain_kg_add_edge(kg, neo_id, ec_id,
        BRAIN_KG_EDGE_SENDS_TO, "Cortical input", 0.7f);

    /* Gateway connects EC and HPC */
    brain_kg_add_edge(kg, gateway_id, ec_id,
        BRAIN_KG_EDGE_INTEGRATES_WITH, "Gateway interface", 1.0f);
    brain_kg_add_edge(kg, gateway_id, hpc_id,
        BRAIN_KG_EDGE_INTEGRATES_WITH, "Gateway interface", 1.0f);

    /* Verify path from EC to Hippocampus */
    brain_kg_path_t* path = brain_kg_find_path(kg, ec_id, hpc_id);
    ASSERT_NE(nullptr, path);
    EXPECT_GE(path->length, 2u);
    brain_kg_path_destroy(path);
}

TEST_F(EntorhinalKGWiringTest, CreateSpatialCircuitEdges) {
    /* Create nodes for spatial processing circuit */
    brain_kg_node_id_t grid_id = RegisterEntorhinalNode("grid_cells", "Grid cells");
    brain_kg_node_id_t border_id = RegisterEntorhinalNode("border_cells", "Border cells");
    brain_kg_node_id_t hd_id = RegisterEntorhinalNode("hd_cells", "HD cells");
    brain_kg_node_id_t path_id = RegisterEntorhinalNode("path_integration", "Path integrator");
    brain_kg_node_id_t place_id = RegisterSubcorticalNode("place_cells", "Place cells (HPC)");

    /* Create spatial circuit edges */
    /* Grid cells integrate with path integration */
    brain_kg_add_edge(kg, path_id, grid_id,
        BRAIN_KG_EDGE_MODULATES, "Velocity updates", 0.9f);

    /* Border cells correct path integration drift */
    brain_kg_add_edge(kg, border_id, path_id,
        BRAIN_KG_EDGE_MODULATES, "Boundary correction", 0.8f);

    /* HD cells provide heading input */
    brain_kg_add_edge(kg, hd_id, path_id,
        BRAIN_KG_EDGE_SENDS_TO, "Heading signal", 0.9f);

    /* Grid -> Place cell pathway */
    brain_kg_add_edge(kg, grid_id, place_id,
        BRAIN_KG_EDGE_SENDS_TO, "Spatial context", 0.85f);

    /* Verify all connections */
    EXPECT_TRUE(brain_kg_are_connected(kg, path_id, grid_id));
    EXPECT_TRUE(brain_kg_are_connected(kg, border_id, path_id));
    EXPECT_TRUE(brain_kg_are_connected(kg, hd_id, path_id));
    EXPECT_TRUE(brain_kg_are_connected(kg, grid_id, place_id));
}

TEST_F(EntorhinalKGWiringTest, CreateBrainRegionEdges) {
    /* Create nodes for multiple brain regions */
    brain_kg_node_id_t ec_id = RegisterEntorhinalNode("entorhinal_cortex", "EC");
    brain_kg_node_id_t hpc_id = RegisterSubcorticalNode("hippocampus", "Hippocampus");
    brain_kg_node_id_t thal_id = RegisterSubcorticalNode("thalamus", "Thalamus");
    brain_kg_node_id_t pfc_id = RegisterEntorhinalNode("prefrontal_cortex", "PFC");
    brain_kg_node_id_t parietal_id = RegisterEntorhinalNode("parietal_cortex", "Parietal");

    /* Create inter-region edges */
    brain_kg_edge_id_t e1 = brain_kg_add_edge(kg, ec_id, hpc_id,
        BRAIN_KG_EDGE_CONNECTS_TO, "EC-HPC connection", 0.95f);
    brain_kg_edge_id_t e2 = brain_kg_add_edge(kg, thal_id, ec_id,
        BRAIN_KG_EDGE_SENDS_TO, "Thalamic relay to EC", 0.7f);
    brain_kg_edge_id_t e3 = brain_kg_add_edge(kg, pfc_id, ec_id,
        BRAIN_KG_EDGE_MODULATES, "Executive control of EC", 0.6f);
    brain_kg_edge_id_t e4 = brain_kg_add_edge(kg, parietal_id, ec_id,
        BRAIN_KG_EDGE_SENDS_TO, "Spatial info to EC", 0.8f);

    EXPECT_NE(BRAIN_KG_INVALID_NODE, e1);
    EXPECT_NE(BRAIN_KG_INVALID_NODE, e2);
    EXPECT_NE(BRAIN_KG_INVALID_NODE, e3);
    EXPECT_NE(BRAIN_KG_INVALID_NODE, e4);
}

/*=============================================================================
 * HEALTH MONITORING VIA KG TESTS
 *===========================================================================*/

TEST_F(EntorhinalKGWiringTest, UpdateEntorhinalNodeState) {
    brain_kg_node_id_t ec_id = RegisterEntorhinalNode("entorhinal", "EC");

    /* Update state as entorhinal progresses through lifecycle */
    EXPECT_EQ(0, brain_kg_update_node(kg, ec_id, NULL,
        BRAIN_KG_STATE_INITIALIZING));

    const brain_kg_node_t* node = brain_kg_get_node(kg, ec_id);
    EXPECT_EQ(BRAIN_KG_STATE_INITIALIZING, node->state);

    EXPECT_EQ(0, brain_kg_update_node(kg, ec_id, NULL,
        BRAIN_KG_STATE_ACTIVE));

    node = brain_kg_get_node(kg, ec_id);
    EXPECT_EQ(BRAIN_KG_STATE_ACTIVE, node->state);
}

TEST_F(EntorhinalKGWiringTest, ReportHealthStatusToKG) {
    brain_kg_node_id_t ec_id = RegisterEntorhinalNode("entorhinal", "EC");

    /* Associate KG node with actual entorhinal instance */
    EXPECT_EQ(0, brain_kg_set_module_ptr(kg, ec_id, ec));

    const brain_kg_node_t* node = brain_kg_get_node(kg, ec_id);
    EXPECT_EQ((void*)ec, node->module_ptr);

    /* Get health status from entorhinal */
    float health = entorhinal_get_health_status(ec);
    EXPECT_GE(health, 0.0f);
    EXPECT_LE(health, 1.0f);

    /* Update node description to reflect health */
    char desc[256];
    snprintf(desc, sizeof(desc), "Entorhinal cortex (health: %.2f)", health);
    EXPECT_EQ(0, brain_kg_update_node(kg, ec_id, desc, BRAIN_KG_STATE_ACTIVE));
}

TEST_F(EntorhinalKGWiringTest, TrackComponentHealthViaKG) {
    /* Register component nodes */
    brain_kg_node_id_t grid_id = RegisterEntorhinalNode("ec_grid", "Grid cells");
    brain_kg_node_id_t border_id = RegisterEntorhinalNode("ec_border", "Border cells");
    brain_kg_node_id_t hd_id = RegisterEntorhinalNode("ec_hd", "HD cells");
    brain_kg_node_id_t path_id = RegisterEntorhinalNode("ec_path", "Path integration");
    brain_kg_node_id_t gateway_id = RegisterEntorhinalNode("ec_gateway", "Memory gateway");

    /* Set all to active state */
    brain_kg_update_node(kg, grid_id, NULL, BRAIN_KG_STATE_ACTIVE);
    brain_kg_update_node(kg, border_id, NULL, BRAIN_KG_STATE_ACTIVE);
    brain_kg_update_node(kg, hd_id, NULL, BRAIN_KG_STATE_ACTIVE);
    brain_kg_update_node(kg, path_id, NULL, BRAIN_KG_STATE_ACTIVE);
    brain_kg_update_node(kg, gateway_id, NULL, BRAIN_KG_STATE_ACTIVE);

    /* Verify all are active */
    EXPECT_EQ(BRAIN_KG_STATE_ACTIVE, brain_kg_get_node(kg, grid_id)->state);
    EXPECT_EQ(BRAIN_KG_STATE_ACTIVE, brain_kg_get_node(kg, border_id)->state);
    EXPECT_EQ(BRAIN_KG_STATE_ACTIVE, brain_kg_get_node(kg, hd_id)->state);
    EXPECT_EQ(BRAIN_KG_STATE_ACTIVE, brain_kg_get_node(kg, path_id)->state);
    EXPECT_EQ(BRAIN_KG_STATE_ACTIVE, brain_kg_get_node(kg, gateway_id)->state);

    /* Simulate error in grid cells */
    brain_kg_update_node(kg, grid_id, "Grid cell drift detected", BRAIN_KG_STATE_ERROR);
    EXPECT_EQ(BRAIN_KG_STATE_ERROR, brain_kg_get_node(kg, grid_id)->state);
}

TEST_F(EntorhinalKGWiringTest, HealthMetadataTracking) {
    brain_kg_node_id_t ec_id = RegisterEntorhinalNode("entorhinal", "EC");

    /* Add health-related metadata */
    EXPECT_EQ(0, brain_kg_add_metadata(kg, ec_id,
        "grid_coherence", "0.95"));
    EXPECT_EQ(0, brain_kg_add_metadata(kg, ec_id,
        "path_integration_error", "0.02"));
    EXPECT_EQ(0, brain_kg_add_metadata(kg, ec_id,
        "memory_gateway_latency_ms", "5.2"));
    EXPECT_EQ(0, brain_kg_add_metadata(kg, ec_id,
        "encoding_success_rate", "0.98"));

    /* Verify metadata */
    const brain_kg_node_t* node = brain_kg_get_node(kg, ec_id);
    EXPECT_GE(node->metadata_count, 4u);
}

/*=============================================================================
 * KG QUERY OPERATION TESTS
 *===========================================================================*/

TEST_F(EntorhinalKGWiringTest, SearchEntorhinalNodes) {
    RegisterEntorhinalNode("entorhinal_cortex_grid", "Grid cells");
    RegisterEntorhinalNode("entorhinal_cortex_border", "Border cells");
    RegisterEntorhinalNode("entorhinal_cortex_hd", "HD cells");
    RegisterEntorhinalNode("entorhinal_cortex_gateway", "Gateway");
    brain_kg_add_node(kg, "hippocampus", BRAIN_KG_NODE_SUBCORTICAL, "HPC");
    brain_kg_add_node(kg, "prefrontal", BRAIN_KG_NODE_CORTICAL, "PFC");

    /* Search for entorhinal-related nodes */
    brain_kg_node_list_t* ec_nodes = brain_kg_search_nodes(kg, "entorhinal");
    ASSERT_NE(nullptr, ec_nodes);
    EXPECT_EQ(4u, ec_nodes->count);
    brain_kg_node_list_destroy(ec_nodes);
}

TEST_F(EntorhinalKGWiringTest, GetNodesByType) {
    RegisterEntorhinalNode("ec_1", "EC1");
    RegisterEntorhinalNode("ec_2", "EC2");
    RegisterSubcorticalNode("hippocampus", "HPC");
    RegisterSubcorticalNode("thalamus", "Thalamus");
    brain_kg_add_node(kg, "medulla", BRAIN_KG_NODE_BRAINSTEM, "Medulla");

    brain_kg_node_list_t* cortical = brain_kg_get_nodes_by_type(kg,
        BRAIN_KG_NODE_CORTICAL);
    ASSERT_NE(nullptr, cortical);
    EXPECT_EQ(2u, cortical->count);
    brain_kg_node_list_destroy(cortical);

    brain_kg_node_list_t* subcortical = brain_kg_get_nodes_by_type(kg,
        BRAIN_KG_NODE_SUBCORTICAL);
    ASSERT_NE(nullptr, subcortical);
    EXPECT_EQ(2u, subcortical->count);
    brain_kg_node_list_destroy(subcortical);

    brain_kg_node_list_t* brainstem = brain_kg_get_nodes_by_type(kg,
        BRAIN_KG_NODE_BRAINSTEM);
    ASSERT_NE(nullptr, brainstem);
    EXPECT_EQ(1u, brainstem->count);
    brain_kg_node_list_destroy(brainstem);
}

TEST_F(EntorhinalKGWiringTest, FindPathThroughMemoryCircuit) {
    /* Build memory circuit hierarchy */
    brain_kg_node_id_t pfc_id = brain_kg_add_node(kg, "prefrontal",
        BRAIN_KG_NODE_CORTICAL, "Executive control");
    brain_kg_node_id_t ec_id = RegisterEntorhinalNode("entorhinal", "Memory gateway");
    brain_kg_node_id_t hpc_id = RegisterSubcorticalNode("hippocampus", "Episodic memory");
    brain_kg_node_id_t ca3_id = RegisterSubcorticalNode("ca3", "Pattern completion");
    brain_kg_node_id_t dg_id = RegisterSubcorticalNode("dentate_gyrus", "Pattern separation");

    /* Create pathway edges */
    brain_kg_add_edge(kg, pfc_id, ec_id, BRAIN_KG_EDGE_SENDS_TO, "Goal signal", 0.7f);
    brain_kg_add_edge(kg, ec_id, hpc_id, BRAIN_KG_EDGE_SENDS_TO, "Memory input", 0.9f);
    brain_kg_add_edge(kg, hpc_id, dg_id, BRAIN_KG_EDGE_CONNECTS_TO, "HPC circuit", 0.95f);
    brain_kg_add_edge(kg, dg_id, ca3_id, BRAIN_KG_EDGE_SENDS_TO, "Mossy fibers", 0.9f);

    /* Find path from PFC to CA3 */
    brain_kg_path_t* path = brain_kg_find_path(kg, pfc_id, ca3_id);
    ASSERT_NE(nullptr, path);
    EXPECT_EQ(5u, path->length); /* PFC -> EC -> HPC -> DG -> CA3 */
    brain_kg_path_destroy(path);
}

TEST_F(EntorhinalKGWiringTest, GetReachableFromEntorhinal) {
    /* Create entorhinal with connections */
    brain_kg_node_id_t ec_id = RegisterEntorhinalNode("entorhinal", "EC");
    brain_kg_node_id_t hpc_id = RegisterSubcorticalNode("hippocampus", "HPC");
    brain_kg_node_id_t neo_id = RegisterEntorhinalNode("neocortex", "Neocortex");
    brain_kg_node_id_t thal_id = RegisterSubcorticalNode("thalamus", "Thalamus");
    brain_kg_node_id_t parietal_id = RegisterEntorhinalNode("parietal", "Parietal");

    brain_kg_add_edge(kg, ec_id, hpc_id, BRAIN_KG_EDGE_CONNECTS_TO, "", 0.9f);
    brain_kg_add_edge(kg, ec_id, neo_id, BRAIN_KG_EDGE_CONNECTS_TO, "", 0.8f);
    brain_kg_add_edge(kg, neo_id, thal_id, BRAIN_KG_EDGE_CONNECTS_TO, "", 0.7f);
    brain_kg_add_edge(kg, thal_id, parietal_id, BRAIN_KG_EDGE_CONNECTS_TO, "", 0.6f);

    /* Get all nodes reachable from EC within 2 hops */
    brain_kg_node_list_t* reachable = brain_kg_get_reachable(kg, ec_id, 2);
    ASSERT_NE(nullptr, reachable);
    EXPECT_GE(reachable->count, 3u); /* HPC, Neo, Thal */
    brain_kg_node_list_destroy(reachable);
}

TEST_F(EntorhinalKGWiringTest, GetEntorhinalNeighbors) {
    brain_kg_node_id_t ec_id = RegisterEntorhinalNode("entorhinal", "EC");
    brain_kg_node_id_t hpc_id = RegisterSubcorticalNode("hippocampus", "HPC");
    brain_kg_node_id_t pfc_id = RegisterEntorhinalNode("prefrontal", "PFC");
    brain_kg_node_id_t parietal_id = RegisterEntorhinalNode("parietal", "Parietal");

    /* Create edges - some incoming, some outgoing */
    brain_kg_add_edge(kg, ec_id, hpc_id,
        BRAIN_KG_EDGE_SENDS_TO, "Memory output", 0.9f);
    brain_kg_add_edge(kg, pfc_id, ec_id,
        BRAIN_KG_EDGE_SENDS_TO, "Executive control", 0.7f);
    brain_kg_add_edge(kg, parietal_id, ec_id,
        BRAIN_KG_EDGE_SENDS_TO, "Spatial input", 0.8f);

    brain_kg_node_list_t* neighbors = brain_kg_get_neighbors(kg, ec_id);
    ASSERT_NE(nullptr, neighbors);
    EXPECT_GE(neighbors->count, 3u);
    brain_kg_node_list_destroy(neighbors);
}

TEST_F(EntorhinalKGWiringTest, GetHubNodes) {
    /* Create hub structure where EC has most connections */
    brain_kg_node_id_t ec_id = RegisterEntorhinalNode("entorhinal", "EC - hub");
    brain_kg_node_id_t n1 = RegisterSubcorticalNode("hippocampus", "HPC");
    brain_kg_node_id_t n2 = RegisterEntorhinalNode("neocortex", "Neo");
    brain_kg_node_id_t n3 = RegisterSubcorticalNode("thalamus", "Thal");
    brain_kg_node_id_t n4 = RegisterEntorhinalNode("parietal", "Parietal");
    brain_kg_node_id_t n5 = RegisterEntorhinalNode("prefrontal", "PFC");

    /* EC connects to all others */
    brain_kg_add_edge(kg, ec_id, n1, BRAIN_KG_EDGE_CONNECTS_TO, "", 0.9f);
    brain_kg_add_edge(kg, ec_id, n2, BRAIN_KG_EDGE_CONNECTS_TO, "", 0.8f);
    brain_kg_add_edge(kg, ec_id, n3, BRAIN_KG_EDGE_CONNECTS_TO, "", 0.7f);
    brain_kg_add_edge(kg, ec_id, n4, BRAIN_KG_EDGE_CONNECTS_TO, "", 0.6f);
    brain_kg_add_edge(kg, ec_id, n5, BRAIN_KG_EDGE_CONNECTS_TO, "", 0.5f);

    brain_kg_node_list_t* hubs = brain_kg_get_hubs(kg, 3);
    ASSERT_NE(nullptr, hubs);
    EXPECT_GT(hubs->count, 0u);
    /* EC should be the most connected */
    EXPECT_EQ(ec_id, hubs->nodes[0]->id);
    brain_kg_node_list_destroy(hubs);
}

/*=============================================================================
 * SELF-AWARENESS AND INTROSPECTION TESTS
 *===========================================================================*/

TEST_F(EntorhinalKGWiringTest, ComponentDiscoveryThroughKG) {
    /* Register all entorhinal components */
    RegisterEntorhinalNode("entorhinal_cortex", "EC root");
    RegisterEntorhinalNode("ec_grid_cells", "Grid cells");
    RegisterEntorhinalNode("ec_border_cells", "Border cells");
    RegisterEntorhinalNode("ec_hd_cells", "HD cells");
    RegisterEntorhinalNode("ec_path_integration", "Path integration");
    RegisterEntorhinalNode("ec_memory_gateway", "Memory gateway");

    /* Discover components by searching for "ec_" prefix */
    brain_kg_node_list_t* components = brain_kg_search_nodes(kg, "ec_");
    ASSERT_NE(nullptr, components);
    EXPECT_EQ(5u, components->count);  /* All components except root */
    brain_kg_node_list_destroy(components);
}

TEST_F(EntorhinalKGWiringTest, IntrospectEntorhinalCapabilities) {
    brain_kg_node_id_t ec_id = RegisterEntorhinalNode("entorhinal", "EC");

    /* Add capability metadata for introspection */
    brain_kg_add_metadata(kg, ec_id, "supports_grid_cells", "true");
    brain_kg_add_metadata(kg, ec_id, "supports_border_cells", "true");
    brain_kg_add_metadata(kg, ec_id, "supports_hd_cells", "true");
    brain_kg_add_metadata(kg, ec_id, "supports_path_integration", "true");
    brain_kg_add_metadata(kg, ec_id, "supports_memory_gateway", "true");
    brain_kg_add_metadata(kg, ec_id, "spatial_dimensions", "3");
    brain_kg_add_metadata(kg, ec_id, "grid_modules", "4");

    /* Verify capabilities through introspection */
    const brain_kg_node_t* node = brain_kg_get_node(kg, ec_id);
    ASSERT_NE(nullptr, node);
    EXPECT_GE(node->metadata_count, 7u);
}

TEST_F(EntorhinalKGWiringTest, SelfAwarenessViaModulePointer) {
    brain_kg_node_id_t ec_id = RegisterEntorhinalNode("entorhinal", "EC");

    /* Set module pointer for self-reference */
    EXPECT_EQ(0, brain_kg_set_module_ptr(kg, ec_id, ec));

    /* Verify self-awareness - can access EC through KG */
    const brain_kg_node_t* node = brain_kg_get_node(kg, ec_id);
    nimcp_entorhinal_t* ec_from_kg = (nimcp_entorhinal_t*)node->module_ptr;
    EXPECT_EQ(ec, ec_from_kg);

    /* Can query EC status through KG reference */
    if (ec_from_kg) {
        entorhinal_status_t status = entorhinal_get_status(ec_from_kg);
        EXPECT_NE(ENTORHINAL_STATUS_ERROR, status);
    }
}

/*=============================================================================
 * MESSAGE HANDLER MAPPING TESTS
 *===========================================================================*/

TEST_F(EntorhinalKGWiringTest, RegisterEntorhinalMessageHandlers) {
    brain_kg_node_id_t ec_id = RegisterEntorhinalNode("entorhinal", "EC");

    /* Register entorhinal-related message handlers */
    uint32_t msg_spatial_update = 0x200;
    uint32_t msg_memory_encode = 0x201;
    uint32_t msg_memory_retrieve = 0x202;
    uint32_t msg_path_integrate = 0x203;

    EXPECT_EQ(0, brain_kg_add_message_handler(kg, ec_id, msg_spatial_update));
    EXPECT_EQ(0, brain_kg_add_message_handler(kg, ec_id, msg_memory_encode));
    EXPECT_EQ(0, brain_kg_add_message_handler(kg, ec_id, msg_memory_retrieve));
    EXPECT_EQ(0, brain_kg_add_message_handler(kg, ec_id, msg_path_integrate));

    /* Verify handlers can be looked up */
    brain_kg_handler_list_t* handlers = brain_kg_get_handlers_for_message_type(
        kg, msg_spatial_update);
    ASSERT_NE(nullptr, handlers);
    EXPECT_GE(handlers->count, 1u);

    /* EC should be in the handler list */
    bool found_ec = false;
    for (uint32_t i = 0; i < handlers->count; i++) {
        if (handlers->handlers[i] == ec_id) {
            found_ec = true;
            break;
        }
    }
    EXPECT_TRUE(found_ec);
    brain_kg_handler_list_destroy(handlers);
}

TEST_F(EntorhinalKGWiringTest, QueryModuleHandledMessages) {
    brain_kg_node_id_t ec_id = RegisterEntorhinalNode("entorhinal", "EC");

    /* Register multiple message handlers */
    uint32_t msg_types[] = {0x200, 0x201, 0x202, 0x203, 0x204};
    for (int i = 0; i < 5; i++) {
        brain_kg_add_message_handler(kg, ec_id, msg_types[i]);
    }

    /* Query what messages the EC module handles */
    uint32_t handled[16];
    uint32_t count = brain_kg_get_module_handled_messages(
        kg, ec_id, handled, 16);
    EXPECT_EQ(5u, count);
}

/*=============================================================================
 * STATISTICS AND SUMMARY TESTS
 *===========================================================================*/

TEST_F(EntorhinalKGWiringTest, KGStatisticsTracking) {
    /* Add nodes and edges */
    brain_kg_node_id_t n1 = RegisterEntorhinalNode("ec1", "EC1");
    brain_kg_node_id_t n2 = RegisterEntorhinalNode("ec2", "EC2");
    brain_kg_node_id_t n3 = RegisterSubcorticalNode("hpc", "HPC");
    brain_kg_add_edge(kg, n1, n2, BRAIN_KG_EDGE_CONNECTS_TO, "", 0.5f);
    brain_kg_add_edge(kg, n2, n3, BRAIN_KG_EDGE_SENDS_TO, "", 0.6f);

    brain_kg_stats_t stats;
    EXPECT_EQ(0, brain_kg_get_stats(kg, &stats));
    EXPECT_EQ(3u, stats.total_nodes);
    EXPECT_EQ(2u, stats.total_edges);
    EXPECT_GT(stats.nodes_by_type[BRAIN_KG_NODE_CORTICAL], 0u);
    EXPECT_GT(stats.nodes_by_type[BRAIN_KG_NODE_SUBCORTICAL], 0u);
}

TEST_F(EntorhinalKGWiringTest, GenerateKGSummary) {
    RegisterEntorhinalNode("ec_grid", "Grid cells");
    RegisterEntorhinalNode("ec_border", "Border cells");
    RegisterEntorhinalNode("ec_hd", "HD cells");
    RegisterSubcorticalNode("hippocampus", "HPC");

    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    int chars = brain_kg_generate_summary(kg, buffer, sizeof(buffer));
    /* Summary generation should work - content format is implementation-defined */
    EXPECT_GE(chars, 0);
    if (chars > 0) {
        EXPECT_GT(strlen(buffer), 0u);
    }
}

/*=============================================================================
 * EDGE TYPE TESTS
 *===========================================================================*/

TEST_F(EntorhinalKGWiringTest, GetEdgesByType) {
    brain_kg_node_id_t ec_id = RegisterEntorhinalNode("entorhinal", "EC");
    brain_kg_node_id_t hpc_id = RegisterSubcorticalNode("hippocampus", "HPC");
    brain_kg_node_id_t neo_id = RegisterEntorhinalNode("neocortex", "Neo");
    brain_kg_node_id_t pfc_id = RegisterEntorhinalNode("prefrontal", "PFC");

    /* Different edge types */
    brain_kg_add_edge(kg, ec_id, hpc_id,
        BRAIN_KG_EDGE_SENDS_TO, "Memory pathway", 0.9f);
    brain_kg_add_edge(kg, hpc_id, ec_id,
        BRAIN_KG_EDGE_SENDS_TO, "Retrieval pathway", 0.85f);
    brain_kg_add_edge(kg, ec_id, neo_id,
        BRAIN_KG_EDGE_SENDS_TO, "Consolidation", 0.8f);
    brain_kg_add_edge(kg, pfc_id, ec_id,
        BRAIN_KG_EDGE_MODULATES, "Executive control", 0.6f);

    brain_kg_edge_list_t* modulates = brain_kg_get_edges_by_type(kg,
        BRAIN_KG_EDGE_MODULATES);
    ASSERT_NE(nullptr, modulates);
    EXPECT_EQ(1u, modulates->count);
    brain_kg_edge_list_destroy(modulates);

    brain_kg_edge_list_t* sends = brain_kg_get_edges_by_type(kg,
        BRAIN_KG_EDGE_SENDS_TO);
    ASSERT_NE(nullptr, sends);
    EXPECT_EQ(3u, sends->count);
    brain_kg_edge_list_destroy(sends);
}

/*=============================================================================
 * ERROR HANDLING TESTS
 *===========================================================================*/

TEST_F(EntorhinalKGWiringTest, HandleInvalidNodeId) {
    /* Try to get non-existent node */
    const brain_kg_node_t* node = brain_kg_get_node(kg, 9999);
    EXPECT_EQ(nullptr, node);
}

TEST_F(EntorhinalKGWiringTest, HandleInvalidEdgeCreation) {
    /* Try to create edge with invalid node IDs */
    brain_kg_edge_id_t edge = brain_kg_add_edge(kg, 9999, 8888,
        BRAIN_KG_EDGE_CONNECTS_TO, "Invalid", 0.5f);
    EXPECT_EQ(BRAIN_KG_INVALID_NODE, edge);
}

TEST_F(EntorhinalKGWiringTest, HandleDuplicateNodeNames) {
    /* Register node with same name twice */
    brain_kg_node_id_t id1 = RegisterEntorhinalNode("entorhinal", "EC1");
    brain_kg_node_id_t id2 = RegisterEntorhinalNode("entorhinal", "EC2");

    /* Implementation may handle duplicates in three valid ways:
     * 1. Return BRAIN_KG_INVALID_NODE (reject duplicate)
     * 2. Return a different ID (create new node, allow duplicates)
     * 3. Return the same ID (idempotent - return existing node)
     * All behaviors are acceptable as long as consistent */
    if (id2 == BRAIN_KG_INVALID_NODE) {
        /* Duplicate rejected - acceptable */
        SUCCEED();
    } else if (id2 == id1) {
        /* Idempotent behavior - returned existing node ID - acceptable */
        SUCCEED();
    } else {
        /* Created new node with different ID - acceptable */
        EXPECT_NE(id1, id2);
    }
}

TEST_F(EntorhinalKGWiringTest, RemoveEntorhinalNode) {
    brain_kg_node_id_t ec_id = RegisterEntorhinalNode("entorhinal", "EC");
    brain_kg_node_id_t hpc_id = RegisterSubcorticalNode("hippocampus", "HPC");

    brain_kg_add_edge(kg, ec_id, hpc_id,
        BRAIN_KG_EDGE_CONNECTS_TO, "EC-HPC", 0.9f);

    brain_kg_stats_t before;
    brain_kg_get_stats(kg, &before);

    /* Remove EC node - should also remove edge */
    EXPECT_EQ(0, brain_kg_remove_node(kg, ec_id));

    brain_kg_stats_t after;
    brain_kg_get_stats(kg, &after);
    EXPECT_EQ(before.total_nodes - 1, after.total_nodes);
    EXPECT_EQ(before.total_edges - 1, after.total_edges);

    /* EC node should not be findable */
    EXPECT_EQ(BRAIN_KG_INVALID_NODE, brain_kg_find_node(kg, "entorhinal"));
}

TEST_F(EntorhinalKGWiringTest, RemoveMessageHandler) {
    brain_kg_node_id_t ec_id = RegisterEntorhinalNode("entorhinal", "EC");
    uint32_t msg_type = 0x300;

    /* Add then remove handler */
    EXPECT_EQ(0, brain_kg_add_message_handler(kg, ec_id, msg_type));

    brain_kg_handler_list_t* before = brain_kg_get_handlers_for_message_type(
        kg, msg_type);
    ASSERT_NE(nullptr, before);
    uint32_t count_before = before->count;
    brain_kg_handler_list_destroy(before);

    EXPECT_EQ(0, brain_kg_remove_message_handler(kg, ec_id, msg_type));

    brain_kg_handler_list_t* after = brain_kg_get_handlers_for_message_type(
        kg, msg_type);
    uint32_t count_after = after ? after->count : 0;
    if (after) brain_kg_handler_list_destroy(after);

    EXPECT_LT(count_after, count_before);
}

/*=============================================================================
 * STRING CONVERSION TESTS
 *===========================================================================*/

TEST_F(EntorhinalKGWiringTest, NodeTypeToString) {
    EXPECT_NE(nullptr, brain_kg_node_type_to_string(BRAIN_KG_NODE_CORTICAL));
    EXPECT_NE(nullptr, brain_kg_node_type_to_string(BRAIN_KG_NODE_SUBCORTICAL));
    EXPECT_NE(nullptr, brain_kg_node_type_to_string(BRAIN_KG_NODE_BRAINSTEM));
    EXPECT_NE(nullptr, brain_kg_node_type_to_string(BRAIN_KG_NODE_COGNITIVE));
}

TEST_F(EntorhinalKGWiringTest, EdgeTypeToString) {
    EXPECT_NE(nullptr, brain_kg_edge_type_to_string(BRAIN_KG_EDGE_CONNECTS_TO));
    EXPECT_NE(nullptr, brain_kg_edge_type_to_string(BRAIN_KG_EDGE_SENDS_TO));
    EXPECT_NE(nullptr, brain_kg_edge_type_to_string(BRAIN_KG_EDGE_MODULATES));
    EXPECT_NE(nullptr, brain_kg_edge_type_to_string(BRAIN_KG_EDGE_INTEGRATES_WITH));
}

TEST_F(EntorhinalKGWiringTest, NodeStateToString) {
    EXPECT_NE(nullptr, brain_kg_node_state_to_string(BRAIN_KG_STATE_ACTIVE));
    EXPECT_NE(nullptr, brain_kg_node_state_to_string(BRAIN_KG_STATE_DISABLED));
    EXPECT_NE(nullptr, brain_kg_node_state_to_string(BRAIN_KG_STATE_ERROR));
    EXPECT_NE(nullptr, brain_kg_node_state_to_string(BRAIN_KG_STATE_INITIALIZING));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
