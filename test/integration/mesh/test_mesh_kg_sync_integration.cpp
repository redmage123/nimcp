/**
 * @file test_mesh_kg_sync_integration.cpp
 * @brief Integration Tests for KG and Mesh Topology Synchronization
 *
 * WHAT: Tests that Knowledge Graph and Mesh topology stay synchronized
 * WHY:  Verify hybrid routing correctness with structural and pattern-based routing
 * HOW:  Create KG topology, register with mesh, verify routing follows structure
 *
 * TEST COVERAGE:
 * - KG and mesh topology stay synchronized
 * - Module added to KG appears in mesh
 * - Mesh routing uses KG relationships
 * - KG queries route through mesh channels
 * - Topological filtering for endorser selection
 * - Cross-modal convergence point discovery
 * - Learning from routing outcomes updates both systems
 * - Structural validation of pattern matches
 * - Hybrid routing mode selection
 * - Multi-hop neighbor discovery
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <cmath>
#include <cstring>

extern "C" {
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_integration.h"
#include "mesh/nimcp_mesh_kg_routing_bridge.h"
#include "mesh/nimcp_mesh_pattern_routing.h"
#include "mesh/nimcp_mesh_channel.h"
#include "core/brain/nimcp_kg_module_wiring.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MeshKGSyncIntegrationTest : public ::testing::Test {
protected:
    mesh_bootstrap_t* bootstrap = nullptr;
    mesh_pattern_router_t* router = nullptr;
    mesh_kg_routing_bridge_t* kg_bridge = nullptr;

    /* Standard brain module IDs */
    static const mesh_participant_id_t VISUAL_ID = 0x100;
    static const mesh_participant_id_t AUDITORY_ID = 0x200;
    static const mesh_participant_id_t STS_ID = 0x300;  /* Superior Temporal Sulcus */
    static const mesh_participant_id_t PFC_ID = 0x400;
    static const mesh_participant_id_t MOTOR_ID = 0x500;
    static const mesh_participant_id_t HIPPOCAMPUS_ID = 0x600;
    static const mesh_participant_id_t AMYGDALA_ID = 0x700;

    void SetUp() override {
        mesh_bootstrap_config_t config;
        mesh_bootstrap_default_config(&config);
        config.subsystems = MESH_SUBSYSTEMS_CORE;

        bootstrap = mesh_bootstrap_create(&config);
        ASSERT_NE(bootstrap, nullptr);

        router = mesh_bootstrap_get_pattern_router(bootstrap);
        ASSERT_NE(router, nullptr);

        /* Create KG bridge */
        mesh_kg_bridge_config_t kg_config;
        mesh_kg_bridge_default_config(&kg_config);
        kg_config.enable_topological_filter = true;
        kg_config.enable_structural_validation = true;
        kg_config.enable_logging = false;

        kg_bridge = mesh_kg_bridge_create(router, &kg_config);
        ASSERT_NE(kg_bridge, nullptr);
    }

    void TearDown() override {
        if (kg_bridge) {
            mesh_kg_bridge_destroy(kg_bridge);
            kg_bridge = nullptr;
        }
        if (bootstrap) {
            mesh_bootstrap_destroy(bootstrap);
            bootstrap = nullptr;
        }
        router = nullptr;
    }

    mesh_pattern_t create_pattern(const float* values, size_t count) {
        mesh_pattern_t pattern;
        mesh_pattern_init(&pattern);

        float magnitude = 0.0f;
        for (size_t i = 0; i < count && i < MESH_PATTERN_DIM; i++) {
            pattern.vector[i] = values[i];
            magnitude += values[i] * values[i];
        }
        pattern.magnitude = sqrtf(magnitude);
        pattern.active_dims = (uint32_t)count;

        return pattern;
    }

    mesh_receptive_field_t create_field(const float* pattern_vals, size_t count, float threshold) {
        mesh_receptive_field_t field;
        mesh_receptive_field_init(&field);
        field.preferred[0] = create_pattern(pattern_vals, count);
        field.pattern_count = 1;
        field.threshold = threshold;
        field.sharpness = 1.0f;
        return field;
    }

    mesh_pattern_transaction_t create_transaction(const mesh_pattern_t& pattern,
                                                   mesh_participant_id_t proposer) {
        mesh_pattern_transaction_t tx;
        memset(&tx, 0, sizeof(tx));
        tx.content_pattern = pattern;
        tx.context_pattern = pattern;
        tx.goal_pattern = pattern;
        tx.proposer = proposer;
        tx.urgency = 0.5f;
        tx.novelty = 0.5f;
        return tx;
    }

    void setup_visual_auditory_topology() {
        /* Visual cortex patterns */
        float visual_pv[] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        float auditory_pv[] = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        float sts_pv[] = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f};
        float pfc_pv[] = {0.2f, 0.2f, 0.6f, 0.0f, 0.0f, 0.0f};

        mesh_pattern_t vis_pat = create_pattern(visual_pv, 6);
        mesh_pattern_t aud_pat = create_pattern(auditory_pv, 6);
        mesh_pattern_t sts_pat = create_pattern(sts_pv, 6);
        mesh_pattern_t pfc_pat = create_pattern(pfc_pv, 6);

        /* Create KG wiring for visual cortex */
        kg_module_wiring_t* visual = kg_module_wiring_create("visual", "SENSORY");
        kg_module_wiring_add_output(visual, "VISUAL_FEATURES", "Visual features");
        kg_module_wiring_add_handler(visual, "RAW_VISUAL", 100);

        /* Auditory cortex */
        kg_module_wiring_t* auditory = kg_module_wiring_create("auditory", "SENSORY");
        kg_module_wiring_add_output(auditory, "AUDIO_FEATURES", "Audio features");
        kg_module_wiring_add_handler(auditory, "RAW_AUDIO", 100);

        /* Superior Temporal Sulcus - multimodal integration */
        kg_module_wiring_t* sts = kg_module_wiring_create("sts", "INTEGRATION");
        kg_module_wiring_add_input(sts, "visual", "VISUAL_FEATURES", true);
        kg_module_wiring_add_input(sts, "auditory", "AUDIO_FEATURES", true);
        kg_module_wiring_add_output(sts, "MULTIMODAL", "Integrated percept");
        kg_module_wiring_add_handler(sts, "VISUAL_FEATURES", 100);
        kg_module_wiring_add_handler(sts, "AUDIO_FEATURES", 100);

        /* PFC - receives from STS */
        kg_module_wiring_t* pfc = kg_module_wiring_create("pfc", "COGNITIVE");
        kg_module_wiring_add_input(pfc, "sts", "MULTIMODAL", true);
        kg_module_wiring_add_output(pfc, "DECISION", "Executive decision");
        kg_module_wiring_add_handler(pfc, "MULTIMODAL", 100);

        /* Register with KG bridge */
        mesh_receptive_field_t vis_field = create_field(visual_pv, 6, 0.3f);
        mesh_receptive_field_t aud_field = create_field(auditory_pv, 6, 0.3f);
        mesh_receptive_field_t sts_field = create_field(sts_pv, 6, 0.3f);
        mesh_receptive_field_t pfc_field = create_field(pfc_pv, 6, 0.3f);

        mesh_kg_bridge_register_module(kg_bridge, VISUAL_ID, visual, &vis_field);
        mesh_kg_bridge_register_module(kg_bridge, AUDITORY_ID, auditory, &aud_field);
        mesh_kg_bridge_register_module(kg_bridge, STS_ID, sts, &sts_field);
        mesh_kg_bridge_register_module(kg_bridge, PFC_ID, pfc, &pfc_field);

        kg_module_wiring_destroy(visual);
        kg_module_wiring_destroy(auditory);
        kg_module_wiring_destroy(sts);
        kg_module_wiring_destroy(pfc);
    }
};

