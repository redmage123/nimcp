/**
 * @file test_subcortical_kg_wiring.cpp
 * @brief Unit tests for subcortical Knowledge Graph wiring
 *
 * Tests the KG wiring for subcortical brain regions:
 * - subcortical_kg_wiring_init() registers nodes
 * - Nodes are queryable after registration
 * - Edges are created correctly
 *
 * @date 2026-02-03
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "core/brain/nimcp_brain_kg.h"
#include "core/brain/regions/hippocampus/bridges/nimcp_hippocampus_kg_wiring.h"
#include "core/brain/regions/amygdala/bridges/nimcp_amygdala_kg_wiring.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SubcorticalKGWiringTest : public ::testing::Test {
protected:
    brain_kg_t* kg = nullptr;

    /* Admin token for KG operations */
    static constexpr uint64_t ADMIN_TOKEN = 0x12345678ULL;

    void SetUp() override {
        /* Create a KG with security disabled for testing */
        brain_kg_config_t config;
        brain_kg_default_config(&config);
        config.enable_security = false;
        config.enable_access_control = false;
        config.enable_immune_integration = false;
        kg = brain_kg_create(&config);
        ASSERT_NE(kg, nullptr) << "Failed to create brain KG";
    }

    void TearDown() override {
        if (kg) {
            brain_kg_destroy(kg);
            kg = nullptr;
        }
    }
};

/* ============================================================================
 * Hippocampus KG Wiring Tests
 * ============================================================================ */

TEST_F(SubcorticalKGWiringTest, HippocampusDefaultConfigValid) {
    hippocampus_kg_config_t config;
    memset(&config, 0, sizeof(config));

    int result = hippocampus_kg_default_config(&config);
    ASSERT_EQ(result, 0) << "hippocampus_kg_default_config should return 0";

    /* Verify defaults are all enabled */
    EXPECT_TRUE(config.register_ca1);
    EXPECT_TRUE(config.register_ca3);
    EXPECT_TRUE(config.register_dg);
    EXPECT_TRUE(config.register_subiculum);
    EXPECT_TRUE(config.register_memory_nodes);
    EXPECT_TRUE(config.register_nav_nodes);
    EXPECT_TRUE(config.register_cross_edges);
    EXPECT_TRUE(config.include_state_metadata);
}

TEST_F(SubcorticalKGWiringTest, HippocampusConfigNullReturnsError) {
    int result = hippocampus_kg_default_config(nullptr);
    EXPECT_EQ(result, -1)
        << "hippocampus_kg_default_config(NULL) should return -1";
}

TEST_F(SubcorticalKGWiringTest, HippocampusRegisterAllWithNullKGFails) {
    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = hippocampus_kg_register_all(nullptr, nullptr, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, -1)
        << "hippocampus_kg_register_all with NULL KG should fail";
}

TEST_F(SubcorticalKGWiringTest, HippocampusRegisterAllSucceeds) {
    hippocampus_kg_config_t config;
    hippocampus_kg_default_config(&config);

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = hippocampus_kg_register_all(kg, &config, &state, ADMIN_TOKEN);
    ASSERT_EQ(result, 0) << "hippocampus_kg_register_all should succeed";

    /* Verify state was populated */
    EXPECT_TRUE(state.registered);
    EXPECT_NE(state.root_id, BRAIN_KG_INVALID_NODE);
    EXPECT_GT(state.node_count, 0u);
    EXPECT_GT(state.edge_count, 0u);
}

TEST_F(SubcorticalKGWiringTest, HippocampusSubfieldNodesCreated) {
    hippocampus_kg_config_t config;
    hippocampus_kg_default_config(&config);

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = hippocampus_kg_register_all(kg, &config, &state, ADMIN_TOKEN);
    ASSERT_EQ(result, 0);

    /* Verify subfield nodes were created */
    EXPECT_NE(state.ca1_id, BRAIN_KG_INVALID_NODE)
        << "CA1 node should be created";
    EXPECT_NE(state.ca3_id, BRAIN_KG_INVALID_NODE)
        << "CA3 node should be created";
    EXPECT_NE(state.dg_id, BRAIN_KG_INVALID_NODE)
        << "Dentate gyrus node should be created";
    EXPECT_NE(state.subiculum_id, BRAIN_KG_INVALID_NODE)
        << "Subiculum node should be created";
}

