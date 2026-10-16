/**
 * @file test_mesh_kg_routing_bridge.cpp
 * @brief Unit tests for KG-Mesh Routing Bridge
 *
 * WHAT: Tests individual functions of the KG-Mesh bridge
 * WHY:  Verify correct behavior of hybrid routing components
 * HOW:  Isolated tests for each API function
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
#include "core/brain/nimcp_kg_module_wiring.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MeshKGRoutingBridgeTest : public ::testing::Test {
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
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(MeshKGRoutingBridgeTest, CreateDestroy) {
    /* Bridge was created in SetUp, verify it's valid */
    EXPECT_NE(bridge, nullptr);

    /* Create another with custom config */
    mesh_kg_bridge_config_t config;
    mesh_kg_bridge_default_config(&config);
    config.mode = MESH_KG_ROUTE_PATTERN_ONLY;
    config.enable_logging = true;

    mesh_kg_routing_bridge_t* bridge2 = mesh_kg_bridge_create(router, &config);
    EXPECT_NE(bridge2, nullptr);

    mesh_kg_bridge_destroy(bridge2);
}

TEST_F(MeshKGRoutingBridgeTest, CreateWithNullRouter) {
    mesh_kg_routing_bridge_t* bad = mesh_kg_bridge_create(nullptr, nullptr);
    EXPECT_EQ(bad, nullptr);
}

TEST_F(MeshKGRoutingBridgeTest, DefaultConfig) {
    mesh_kg_bridge_config_t config;
    mesh_kg_bridge_default_config(&config);

    EXPECT_EQ(config.mode, MESH_KG_ROUTE_HYBRID);
    EXPECT_FLOAT_EQ(config.structural_weight, MESH_KG_DEFAULT_STRUCTURAL_WEIGHT);
    EXPECT_TRUE(config.enable_topological_filter);
    EXPECT_TRUE(config.enable_structural_validation);
    EXPECT_TRUE(config.allow_novel_connections);
    EXPECT_TRUE(config.learn_from_routing);
    EXPECT_EQ(config.max_hops, 2u);
}

/* ============================================================================
 * Module Registration Tests
 * ============================================================================ */

TEST_F(MeshKGRoutingBridgeTest, RegisterModuleWithBoth) {
    float pv[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);
    mesh_receptive_field_t field = create_field(pattern, 0.3f);

    const char* inputs[] = {"sensory"};
    const char* outputs[] = {"DECISION"};
    kg_module_wiring_t* wiring = create_wiring("pfc", "COGNITIVE", inputs, 1, outputs, 1);
    ASSERT_NE(wiring, nullptr);

    nimcp_error_t err = mesh_kg_bridge_register_module(
        bridge, 0x100, wiring, &field
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);

    kg_module_wiring_destroy(wiring);
}

TEST_F(MeshKGRoutingBridgeTest, RegisterModuleWithWiringOnly) {
    const char* inputs[] = {"input1"};
    const char* outputs[] = {"OUTPUT1"};
    kg_module_wiring_t* wiring = create_wiring("module1", "TEST", inputs, 1, outputs, 1);
    ASSERT_NE(wiring, nullptr);

    nimcp_error_t err = mesh_kg_bridge_register_module(
        bridge, 0x200, wiring, nullptr
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);

    kg_module_wiring_destroy(wiring);
}

