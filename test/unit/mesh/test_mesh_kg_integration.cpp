/**
 * @file test_mesh_kg_integration.cpp
 * @brief Unit tests for KG topology sync with mesh integration
 *
 * Tests KG topology sync with mesh, module discovery through KG + mesh,
 * KG query routing, and relationship sync.
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "mesh/nimcp_mesh_kg_routing_bridge.h"
#include "mesh/nimcp_mesh_pattern_routing.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_receptive_fields.h"
#include "core/brain/nimcp_kg_module_wiring.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MeshKGIntegrationTest : public ::testing::Test {
protected:
    mesh_pattern_router_t* router = nullptr;
    mesh_kg_routing_bridge_t* bridge = nullptr;

    void SetUp() override {
        /* Create pattern router */
        mesh_pattern_router_config_t router_cfg;
        memset(&router_cfg, 0, sizeof(router_cfg));
        router_cfg.default_threshold = 0.3f;
        router_cfg.competition_strength = 0.1f;
        router_cfg.max_endorsers = 16;
        router = mesh_pattern_router_create(&router_cfg);
        ASSERT_NE(router, nullptr);

        /* Create bridge with default config */
        bridge = mesh_kg_bridge_create(router, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) mesh_kg_bridge_destroy(bridge);
        if (router) mesh_pattern_router_destroy(router);
    }

    /* Helper: Create a pattern with specific values */
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

    /* Helper: Create a receptive field */
    mesh_receptive_field_t create_field(const mesh_pattern_t& pattern, float threshold) {
        mesh_receptive_field_t field;
        mesh_receptive_field_init(&field);
        field.preferred[0] = pattern;
        field.pattern_count = 1;
        field.threshold = threshold;
        field.sharpness = 1.0f;
        return field;
    }

    /* Helper: Create a transaction */
    mesh_pattern_transaction_t create_transaction(
        const mesh_pattern_t& pattern,
        mesh_participant_id_t proposer
    ) {
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

    /* Helper: Create KG wiring */
    kg_module_wiring_t* create_wiring(
        const char* name,
        const char* type,
        const char** inputs,
        size_t input_count,
        const char** outputs,
        size_t output_count
    ) {
        kg_module_wiring_t* wiring = kg_module_wiring_create(name, type);
        if (!wiring) return nullptr;

        for (size_t i = 0; i < input_count; i++) {
            kg_module_wiring_add_input(wiring, inputs[i], "DATA", true);
        }

        for (size_t i = 0; i < output_count; i++) {
            kg_module_wiring_add_output(wiring, outputs[i], "Output data");
            kg_module_wiring_add_handler(wiring, outputs[i], 100);
        }

        return wiring;
    }
};

/* ============================================================================
 * KG Topology Sync with Mesh Tests
 * ============================================================================ */