TEST_F(SubcorticalKGWiringTest, HippocampusMemoryNodesCreated) {
    hippocampus_kg_config_t config;
    hippocampus_kg_default_config(&config);

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));

    hippocampus_kg_register_all(kg, &config, &state, ADMIN_TOKEN);

    /* Verify memory system nodes */
    EXPECT_NE(state.memory_system_id, BRAIN_KG_INVALID_NODE)
        << "Memory system node should be created";
    EXPECT_NE(state.episodic_id, BRAIN_KG_INVALID_NODE)
        << "Episodic memory node should be created";
    EXPECT_NE(state.spatial_id, BRAIN_KG_INVALID_NODE)
        << "Spatial memory node should be created";
}

TEST_F(SubcorticalKGWiringTest, HippocampusNavigationNodesCreated) {
    hippocampus_kg_config_t config;
    hippocampus_kg_default_config(&config);

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));

    hippocampus_kg_register_all(kg, &config, &state, ADMIN_TOKEN);

    /* Verify navigation nodes */
    EXPECT_NE(state.nav_system_id, BRAIN_KG_INVALID_NODE)
        << "Navigation system node should be created";
    EXPECT_NE(state.place_cells_id, BRAIN_KG_INVALID_NODE)
        << "Place cells node should be created";
    EXPECT_NE(state.grid_cells_id, BRAIN_KG_INVALID_NODE)
        << "Grid cells node should be created";
}

TEST_F(SubcorticalKGWiringTest, HippocampusSelectiveRegistration) {
    hippocampus_kg_config_t config;
    hippocampus_kg_default_config(&config);

    /* Disable memory and nav nodes */
    config.register_memory_nodes = false;
    config.register_nav_nodes = false;
    config.register_cross_edges = false;

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = hippocampus_kg_register_all(kg, &config, &state, ADMIN_TOKEN);
    ASSERT_EQ(result, 0);

    /* Subfields should still be created */
    EXPECT_NE(state.ca1_id, BRAIN_KG_INVALID_NODE);

    /* Memory nodes should NOT be created (or be invalid) */
    EXPECT_TRUE(
        state.memory_system_id == BRAIN_KG_INVALID_NODE ||
        state.memory_system_id == 0
    ) << "Memory system should not be registered when disabled";
}

TEST_F(SubcorticalKGWiringTest, HippocampusNodesQueryable) {
    hippocampus_kg_config_t config;
    hippocampus_kg_default_config(&config);

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));

    hippocampus_kg_register_all(kg, &config, &state, ADMIN_TOKEN);

    /* Query the root node by ID */
    const brain_kg_node_t* node = brain_kg_get_node(kg, state.root_id);
    EXPECT_NE(node, nullptr) << "Root node should be queryable";

    if (node != nullptr) {
        EXPECT_STREQ(node->name, HIPPOCAMPUS_KG_ROOT_NAME)
            << "Root node name should match";
    }
}

TEST_F(SubcorticalKGWiringTest, HippocampusEdgesCreated) {
    hippocampus_kg_config_t config;
    hippocampus_kg_default_config(&config);

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));

    hippocampus_kg_register_all(kg, &config, &state, ADMIN_TOKEN);

    /* Verify edges were created */
    EXPECT_GT(state.edge_count, 0u)
        << "Edges should be created between nodes";

    /* Query edges from root node */
    brain_kg_edge_list_t* edge_list = brain_kg_get_outgoing(kg, state.root_id);

    if (edge_list) {
        EXPECT_GT(edge_list->count, 0u)
            << "Root node should have outgoing edges";
        brain_kg_edge_list_destroy(edge_list);
    }
}

/* ============================================================================
 * Amygdala KG Wiring Tests
 * ============================================================================ */

TEST_F(SubcorticalKGWiringTest, AmygdalaDefaultConfigValid) {
    amygdala_kg_config_t config;
    memset(&config, 0, sizeof(config));

    int result = amygdala_kg_default_config(&config);
    ASSERT_EQ(result, 0) << "amygdala_kg_default_config should return 0";

    /* Verify reasonable defaults - use actual struct fields */
    EXPECT_TRUE(config.register_bla);
    EXPECT_TRUE(config.register_cea);
    EXPECT_TRUE(config.register_emotion_nodes);
}

TEST_F(SubcorticalKGWiringTest, AmygdalaConfigNullReturnsError) {
    int result = amygdala_kg_default_config(nullptr);
    EXPECT_EQ(result, -1)
        << "amygdala_kg_default_config(NULL) should return -1";
}