TEST_F(MeshKGRoutingBridgeTest, RegisterModuleWithFieldOnly) {
    float pv[] = {0.0f, 1.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);
    mesh_receptive_field_t field = create_field(pattern, 0.3f);

    nimcp_error_t err = mesh_kg_bridge_register_module(
        bridge, 0x300, nullptr, &field
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshKGRoutingBridgeTest, RegisterModuleUpdate) {
    float pv1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern1 = create_pattern(pv1, 4);
    mesh_receptive_field_t field1 = create_field(pattern1, 0.3f);

    EXPECT_EQ(mesh_kg_bridge_register_module(bridge, 0x100, nullptr, &field1),
              NIMCP_SUCCESS);

    /* Update with different field */
    float pv2[] = {0.0f, 1.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern2 = create_pattern(pv2, 4);
    mesh_receptive_field_t field2 = create_field(pattern2, 0.5f);

    EXPECT_EQ(mesh_kg_bridge_register_module(bridge, 0x100, nullptr, &field2),
              NIMCP_SUCCESS);
}

/* ============================================================================
 * Routing Tests
 * ============================================================================ */

TEST_F(MeshKGRoutingBridgeTest, BasicRoute) {
    /* Register some modules */
    float pv1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float pv2[] = {0.0f, 1.0f, 0.0f, 0.0f};
    mesh_pattern_t p1 = create_pattern(pv1, 4);
    mesh_pattern_t p2 = create_pattern(pv2, 4);

    mesh_receptive_field_t f1 = create_field(p1, 0.3f);
    mesh_receptive_field_t f2 = create_field(p2, 0.3f);

    mesh_kg_bridge_register_module(bridge, 0x100, nullptr, &f1);
    mesh_kg_bridge_register_module(bridge, 0x200, nullptr, &f2);

    /* Create transaction matching first module */
    mesh_pattern_transaction_t tx = create_transaction(p1, 0x001);

    /* Route */
    mesh_activation_t endorsers[16];
    size_t count = 0;
    nimcp_error_t err = mesh_kg_bridge_route(bridge, &tx, endorsers, 16, &count);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(count, 0u);

    /* First module should be activated */
    bool found_first = false;
    for (size_t i = 0; i < count; i++) {
        if (endorsers[i].module_id == 0x100) {
            found_first = true;
            EXPECT_GT(endorsers[i].pattern_similarity, 0.8f);
            break;
        }
    }
    EXPECT_TRUE(found_first);
}

TEST_F(MeshKGRoutingBridgeTest, RouteWithExplanation) {
    float pv[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);
    mesh_receptive_field_t field = create_field(pattern, 0.3f);

    mesh_kg_bridge_register_module(bridge, 0x100, nullptr, &field);

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
}

/* ============================================================================
 * Topological Filtering Tests
 * ============================================================================ */

TEST_F(MeshKGRoutingBridgeTest, HasConnection) {
    /* Create connected modules via KG wiring */
    const char* pfc_inputs[] = {"sensory"};
    const char* pfc_outputs[] = {"DECISION"};
    kg_module_wiring_t* pfc_wiring = create_wiring("pfc", "COGNITIVE",
        pfc_inputs, 1, pfc_outputs, 1);

    const char* motor_inputs[] = {"pfc"};
    const char* motor_outputs[] = {"MOTOR_CMD"};
    kg_module_wiring_t* motor_wiring = create_wiring("motor", "MOTOR",
        motor_inputs, 1, motor_outputs, 1);

    /* Motor declares it receives from PFC */
    kg_module_wiring_add_handler(motor_wiring, "DECISION", 100);

    mesh_kg_bridge_register_module(bridge, 0x100, pfc_wiring, nullptr);
    mesh_kg_bridge_register_module(bridge, 0x200, motor_wiring, nullptr);

    /* PFC -> Motor should be connected (PFC outputs DECISION, Motor handles it) */
    bool connected = mesh_kg_bridge_has_connection(bridge, 0x100, 0x200);
    EXPECT_TRUE(connected);

    /* Motor -> PFC should NOT be connected */
    bool reverse = mesh_kg_bridge_has_connection(bridge, 0x200, 0x100);
    EXPECT_FALSE(reverse);

    kg_module_wiring_destroy(pfc_wiring);
    kg_module_wiring_destroy(motor_wiring);
}

TEST_F(MeshKGRoutingBridgeTest, GetTopologicalNeighbors) {
    /* Create a chain: A -> B -> C */
    const char* no_inputs[] = {};
    const char* a_outputs[] = {"MSG_A"};
    kg_module_wiring_t* a_wiring = create_wiring("A", "TEST", no_inputs, 0, a_outputs, 1);

    const char* b_inputs[] = {"A"};
    const char* b_outputs[] = {"MSG_B"};
    kg_module_wiring_t* b_wiring = create_wiring("B", "TEST", b_inputs, 1, b_outputs, 1);
    kg_module_wiring_add_handler(b_wiring, "MSG_A", 100);

    const char* c_inputs[] = {"B"};
    const char* c_outputs[] = {"MSG_C"};
    kg_module_wiring_t* c_wiring = create_wiring("C", "TEST", c_inputs, 1, c_outputs, 1);
    kg_module_wiring_add_handler(c_wiring, "MSG_B", 100);

    mesh_kg_bridge_register_module(bridge, 0x100, a_wiring, nullptr);
    mesh_kg_bridge_register_module(bridge, 0x200, b_wiring, nullptr);
    mesh_kg_bridge_register_module(bridge, 0x300, c_wiring, nullptr);

    /* Get neighbors of A within 1 hop */
    mesh_participant_id_t neighbors[16];
    size_t count = 0;
    nimcp_error_t err = mesh_kg_bridge_get_topological_neighbors(
        bridge, 0x100, 1, neighbors, 16, &count
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(count, 1u);  /* Only B */
    EXPECT_EQ(neighbors[0], 0x200u);

    /* Get neighbors of A within 2 hops */
    count = 0;
    err = mesh_kg_bridge_get_topological_neighbors(
        bridge, 0x100, 2, neighbors, 16, &count
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(count, 2u);  /* B and C */

    kg_module_wiring_destroy(a_wiring);
    kg_module_wiring_destroy(b_wiring);
    kg_module_wiring_destroy(c_wiring);
}

/* ============================================================================
 * Cross-Modal Discovery Tests
 * ============================================================================ */

TEST_F(MeshKGRoutingBridgeTest, FindConvergencePoints) {
    /* Create visual, auditory, and STS (receives from both) */
    const char* no_inputs[] = {};
    const char* visual_outputs[] = {"VISUAL_DATA"};
    kg_module_wiring_t* visual = create_wiring("visual", "SENSORY",
        no_inputs, 0, visual_outputs, 1);

    const char* auditory_outputs[] = {"AUDITORY_DATA"};
    kg_module_wiring_t* auditory = create_wiring("auditory", "SENSORY",
        no_inputs, 0, auditory_outputs, 1);

    const char* sts_inputs[] = {"visual", "auditory"};
    const char* sts_outputs[] = {"INTEGRATED"};
    kg_module_wiring_t* sts = create_wiring("sts", "INTEGRATION",
        sts_inputs, 2, sts_outputs, 1);
    kg_module_wiring_add_handler(sts, "VISUAL_DATA", 100);
    kg_module_wiring_add_handler(sts, "AUDITORY_DATA", 100);

    mesh_kg_bridge_register_module(bridge, 0x100, visual, nullptr);
    mesh_kg_bridge_register_module(bridge, 0x200, auditory, nullptr);
    mesh_kg_bridge_register_module(bridge, 0x300, sts, nullptr);

    /* Find modules that receive from both visual and auditory */
    mesh_participant_id_t sources[] = {0x100, 0x200};
    mesh_participant_id_t convergence[16];
    size_t count = 0;

    nimcp_error_t err = mesh_kg_bridge_find_convergence_points(
        bridge, sources, 2, convergence, 16, &count
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(convergence[0], 0x300u);  /* STS */

    kg_module_wiring_destroy(visual);
    kg_module_wiring_destroy(auditory);
    kg_module_wiring_destroy(sts);
}

TEST_F(MeshKGRoutingBridgeTest, SuggestMultimodalEndorsers) {
    /* Create visual, auditory, and STS with pattern fields */
    float vis_pv[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float aud_pv[] = {0.0f, 1.0f, 0.0f, 0.0f};
    float sts_pv[] = {0.5f, 0.5f, 0.0f, 0.0f};  /* Blended */

    mesh_pattern_t vis_pat = create_pattern(vis_pv, 4);
    mesh_pattern_t aud_pat = create_pattern(aud_pv, 4);
    mesh_pattern_t sts_pat = create_pattern(sts_pv, 4);

    mesh_receptive_field_t vis_field = create_field(vis_pat, 0.3f);
    mesh_receptive_field_t aud_field = create_field(aud_pat, 0.3f);
    mesh_receptive_field_t sts_field = create_field(sts_pat, 0.3f);

    const char* no_inputs[] = {};
    const char* visual_outputs[] = {"VISUAL_DATA"};
    kg_module_wiring_t* visual = create_wiring("visual", "SENSORY",
        no_inputs, 0, visual_outputs, 1);

    const char* auditory_outputs[] = {"AUDITORY_DATA"};
    kg_module_wiring_t* auditory = create_wiring("auditory", "SENSORY",
        no_inputs, 0, auditory_outputs, 1);

    const char* sts_inputs[] = {"visual", "auditory"};
    const char* sts_outputs[] = {"INTEGRATED"};
    kg_module_wiring_t* sts = create_wiring("sts", "INTEGRATION",
        sts_inputs, 2, sts_outputs, 1);
    kg_module_wiring_add_handler(sts, "VISUAL_DATA", 100);
    kg_module_wiring_add_handler(sts, "AUDITORY_DATA", 100);

    mesh_kg_bridge_register_module(bridge, 0x100, visual, &vis_field);
    mesh_kg_bridge_register_module(bridge, 0x200, auditory, &aud_field);
    mesh_kg_bridge_register_module(bridge, 0x300, sts, &sts_field);

    /* Suggest endorsers for multimodal input */
    mesh_pattern_t patterns[] = {vis_pat, aud_pat};
    mesh_participant_id_t sources[] = {0x100, 0x200};
    mesh_activation_t suggested[16];
    size_t count = 0;

    nimcp_error_t err = mesh_kg_bridge_suggest_multimodal_endorsers(
        bridge, patterns, sources, 2, suggested, 16, &count
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(suggested[0].module_id, 0x300u);

    kg_module_wiring_destroy(visual);
    kg_module_wiring_destroy(auditory);
    kg_module_wiring_destroy(sts);
}

/* ============================================================================
 * Validation Tests
 * ============================================================================ */

TEST_F(MeshKGRoutingBridgeTest, ValidateActivation) {
    /* Create connected modules */
    const char* a_outputs[] = {"MSG"};
    kg_module_wiring_t* a_wiring = create_wiring("A", "TEST", nullptr, 0, a_outputs, 1);

    const char* b_inputs[] = {"A"};
    kg_module_wiring_t* b_wiring = create_wiring("B", "TEST", b_inputs, 1, nullptr, 0);
    kg_module_wiring_add_handler(b_wiring, "MSG", 100);

    mesh_kg_bridge_register_module(bridge, 0x100, a_wiring, nullptr);
    mesh_kg_bridge_register_module(bridge, 0x200, b_wiring, nullptr);

    /* A -> B should be valid */
    char reason[128];
    bool valid = mesh_kg_bridge_validate_activation(
        bridge, 0x100, 0x200, reason, sizeof(reason)
    );
    EXPECT_TRUE(valid);

    kg_module_wiring_destroy(a_wiring);
    kg_module_wiring_destroy(b_wiring);
}

TEST_F(MeshKGRoutingBridgeTest, FilterByStructure) {
    /* Create one connected and one unconnected module */
    const char* a_outputs[] = {"MSG"};
    kg_module_wiring_t* a_wiring = create_wiring("A", "TEST", nullptr, 0, a_outputs, 1);

    const char* b_inputs[] = {"A"};
    kg_module_wiring_t* b_wiring = create_wiring("B", "TEST", b_inputs, 1, nullptr, 0);
    kg_module_wiring_add_handler(b_wiring, "MSG", 100);

    /* C has no connection to A */
    kg_module_wiring_t* c_wiring = create_wiring("C", "TEST", nullptr, 0, nullptr, 0);

    mesh_kg_bridge_register_module(bridge, 0x100, a_wiring, nullptr);
    mesh_kg_bridge_register_module(bridge, 0x200, b_wiring, nullptr);
    mesh_kg_bridge_register_module(bridge, 0x300, c_wiring, nullptr);

    /* Disable novel connections to test filtering */
    mesh_kg_bridge_config_t config;
    mesh_kg_bridge_default_config(&config);
    config.allow_novel_connections = false;

    mesh_kg_routing_bridge_t* strict_bridge = mesh_kg_bridge_create(router, &config);
    mesh_kg_bridge_register_module(strict_bridge, 0x100, a_wiring, nullptr);
    mesh_kg_bridge_register_module(strict_bridge, 0x200, b_wiring, nullptr);
    mesh_kg_bridge_register_module(strict_bridge, 0x300, c_wiring, nullptr);

    /* Create activations for both B and C */
    mesh_activation_t activations[2];
    activations[0].module_id = 0x200;  /* B - connected */
    activations[0].activation_level = 0.8f;
    activations[1].module_id = 0x300;  /* C - NOT connected */
    activations[1].activation_level = 0.7f;

    size_t count = 2;
    nimcp_error_t err = mesh_kg_bridge_filter_by_structure(
        strict_bridge, 0x100, activations, &count
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(count, 1u);  /* Only B remains */
    EXPECT_EQ(activations[0].module_id, 0x200u);

    mesh_kg_bridge_destroy(strict_bridge);
    kg_module_wiring_destroy(a_wiring);
    kg_module_wiring_destroy(b_wiring);
    kg_module_wiring_destroy(c_wiring);
}

/* ============================================================================
 * Learning Tests
 * ============================================================================ */

TEST_F(MeshKGRoutingBridgeTest, LearnOutcome) {
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

TEST_F(MeshKGRoutingBridgeTest, StrengthenConnection) {
    float pv1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float pv2[] = {0.0f, 1.0f, 0.0f, 0.0f};
    mesh_pattern_t p1 = create_pattern(pv1, 4);
    mesh_pattern_t p2 = create_pattern(pv2, 4);

    mesh_receptive_field_t f1 = create_field(p1, 0.3f);
    mesh_receptive_field_t f2 = create_field(p2, 0.3f);

    mesh_kg_bridge_register_module(bridge, 0x100, nullptr, &f1);
    mesh_kg_bridge_register_module(bridge, 0x200, nullptr, &f2);

    nimcp_error_t err = mesh_kg_bridge_strengthen_connection(
        bridge, 0x100, 0x200, 0.5f
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MeshKGRoutingBridgeTest, GetStats) {
    mesh_kg_bridge_stats_t stats;
    nimcp_error_t err = mesh_kg_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_routings, 0u);
}

TEST_F(MeshKGRoutingBridgeTest, ResetStats) {
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

/* ============================================================================
 * Introspection Tests
 * ============================================================================ */

TEST_F(MeshKGRoutingBridgeTest, ExplainRouting) {
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
    EXPECT_GT(explanation.pattern_similarity, 0.9f);  /* Same pattern */
    EXPECT_TRUE(explanation.handles_message_type);

    kg_module_wiring_destroy(wiring);
}

TEST_F(MeshKGRoutingBridgeTest, FormatExplanation) {
    mesh_kg_routing_explanation_t explanation;
    memset(&explanation, 0, sizeof(explanation));
    explanation.module_id = 0x100;
    explanation.pattern_similarity = 0.85f;
    explanation.activation_level = 0.75f;
    explanation.selected_by_pattern = true;
    explanation.has_kg_connection = true;
    explanation.handles_message_type = true;
    explanation.kg_handler_priority = 100;
    strcpy(explanation.connection_path, "sensory -> pfc");
    explanation.combined_score = 0.80f;
    explanation.validated = true;

    char buffer[512];
    int len = mesh_kg_bridge_format_explanation(&explanation, buffer, sizeof(buffer));

    EXPECT_GT(len, 0);
    EXPECT_NE(strstr(buffer, "0x100"), nullptr);
    EXPECT_NE(strstr(buffer, "0.850"), nullptr);
    EXPECT_NE(strstr(buffer, "sensory -> pfc"), nullptr);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(MeshKGRoutingBridgeTest, NullParameters) {
    EXPECT_EQ(mesh_kg_bridge_register_module(nullptr, 0x100, nullptr, nullptr),
              NIMCP_ERROR_INVALID_PARAM);

    EXPECT_EQ(mesh_kg_bridge_route(nullptr, nullptr, nullptr, 0, nullptr),
              NIMCP_ERROR_INVALID_PARAM);

    EXPECT_EQ(mesh_kg_bridge_get_topological_neighbors(
        nullptr, 0, 0, nullptr, 0, nullptr),
        NIMCP_ERROR_INVALID_PARAM);

    EXPECT_EQ(mesh_kg_bridge_find_convergence_points(
        nullptr, nullptr, 0, nullptr, 0, nullptr),
        NIMCP_ERROR_INVALID_PARAM);
}

TEST_F(MeshKGRoutingBridgeTest, EmptyRoute) {
    /* Route with no registered modules */
    float pv[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);
    mesh_pattern_transaction_t tx = create_transaction(pattern, 0x001);

    mesh_activation_t endorsers[16];
    size_t count = 0;
    nimcp_error_t err = mesh_kg_bridge_route(bridge, &tx, endorsers, 16, &count);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(count, 0u);
}