TEST_F(MeshKGIntegrationTest, InitializeFromTopology) {
    /* Create a simple connected topology */
    const char* sensory_outputs[] = {"VISUAL_DATA"};
    kg_module_wiring_t* sensory = create_wiring("sensory", "SENSORY",
        nullptr, 0, sensory_outputs, 1);
    ASSERT_NE(sensory, nullptr);

    const char* pfc_inputs[] = {"sensory"};
    const char* pfc_outputs[] = {"DECISION"};
    kg_module_wiring_t* pfc = create_wiring("pfc", "COGNITIVE",
        pfc_inputs, 1, pfc_outputs, 1);
    ASSERT_NE(pfc, nullptr);
    kg_module_wiring_add_handler(pfc, "VISUAL_DATA", 100);

    /* Register both modules */
    EXPECT_EQ(mesh_kg_bridge_register_module(bridge, 0x100, sensory, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(mesh_kg_bridge_register_module(bridge, 0x200, pfc, nullptr), NIMCP_SUCCESS);

    /* Initialize pattern router from topology */
    nimcp_error_t err = mesh_kg_bridge_init_from_topology(bridge);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    kg_module_wiring_destroy(sensory);
    kg_module_wiring_destroy(pfc);
}

TEST_F(MeshKGIntegrationTest, TopologyConnectionsEstablished) {
    /* Create modules with explicit connections */
    const char* a_outputs[] = {"MSG_A"};
    kg_module_wiring_t* a = create_wiring("module_a", "TEST", nullptr, 0, a_outputs, 1);

    const char* b_inputs[] = {"module_a"};
    kg_module_wiring_t* b = create_wiring("module_b", "TEST", b_inputs, 1, nullptr, 0);
    kg_module_wiring_add_handler(b, "MSG_A", 100);

    mesh_kg_bridge_register_module(bridge, 0x100, a, nullptr);
    mesh_kg_bridge_register_module(bridge, 0x200, b, nullptr);

    /* Verify connection A -> B */
    bool connected = mesh_kg_bridge_has_connection(bridge, 0x100, 0x200);
    EXPECT_TRUE(connected);

    /* Verify no reverse connection B -> A */
    bool reverse = mesh_kg_bridge_has_connection(bridge, 0x200, 0x100);
    EXPECT_FALSE(reverse);

    kg_module_wiring_destroy(a);
    kg_module_wiring_destroy(b);
}

TEST_F(MeshKGIntegrationTest, TopologySyncWithPatternFields) {
    float vis_pv[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float cog_pv[] = {0.0f, 1.0f, 0.0f, 0.0f};

    mesh_pattern_t vis_pat = create_pattern(vis_pv, 4);
    mesh_pattern_t cog_pat = create_pattern(cog_pv, 4);

    mesh_receptive_field_t vis_field = create_field(vis_pat, 0.3f);
    mesh_receptive_field_t cog_field = create_field(cog_pat, 0.3f);

    const char* vis_outputs[] = {"VISUAL"};
    kg_module_wiring_t* vis_wiring = create_wiring("visual", "SENSORY",
        nullptr, 0, vis_outputs, 1);

    const char* cog_inputs[] = {"visual"};
    kg_module_wiring_t* cog_wiring = create_wiring("cognitive", "COGNITIVE",
        cog_inputs, 1, nullptr, 0);
    kg_module_wiring_add_handler(cog_wiring, "VISUAL", 100);

    /* Register with both wiring and pattern fields */
    EXPECT_EQ(mesh_kg_bridge_register_module(bridge, 0x100, vis_wiring, &vis_field),
              NIMCP_SUCCESS);
    EXPECT_EQ(mesh_kg_bridge_register_module(bridge, 0x200, cog_wiring, &cog_field),
              NIMCP_SUCCESS);

    kg_module_wiring_destroy(vis_wiring);
    kg_module_wiring_destroy(cog_wiring);
}

/* ============================================================================
 * Module Discovery Through KG + Mesh Tests
 * ============================================================================ */

TEST_F(MeshKGIntegrationTest, DiscoverModulesViaTopologicalNeighbors) {
    /* Create chain: A -> B -> C */
    const char* a_outputs[] = {"MSG_A"};
    kg_module_wiring_t* a = create_wiring("A", "TEST", nullptr, 0, a_outputs, 1);

    const char* b_inputs[] = {"A"};
    const char* b_outputs[] = {"MSG_B"};
    kg_module_wiring_t* b = create_wiring("B", "TEST", b_inputs, 1, b_outputs, 1);
    kg_module_wiring_add_handler(b, "MSG_A", 100);

    const char* c_inputs[] = {"B"};
    kg_module_wiring_t* c = create_wiring("C", "TEST", c_inputs, 1, nullptr, 0);
    kg_module_wiring_add_handler(c, "MSG_B", 100);

    mesh_kg_bridge_register_module(bridge, 0x100, a, nullptr);
    mesh_kg_bridge_register_module(bridge, 0x200, b, nullptr);
    mesh_kg_bridge_register_module(bridge, 0x300, c, nullptr);

    /* Discover neighbors of A within 1 hop */
    mesh_participant_id_t neighbors[16];
    size_t count = 0;
    nimcp_error_t err = mesh_kg_bridge_get_topological_neighbors(
        bridge, 0x100, 1, neighbors, 16, &count
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(neighbors[0], 0x200u);

    kg_module_wiring_destroy(a);
    kg_module_wiring_destroy(b);
    kg_module_wiring_destroy(c);
}

TEST_F(MeshKGIntegrationTest, DiscoverModulesWithinMultipleHops) {
    /* Create chain: A -> B -> C */
    const char* a_outputs[] = {"MSG_A"};
    kg_module_wiring_t* a = create_wiring("A", "TEST", nullptr, 0, a_outputs, 1);

    const char* b_inputs[] = {"A"};
    const char* b_outputs[] = {"MSG_B"};
    kg_module_wiring_t* b = create_wiring("B", "TEST", b_inputs, 1, b_outputs, 1);
    kg_module_wiring_add_handler(b, "MSG_A", 100);

    const char* c_inputs[] = {"B"};
    kg_module_wiring_t* c = create_wiring("C", "TEST", c_inputs, 1, nullptr, 0);
    kg_module_wiring_add_handler(c, "MSG_B", 100);

    mesh_kg_bridge_register_module(bridge, 0x100, a, nullptr);
    mesh_kg_bridge_register_module(bridge, 0x200, b, nullptr);
    mesh_kg_bridge_register_module(bridge, 0x300, c, nullptr);

    /* Discover neighbors of A within 2 hops */
    mesh_participant_id_t neighbors[16];
    size_t count = 0;
    nimcp_error_t err = mesh_kg_bridge_get_topological_neighbors(
        bridge, 0x100, 2, neighbors, 16, &count
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(count, 2u);

    kg_module_wiring_destroy(a);
    kg_module_wiring_destroy(b);
    kg_module_wiring_destroy(c);
}

TEST_F(MeshKGIntegrationTest, DiscoverConvergencePoints) {
    /* Create visual, auditory, and integration module */
    const char* vis_outputs[] = {"VISUAL"};
    kg_module_wiring_t* vis = create_wiring("visual", "SENSORY",
        nullptr, 0, vis_outputs, 1);

    const char* aud_outputs[] = {"AUDIO"};
    kg_module_wiring_t* aud = create_wiring("auditory", "SENSORY",
        nullptr, 0, aud_outputs, 1);

    const char* int_inputs[] = {"visual", "auditory"};
    kg_module_wiring_t* integration = create_wiring("integration", "MULTIMODAL",
        int_inputs, 2, nullptr, 0);
    kg_module_wiring_add_handler(integration, "VISUAL", 100);
    kg_module_wiring_add_handler(integration, "AUDIO", 100);

    mesh_kg_bridge_register_module(bridge, 0x100, vis, nullptr);
    mesh_kg_bridge_register_module(bridge, 0x200, aud, nullptr);
    mesh_kg_bridge_register_module(bridge, 0x300, integration, nullptr);

    /* Find convergence point */
    mesh_participant_id_t sources[] = {0x100, 0x200};
    mesh_participant_id_t convergence[16];
    size_t count = 0;

    nimcp_error_t err = mesh_kg_bridge_find_convergence_points(
        bridge, sources, 2, convergence, 16, &count
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(convergence[0], 0x300u);

    kg_module_wiring_destroy(vis);
    kg_module_wiring_destroy(aud);
    kg_module_wiring_destroy(integration);
}

/* ============================================================================
 * KG Query Routing Tests
 * ============================================================================ */

TEST_F(MeshKGIntegrationTest, RouteByPatternUsingKGFilter) {
    /* Create modules with both pattern fields and KG wiring */
    float pv1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float pv2[] = {0.0f, 1.0f, 0.0f, 0.0f};
    float pv3[] = {0.0f, 0.0f, 1.0f, 0.0f};

    mesh_pattern_t p1 = create_pattern(pv1, 4);
    mesh_pattern_t p2 = create_pattern(pv2, 4);
    mesh_pattern_t p3 = create_pattern(pv3, 4);

    mesh_receptive_field_t f1 = create_field(p1, 0.3f);
    mesh_receptive_field_t f2 = create_field(p2, 0.3f);
    mesh_receptive_field_t f3 = create_field(p3, 0.3f);

    mesh_kg_bridge_register_module(bridge, 0x100, nullptr, &f1);
    mesh_kg_bridge_register_module(bridge, 0x200, nullptr, &f2);
    mesh_kg_bridge_register_module(bridge, 0x300, nullptr, &f3);

    /* Route transaction matching first pattern */
    mesh_pattern_transaction_t tx = create_transaction(p1, 0x001);

    mesh_activation_t endorsers[16];
    size_t count = 0;
    nimcp_error_t err = mesh_kg_bridge_route(bridge, &tx, endorsers, 16, &count);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(count, 0u);

    /* First module should be most activated */
    bool found_first = false;
    for (size_t i = 0; i < count; i++) {
        if (endorsers[i].module_id == 0x100) {
            found_first = true;
            break;
        }
    }
    EXPECT_TRUE(found_first);
}

TEST_F(MeshKGIntegrationTest, RouteWithExplanation) {
    float pv[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);
    mesh_receptive_field_t field = create_field(pattern, 0.3f);

    const char* outputs[] = {"RESULT"};
    kg_module_wiring_t* wiring = create_wiring("test_module", "TEST",
        nullptr, 0, outputs, 1);

    mesh_kg_bridge_register_module(bridge, 0x100, wiring, &field);

    mesh_pattern_transaction_t tx = create_transaction(pattern, 0x001);

    mesh_activation_t endorsers[16];
    mesh_kg_routing_explanation_t explanations[16];
    size_t count = 0;

    nimcp_error_t err = mesh_kg_bridge_route_with_explanation(
        bridge, &tx, endorsers, explanations, 16, &count
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(count, 0u);

    /* Check explanation */
    for (size_t i = 0; i < count; i++) {
        EXPECT_EQ(explanations[i].module_id, endorsers[i].module_id);
        EXPECT_GE(explanations[i].pattern_similarity, 0.0f);
        EXPECT_LE(explanations[i].pattern_similarity, 1.0f);
    }

    kg_module_wiring_destroy(wiring);
}

TEST_F(MeshKGIntegrationTest, ExplainRouting) {
    float pv[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);
    mesh_receptive_field_t field = create_field(pattern, 0.3f);

    const char* outputs[] = {"RESULT"};
    kg_module_wiring_t* wiring = create_wiring("test_module", "TEST",
        nullptr, 0, outputs, 1);

    mesh_kg_bridge_register_module(bridge, 0x100, wiring, &field);

    mesh_pattern_transaction_t tx = create_transaction(pattern, 0x001);
    mesh_kg_routing_explanation_t explanation;

    nimcp_error_t err = mesh_kg_bridge_explain_routing(
        bridge, &tx, 0x100, &explanation
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(explanation.module_id, 0x100u);
    EXPECT_GT(explanation.pattern_similarity, 0.9f);

    kg_module_wiring_destroy(wiring);
}

/* ============================================================================
 * Relationship Sync Tests
 * ============================================================================ */

TEST_F(MeshKGIntegrationTest, ValidateActivationAgainstStructure) {
    const char* a_outputs[] = {"MSG"};
    kg_module_wiring_t* a = create_wiring("A", "TEST", nullptr, 0, a_outputs, 1);

    const char* b_inputs[] = {"A"};
    kg_module_wiring_t* b = create_wiring("B", "TEST", b_inputs, 1, nullptr, 0);
    kg_module_wiring_add_handler(b, "MSG", 100);

    mesh_kg_bridge_register_module(bridge, 0x100, a, nullptr);
    mesh_kg_bridge_register_module(bridge, 0x200, b, nullptr);

    /* A -> B should be valid */
    char reason[128];
    bool valid = mesh_kg_bridge_validate_activation(
        bridge, 0x100, 0x200, reason, sizeof(reason)
    );
    EXPECT_TRUE(valid);

    kg_module_wiring_destroy(a);
    kg_module_wiring_destroy(b);
}

TEST_F(MeshKGIntegrationTest, FilterActivationsByStructure) {
    const char* a_outputs[] = {"MSG"};
    kg_module_wiring_t* a = create_wiring("A", "TEST", nullptr, 0, a_outputs, 1);

    const char* b_inputs[] = {"A"};
    kg_module_wiring_t* b = create_wiring("B", "TEST", b_inputs, 1, nullptr, 0);
    kg_module_wiring_add_handler(b, "MSG", 100);

    /* C has no connection to A */
    kg_module_wiring_t* c = create_wiring("C", "TEST", nullptr, 0, nullptr, 0);

    /* Create strict bridge that doesn't allow novel connections */
    mesh_kg_bridge_config_t config;
    mesh_kg_bridge_default_config(&config);
    config.allow_novel_connections = false;

    mesh_kg_routing_bridge_t* strict_bridge = mesh_kg_bridge_create(router, &config);
    mesh_kg_bridge_register_module(strict_bridge, 0x100, a, nullptr);
    mesh_kg_bridge_register_module(strict_bridge, 0x200, b, nullptr);
    mesh_kg_bridge_register_module(strict_bridge, 0x300, c, nullptr);

    /* Create activations for both B and C */
    mesh_activation_t activations[2];
    activations[0].module_id = 0x200;
    activations[0].activation_level = 0.8f;
    activations[1].module_id = 0x300;
    activations[1].activation_level = 0.7f;

    size_t count = 2;
    nimcp_error_t err = mesh_kg_bridge_filter_by_structure(
        strict_bridge, 0x100, activations, &count
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(activations[0].module_id, 0x200u);

    mesh_kg_bridge_destroy(strict_bridge);
    kg_module_wiring_destroy(a);
    kg_module_wiring_destroy(b);
    kg_module_wiring_destroy(c);
}

TEST_F(MeshKGIntegrationTest, StrengthenConnection) {
    float pv1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float pv2[] = {0.0f, 1.0f, 0.0f, 0.0f};
    mesh_pattern_t p1 = create_pattern(pv1, 4);
    mesh_pattern_t p2 = create_pattern(pv2, 4);

    mesh_receptive_field_t f1 = create_field(p1, 0.3f);
    mesh_receptive_field_t f2 = create_field(p2, 0.3f);

    mesh_kg_bridge_register_module(bridge, 0x100, nullptr, &f1);
    mesh_kg_bridge_register_module(bridge, 0x200, nullptr, &f2);

    nimcp_error_t err = mesh_kg_bridge_strengthen_connection(bridge, 0x100, 0x200, 0.5f);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshKGIntegrationTest, LearnFromRoutingOutcome) {
    float pv[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);
    mesh_receptive_field_t field = create_field(pattern, 0.3f);

    mesh_kg_bridge_register_module(bridge, 0x100, nullptr, &field);

    mesh_pattern_transaction_t tx = create_transaction(pattern, 0x001);
    mesh_participant_id_t endorsers[] = {0x100};

    nimcp_error_t err = mesh_kg_bridge_learn_outcome(
        bridge, &tx, endorsers, 1, true, 1.0f
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MeshKGIntegrationTest, GetStats) {
    mesh_kg_bridge_stats_t stats;
    nimcp_error_t err = mesh_kg_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_routings, 0u);
}

TEST_F(MeshKGIntegrationTest, ResetStats) {
    /* Do some routing first */
    float pv[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);
    mesh_receptive_field_t field = create_field(pattern, 0.3f);
    mesh_kg_bridge_register_module(bridge, 0x100, nullptr, &field);

    mesh_pattern_transaction_t tx = create_transaction(pattern, 0x001);
    mesh_activation_t endorsers[16];
    size_t count = 0;
    mesh_kg_bridge_route(bridge, &tx, endorsers, 16, &count);

    /* Verify stats changed */
    mesh_kg_bridge_stats_t stats;
    mesh_kg_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.total_routings, 0u);

    /* Reset */
    EXPECT_EQ(mesh_kg_bridge_reset_stats(bridge), NIMCP_SUCCESS);

    mesh_kg_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_routings, 0u);
}

TEST_F(MeshKGIntegrationTest, StatsTrackCacheHits) {
    mesh_kg_bridge_stats_t stats;
    mesh_kg_bridge_get_stats(bridge, &stats);

    /* Cache stats should be tracked */
    EXPECT_EQ(stats.topology_cache_hits, 0u);
    EXPECT_EQ(stats.topology_cache_misses, 0u);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(MeshKGIntegrationTest, RouteWithNoModules) {
    float pv[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);
    mesh_pattern_transaction_t tx = create_transaction(pattern, 0x001);

    mesh_activation_t endorsers[16];
    size_t count = 0;
    nimcp_error_t err = mesh_kg_bridge_route(bridge, &tx, endorsers, 16, &count);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(count, 0u);
}

TEST_F(MeshKGIntegrationTest, NullParameters) {
    EXPECT_EQ(mesh_kg_bridge_register_module(nullptr, 0x100, nullptr, nullptr),
              NIMCP_ERROR_INVALID_PARAM);

    EXPECT_EQ(mesh_kg_bridge_route(nullptr, nullptr, nullptr, 0, nullptr),
              NIMCP_ERROR_INVALID_PARAM);

    EXPECT_EQ(mesh_kg_bridge_get_topological_neighbors(
        nullptr, 0, 0, nullptr, 0, nullptr),
        NIMCP_ERROR_INVALID_PARAM);
}