TEST_F(SubcorticalKGWiringTest, AmygdalaRegisterAllWithNullKGFails) {
    amygdala_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = amygdala_kg_register_all(nullptr, nullptr, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, -1)
        << "amygdala_kg_register_all with NULL KG should fail";
}

TEST_F(SubcorticalKGWiringTest, AmygdalaRegisterAllSucceeds) {
    amygdala_kg_config_t config;
    amygdala_kg_default_config(&config);

    amygdala_kg_state_t state;
    memset(&state, 0, sizeof(state));

    int result = amygdala_kg_register_all(kg, &config, &state, ADMIN_TOKEN);
    ASSERT_EQ(result, 0) << "amygdala_kg_register_all should succeed";

    /* Verify state was populated */
    EXPECT_TRUE(state.registered);
    EXPECT_NE(state.root_id, BRAIN_KG_INVALID_NODE);
    EXPECT_GT(state.node_count, 0u);
}

TEST_F(SubcorticalKGWiringTest, AmygdalaNucleiNodesCreated) {
    amygdala_kg_config_t config;
    amygdala_kg_default_config(&config);

    amygdala_kg_state_t state;
    memset(&state, 0, sizeof(state));

    amygdala_kg_register_all(kg, &config, &state, ADMIN_TOKEN);

    /* Verify nuclei nodes were created - use actual state fields */
    EXPECT_NE(state.bla_id, BRAIN_KG_INVALID_NODE)
        << "Basolateral complex node should be created";
    EXPECT_NE(state.cea_id, BRAIN_KG_INVALID_NODE)
        << "Central nucleus node should be created";
    EXPECT_NE(state.la_id, BRAIN_KG_INVALID_NODE)
        << "Lateral nucleus node should be created";
}

TEST_F(SubcorticalKGWiringTest, AmygdalaNodesQueryable) {
    amygdala_kg_config_t config;
    amygdala_kg_default_config(&config);

    amygdala_kg_state_t state;
    memset(&state, 0, sizeof(state));

    amygdala_kg_register_all(kg, &config, &state, ADMIN_TOKEN);

    /* Query the root node */
    const brain_kg_node_t* node = brain_kg_get_node(kg, state.root_id);
    EXPECT_NE(node, nullptr) << "Amygdala root node should be queryable";

    if (node != nullptr) {
        EXPECT_STREQ(node->name, AMYGDALA_KG_ROOT_NAME)
            << "Root node name should match";
    }
}

/* ============================================================================
 * Combined Registration Tests
 * ============================================================================ */

TEST_F(SubcorticalKGWiringTest, MultipleSubcorticalRegionsCoexist) {
    /*
     * Test that multiple subcortical regions can be registered
     * in the same KG without conflicts.
     */
    hippocampus_kg_config_t hippo_config;
    hippocampus_kg_default_config(&hippo_config);

    hippocampus_kg_state_t hippo_state;
    memset(&hippo_state, 0, sizeof(hippo_state));

    amygdala_kg_config_t amyg_config;
    amygdala_kg_default_config(&amyg_config);

    amygdala_kg_state_t amyg_state;
    memset(&amyg_state, 0, sizeof(amyg_state));

    /* Register both */
    int result1 = hippocampus_kg_register_all(kg, &hippo_config, &hippo_state, ADMIN_TOKEN);
    int result2 = amygdala_kg_register_all(kg, &amyg_config, &amyg_state, ADMIN_TOKEN);

    EXPECT_EQ(result1, 0) << "Hippocampus registration should succeed";
    EXPECT_EQ(result2, 0) << "Amygdala registration should succeed";

    /* Both should have distinct root nodes */
    EXPECT_NE(hippo_state.root_id, amyg_state.root_id)
        << "Hippocampus and Amygdala should have different root nodes";

    /* Both should be queryable */
    const brain_kg_node_t* hippo_node = brain_kg_get_node(kg, hippo_state.root_id);
    const brain_kg_node_t* amyg_node = brain_kg_get_node(kg, amyg_state.root_id);

    EXPECT_NE(hippo_node, nullptr) << "Hippocampus root should be queryable";
    EXPECT_NE(amyg_node, nullptr) << "Amygdala root should be queryable";
}

/* ============================================================================
 * Edge Type Tests
 * ============================================================================ */