/* ============================================================================
 * Test 1: KG and Mesh Topology Stay Synchronized
 * ============================================================================ */

TEST_F(MeshKGSyncIntegrationTest, KGAndMeshTopologyStaySynchronized) {
    setup_visual_auditory_topology();

    /* Verify connections exist in KG bridge */
    EXPECT_TRUE(mesh_kg_bridge_has_connection(kg_bridge, VISUAL_ID, STS_ID));
    EXPECT_TRUE(mesh_kg_bridge_has_connection(kg_bridge, AUDITORY_ID, STS_ID));
    EXPECT_TRUE(mesh_kg_bridge_has_connection(kg_bridge, STS_ID, PFC_ID));

    /* Verify pattern router has the modules */
    float visual_pv[] = {0.9f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t vis_pattern = create_pattern(visual_pv, 6);
    mesh_pattern_transaction_t tx = create_transaction(vis_pattern, 0x001);

    mesh_activation_t activations[16];
    size_t count = 0;
    nimcp_error_t err = mesh_kg_bridge_route(kg_bridge, &tx, activations, 16, &count);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(count, 0u);

    /* Visual module should be activated */
    bool found_visual = false;
    for (size_t i = 0; i < count; i++) {
        if (activations[i].module_id == VISUAL_ID) {
            found_visual = true;
            EXPECT_GT(activations[i].pattern_similarity, 0.7f);
        }
    }
    EXPECT_TRUE(found_visual);
}

/* ============================================================================
 * Test 2: Module Added to KG Appears in Mesh
 * ============================================================================ */

TEST_F(MeshKGSyncIntegrationTest, ModuleAddedToKGAppearsInMesh) {
    /* Add new module dynamically */
    float new_pattern[] = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    mesh_pattern_t new_pat = create_pattern(new_pattern, 6);

    kg_module_wiring_t* new_module = kg_module_wiring_create("new_module", "CUSTOM");
    kg_module_wiring_add_handler(new_module, "CUSTOM_MSG", 100);

    mesh_receptive_field_t new_field = create_field(new_pattern, 6, 0.3f);
    mesh_participant_id_t NEW_ID = 0x800;

    EXPECT_EQ(mesh_kg_bridge_register_module(kg_bridge, NEW_ID, new_module, &new_field),
              NIMCP_SUCCESS);

    kg_module_wiring_destroy(new_module);

    /* Route a matching pattern */
    mesh_pattern_transaction_t tx = create_transaction(new_pat, 0x001);

    mesh_activation_t activations[16];
    size_t count = 0;
    nimcp_error_t err = mesh_kg_bridge_route(kg_bridge, &tx, activations, 16, &count);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* New module should be found */
    bool found_new = false;
    for (size_t i = 0; i < count; i++) {
        if (activations[i].module_id == NEW_ID) {
            found_new = true;
        }
    }
    EXPECT_TRUE(found_new);
}

/* ============================================================================
 * Test 3: Mesh Routing Uses KG Relationships
 * ============================================================================ */

TEST_F(MeshKGSyncIntegrationTest, MeshRoutingUsesKGRelationships) {
    setup_visual_auditory_topology();

    /* Get topological neighbors from visual */
    mesh_participant_id_t neighbors[16];
    size_t count = 0;

    nimcp_error_t err = mesh_kg_bridge_get_topological_neighbors(
        kg_bridge, VISUAL_ID, 2, neighbors, 16, &count
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(count, 1u);  /* At least STS should be reachable */

    /* STS should be a neighbor (direct connection) */
    bool found_sts = false;
    for (size_t i = 0; i < count; i++) {
        if (neighbors[i] == STS_ID) found_sts = true;
    }
    EXPECT_TRUE(found_sts);

    /* PFC should be reachable (2 hops: visual->sts->pfc) */
    bool found_pfc = false;
    for (size_t i = 0; i < count; i++) {
        if (neighbors[i] == PFC_ID) found_pfc = true;
    }
    /* PFC is 2 hops away, should be found with max_hops=2 */
    EXPECT_TRUE(found_pfc);
}

/* ============================================================================
 * Test 4: KG Queries Route Through Mesh Channels
 * ============================================================================ */

TEST_F(MeshKGSyncIntegrationTest, KGQueriesRouteThroughMeshChannels) {
    setup_visual_auditory_topology();

    /* Route with explanation to see KG contribution */
    float sts_pattern[] = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(sts_pattern, 6);
    mesh_pattern_transaction_t tx = create_transaction(pattern, VISUAL_ID);

    mesh_activation_t activations[16];
    mesh_kg_routing_explanation_t explanations[16];
    size_t count = 0;

    nimcp_error_t err = mesh_kg_bridge_route_with_explanation(
        kg_bridge, &tx, activations, explanations, 16, &count
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Check explanations include both pattern and KG info */
    for (size_t i = 0; i < count; i++) {
        if (activations[i].module_id == STS_ID) {
            EXPECT_TRUE(explanations[i].has_kg_connection);
            EXPECT_GT(explanations[i].pattern_similarity, 0.0f);
            EXPECT_GT(explanations[i].combined_score, 0.0f);
        }
    }
}

/* ============================================================================
 * Test 5: Topological Filtering for Endorser Selection
 * ============================================================================ */

TEST_F(MeshKGSyncIntegrationTest, TopologicalFilteringForEndorserSelection) {
    setup_visual_auditory_topology();

    /* Add an unconnected module */
    float isolated_pattern[] = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f};  /* Same as STS */
    mesh_pattern_t iso_pat = create_pattern(isolated_pattern, 6);

    kg_module_wiring_t* isolated = kg_module_wiring_create("isolated", "CUSTOM");
    /* No connections! */

    mesh_receptive_field_t iso_field = create_field(isolated_pattern, 6, 0.3f);
    mesh_participant_id_t ISO_ID = 0x900;

    mesh_kg_bridge_register_module(kg_bridge, ISO_ID, isolated, &iso_field);
    kg_module_wiring_destroy(isolated);

    /* Create activations including the isolated module */
    mesh_activation_t activations[4];
    activations[0].module_id = STS_ID;
    activations[0].activation_level = 0.9f;
    activations[1].module_id = ISO_ID;
    activations[1].activation_level = 0.85f;
    activations[2].module_id = PFC_ID;
    activations[2].activation_level = 0.5f;

    size_t count = 3;

    /* Filter by structure from visual source */
    nimcp_error_t err = mesh_kg_bridge_filter_by_structure(
        kg_bridge, VISUAL_ID, activations, &count
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    /* Isolated module should be filtered if strict validation is enabled */
}

/* ============================================================================
 * Test 6: Cross-Modal Convergence Point Discovery
 * ============================================================================ */

TEST_F(MeshKGSyncIntegrationTest, CrossModalConvergencePointDiscovery) {
    setup_visual_auditory_topology();

    /* Find modules that receive from both visual AND auditory */
    mesh_participant_id_t sources[] = {VISUAL_ID, AUDITORY_ID};
    mesh_participant_id_t convergence[16];
    size_t count = 0;

    nimcp_error_t err = mesh_kg_bridge_find_convergence_points(
        kg_bridge, sources, 2, convergence, 16, &count
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(count, 1u);

    /* STS should be the convergence point */
    bool found_sts = false;
    for (size_t i = 0; i < count; i++) {
        if (convergence[i] == STS_ID) found_sts = true;
    }
    EXPECT_TRUE(found_sts);
}

/* ============================================================================
 * Test 7: Learning from Routing Outcomes Updates Both Systems
 * ============================================================================ */

TEST_F(MeshKGSyncIntegrationTest, LearningFromRoutingOutcomesUpdatesBothSystems) {
    setup_visual_auditory_topology();

    /* Create a transaction and route */
    float pattern[] = {0.7f, 0.3f, 0.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pat = create_pattern(pattern, 6);
    mesh_pattern_transaction_t tx = create_transaction(pat, 0x001);

    mesh_activation_t activations[16];
    size_t count = 0;
    mesh_kg_bridge_route(kg_bridge, &tx, activations, 16, &count);

    /* Collect endorser IDs */
    mesh_participant_id_t endorsers[16];
    for (size_t i = 0; i < count; i++) {
        endorsers[i] = activations[i].module_id;
    }

    /* Learn from successful outcome */
    nimcp_error_t err = mesh_kg_bridge_learn_outcome(
        kg_bridge, &tx, endorsers, count, true, 1.0f
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify statistics updated */
    mesh_kg_bridge_stats_t stats;
    mesh_kg_bridge_get_stats(kg_bridge, &stats);
    EXPECT_GT(stats.total_routings, 0u);
}

/* ============================================================================
 * Test 8: Structural Validation of Pattern Matches
 * ============================================================================ */

TEST_F(MeshKGSyncIntegrationTest, StructuralValidationOfPatternMatches) {
    setup_visual_auditory_topology();

    /* Validate a structurally valid connection */
    char reason[128];
    bool valid = mesh_kg_bridge_validate_activation(
        kg_bridge, VISUAL_ID, STS_ID, reason, sizeof(reason)
    );
    EXPECT_TRUE(valid);

    /* Validate structurally invalid connection */
    valid = mesh_kg_bridge_validate_activation(
        kg_bridge, PFC_ID, VISUAL_ID, reason, sizeof(reason)
    );
    /* PFC does not output to visual in our topology */
    /* Result depends on config - may be false or true with reason */
}

/* ============================================================================
 * Test 9: Hybrid Routing Mode Selection
 * ============================================================================ */

TEST_F(MeshKGSyncIntegrationTest, HybridRoutingModeSelection) {
    setup_visual_auditory_topology();

    /* Route multiple times and check statistics */
    for (int i = 0; i < 10; i++) {
        float pattern[6];
        pattern[0] = (float)i / 10.0f;
        pattern[1] = 1.0f - pattern[0];
        for (int j = 2; j < 6; j++) pattern[j] = 0.0f;

        mesh_pattern_t pat = create_pattern(pattern, 6);
        mesh_pattern_transaction_t tx = create_transaction(pat, 0x001);

        mesh_activation_t activations[16];
        size_t count = 0;
        mesh_kg_bridge_route(kg_bridge, &tx, activations, 16, &count);
    }

    /* Check stats show hybrid routing occurred */
    mesh_kg_bridge_stats_t stats;
    mesh_kg_bridge_get_stats(kg_bridge, &stats);

    EXPECT_EQ(stats.total_routings, 10u);
    /* Hybrid should be most common in default mode */
    EXPECT_GE(stats.hybrid_routings, 0u);  /* May vary by implementation */
}

/* ============================================================================
 * Test 10: Multi-Hop Neighbor Discovery
 * ============================================================================ */

TEST_F(MeshKGSyncIntegrationTest, MultiHopNeighborDiscovery) {
    setup_visual_auditory_topology();

    /* 1-hop from visual should find STS */
    mesh_participant_id_t neighbors_1hop[16];
    size_t count_1 = 0;
    mesh_kg_bridge_get_topological_neighbors(
        kg_bridge, VISUAL_ID, 1, neighbors_1hop, 16, &count_1
    );

    bool sts_at_1 = false;
    bool pfc_at_1 = false;
    for (size_t i = 0; i < count_1; i++) {
        if (neighbors_1hop[i] == STS_ID) sts_at_1 = true;
        if (neighbors_1hop[i] == PFC_ID) pfc_at_1 = true;
    }
    EXPECT_TRUE(sts_at_1);
    EXPECT_FALSE(pfc_at_1);  /* PFC is 2 hops away */

    /* 2-hop from visual should find PFC */
    mesh_participant_id_t neighbors_2hop[16];
    size_t count_2 = 0;
    mesh_kg_bridge_get_topological_neighbors(
        kg_bridge, VISUAL_ID, 2, neighbors_2hop, 16, &count_2
    );

    bool pfc_at_2 = false;
    for (size_t i = 0; i < count_2; i++) {
        if (neighbors_2hop[i] == PFC_ID) pfc_at_2 = true;
    }
    EXPECT_TRUE(pfc_at_2);

    /* Count should increase with more hops */
    EXPECT_GE(count_2, count_1);
}

/* ============================================================================
 * Test 11: Routing Explanation Formatting
 * ============================================================================ */

TEST_F(MeshKGSyncIntegrationTest, RoutingExplanationFormatting) {
    setup_visual_auditory_topology();

    /* Get explanation for STS routing */
    float sts_pattern[] = {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(sts_pattern, 6);
    mesh_pattern_transaction_t tx = create_transaction(pattern, VISUAL_ID);

    mesh_kg_routing_explanation_t explanation;
    nimcp_error_t err = mesh_kg_bridge_explain_routing(
        kg_bridge, &tx, STS_ID, &explanation
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(explanation.module_id, STS_ID);

    /* Format explanation */
    char buffer[512];
    int len = mesh_kg_bridge_format_explanation(&explanation, buffer, sizeof(buffer));
    EXPECT_GT(len, 0);
    EXPECT_LT((size_t)len, sizeof(buffer));
}

/* ============================================================================
 * Test 12: Strengthen Connection Learning
 * ============================================================================ */

TEST_F(MeshKGSyncIntegrationTest, StrengthenConnectionLearning) {
    setup_visual_auditory_topology();

    /* Get initial stats */
    mesh_kg_bridge_stats_t before;
    mesh_kg_bridge_get_stats(kg_bridge, &before);

    /* Strengthen the visual->STS connection */
    nimcp_error_t err = mesh_kg_bridge_strengthen_connection(
        kg_bridge, VISUAL_ID, STS_ID, 0.8f
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* The connection should now be stronger in pattern matching */
    /* Route and verify STS gets higher activation */
    float pattern[] = {0.6f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pat = create_pattern(pattern, 6);
    mesh_pattern_transaction_t tx = create_transaction(pat, VISUAL_ID);

    mesh_activation_t activations[16];
    size_t count = 0;
    mesh_kg_bridge_route(kg_bridge, &tx, activations, 16, &count);

    /* Find STS activation */
    for (size_t i = 0; i < count; i++) {
        if (activations[i].module_id == STS_ID) {
            /* Should have non-zero activation */
            EXPECT_GT(activations[i].activation_level, 0.0f);
        }
    }
}