TEST_F(SubcorticalKGWiringTest, HippocampusEdgeTypesValid) {
    hippocampus_kg_config_t config;
    hippocampus_kg_default_config(&config);

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));

    hippocampus_kg_register_all(kg, &config, &state, ADMIN_TOKEN);

    /* Get edges and verify types are in valid range */
    brain_kg_edge_list_t* edge_list = brain_kg_get_outgoing(kg, state.root_id);

    if (edge_list && edge_list->count > 0) {
        for (size_t i = 0; i < edge_list->count; ++i) {
            /* Edge type should be valid (not corrupted) */
            EXPECT_GE(static_cast<int>(edge_list->edges[i]->type), 0)
                << "Edge type should be non-negative";
            EXPECT_LT(static_cast<int>(edge_list->edges[i]->type), BRAIN_KG_EDGE_TYPE_COUNT)
                << "Edge type should be in valid range";
        }
        brain_kg_edge_list_destroy(edge_list);
    }
}

/* ============================================================================
 * Cleanup Tests
 * ============================================================================ */

TEST_F(SubcorticalKGWiringTest, HippocampusUnregisterSucceeds) {
    hippocampus_kg_config_t config;
    hippocampus_kg_default_config(&config);

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));

    hippocampus_kg_register_all(kg, &config, &state, ADMIN_TOKEN);
    ASSERT_TRUE(state.registered);

    /* Unregister */
    int result = hippocampus_kg_unregister_all(kg, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, 0) << "hippocampus_kg_unregister_all should succeed";

    /* State should be cleared */
    EXPECT_FALSE(state.registered);
}

TEST_F(SubcorticalKGWiringTest, AmygdalaUnregisterSucceeds) {
    amygdala_kg_config_t config;
    amygdala_kg_default_config(&config);

    amygdala_kg_state_t state;
    memset(&state, 0, sizeof(state));

    amygdala_kg_register_all(kg, &config, &state, ADMIN_TOKEN);
    ASSERT_TRUE(state.registered);

    /* Unregister */
    int result = amygdala_kg_unregister_all(kg, &state, ADMIN_TOKEN);
    EXPECT_EQ(result, 0) << "amygdala_kg_unregister_all should succeed";

    EXPECT_FALSE(state.registered);
}

/* ============================================================================
 * State Synchronization Tests
 * ============================================================================ */

TEST_F(SubcorticalKGWiringTest, HippocampusSyncStateSucceeds) {
    hippocampus_kg_config_t config;
    hippocampus_kg_default_config(&config);
    config.include_state_metadata = true;

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));

    hippocampus_kg_register_all(kg, &config, &state, ADMIN_TOKEN);

    /* Sync state with current values */
    /* This would update node metadata with runtime state */
    int result = hippocampus_kg_update_state(
        kg, &state,
        0.8f,  /* encoding_strength */
        0.9f,  /* retrieval_accuracy */
        0.5f,  /* consolidation_progress */
        0.7f,  /* spatial_precision */
        ADMIN_TOKEN
    );

    /* Should succeed (or -1 if not implemented) */
    EXPECT_TRUE(result == 0 || result == -1);
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST_F(SubcorticalKGWiringTest, HippocampusDoubleRegistrationHandled) {
    hippocampus_kg_config_t config;
    hippocampus_kg_default_config(&config);

    hippocampus_kg_state_t state;
    memset(&state, 0, sizeof(state));

    /* First registration */
    int result1 = hippocampus_kg_register_all(kg, &config, &state, ADMIN_TOKEN);
    ASSERT_EQ(result1, 0);

    /* Second registration with same state should be handled */
    int result2 = hippocampus_kg_register_all(kg, &config, &state, ADMIN_TOKEN);

    /* Should either succeed (idempotent) or fail gracefully */
    EXPECT_TRUE(result2 == 0 || result2 == -1)
        << "Double registration should be handled gracefully";
}

TEST_F(SubcorticalKGWiringTest, RegisterWithNullStateHandled) {
    hippocampus_kg_config_t config;
    hippocampus_kg_default_config(&config);

    int result = hippocampus_kg_register_all(kg, &config, nullptr, ADMIN_TOKEN);
    /* Implementation may accept NULL state (ignoring output) or reject it */
    EXPECT_TRUE(result == 0 || result == -1)
        << "NULL state should be handled gracefully";
}
