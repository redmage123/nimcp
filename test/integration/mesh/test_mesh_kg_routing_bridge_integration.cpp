/**
 * @file test_mesh_kg_routing_bridge_integration.cpp
 * @brief Integration tests for KG-Mesh Routing Bridge
 *
 * WHAT: Tests KG wiring and pattern router working together
 * WHY:  Verify hybrid routing produces correct results
 * HOW:  Creates realistic brain-like topologies and tests routing behavior
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

class MeshKGBridgeIntegrationTest : public ::testing::Test {
protected:
    mesh_pattern_router_t* router = nullptr;
    mesh_kg_routing_bridge_t* bridge = nullptr;

    /* Module IDs */
    static const mesh_participant_id_t SENSORY_ID = 0x100;
    static const mesh_participant_id_t PFC_ID = 0x200;
    static const mesh_participant_id_t MOTOR_ID = 0x300;
    static const mesh_participant_id_t HIPPOCAMPUS_ID = 0x400;
    static const mesh_participant_id_t AMYGDALA_ID = 0x500;

    void SetUp() override {
        mesh_pattern_router_config_t router_cfg;
        memset(&router_cfg, 0, sizeof(router_cfg));
        router_cfg.default_threshold = 0.3f;
        router_cfg.competition_strength = 0.1f;
        router_cfg.max_endorsers = 16;
        router = mesh_pattern_router_create(&router_cfg);
        ASSERT_NE(router, nullptr);

        mesh_kg_bridge_config_t bridge_cfg;
        mesh_kg_bridge_default_config(&bridge_cfg);
        bridge_cfg.enable_logging = false;
        bridge = mesh_kg_bridge_create(router, &bridge_cfg);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) mesh_kg_bridge_destroy(bridge);
        if (router) mesh_pattern_router_destroy(router);
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

    mesh_receptive_field_t create_field(const mesh_pattern_t& pattern, float threshold) {
        mesh_receptive_field_t field;
        mesh_receptive_field_init(&field);
        field.preferred[0] = pattern;
        field.pattern_count = 1;
        field.threshold = threshold;
        field.sharpness = 1.0f;
        return field;
    }

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

    /* Set up a realistic brain topology */
    void setup_brain_topology() {
        /* Pattern vectors for each brain region */
        float sensory_pv[] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        float pfc_pv[] = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        float motor_pv[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
        float hipp_pv[] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
        float amyg_pv[] = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};

        mesh_pattern_t sensory_pat = create_pattern(sensory_pv, 6);
        mesh_pattern_t pfc_pat = create_pattern(pfc_pv, 6);
        mesh_pattern_t motor_pat = create_pattern(motor_pv, 6);
        mesh_pattern_t hipp_pat = create_pattern(hipp_pv, 6);
        mesh_pattern_t amyg_pat = create_pattern(amyg_pv, 6);

        /* Sensory: no inputs, outputs to PFC and Hippocampus */
        kg_module_wiring_t* sensory = kg_module_wiring_create("sensory", "SENSORY");
        kg_module_wiring_add_output(sensory, "SENSORY_DATA", "Sensory features");
        kg_module_wiring_add_handler(sensory, "RAW_INPUT", 100);

        /* PFC: receives from sensory, hippocampus; outputs to motor */
        kg_module_wiring_t* pfc = kg_module_wiring_create("pfc", "COGNITIVE");
        kg_module_wiring_add_input(pfc, "sensory", "SENSORY_DATA", true);
        kg_module_wiring_add_input(pfc, "hippocampus", "MEMORY_RECALL", false);
        kg_module_wiring_add_output(pfc, "DECISION", "Executive decisions");
        kg_module_wiring_add_handler(pfc, "SENSORY_DATA", 100);
        kg_module_wiring_add_handler(pfc, "MEMORY_RECALL", 80);

        /* Motor: receives from PFC, can be inhibited by amygdala */
        kg_module_wiring_t* motor = kg_module_wiring_create("motor", "MOTOR");
        kg_module_wiring_add_input(motor, "pfc", "DECISION", true);
        kg_module_wiring_add_output(motor, "MOTOR_CMD", "Motor commands");
        kg_module_wiring_add_handler(motor, "DECISION", 100);

        /* Hippocampus: receives from sensory, outputs to PFC */
        kg_module_wiring_t* hipp = kg_module_wiring_create("hippocampus", "MEMORY");
        kg_module_wiring_add_input(hipp, "sensory", "SENSORY_DATA", true);
        kg_module_wiring_add_output(hipp, "MEMORY_RECALL", "Recalled memories");
        kg_module_wiring_add_handler(hipp, "SENSORY_DATA", 90);

        /* Amygdala: receives from sensory, can inhibit motor */
        kg_module_wiring_t* amyg = kg_module_wiring_create("amygdala", "EMOTIONAL");
        kg_module_wiring_add_input(amyg, "sensory", "SENSORY_DATA", true);
        kg_module_wiring_add_output(amyg, "THREAT_SIGNAL", "Threat detection");
        kg_module_wiring_add_handler(amyg, "SENSORY_DATA", 200);  /* High priority */

        /* Register all modules */
        mesh_receptive_field_t sensory_field = create_field(sensory_pat, 0.3f);
        mesh_receptive_field_t pfc_field = create_field(pfc_pat, 0.3f);
        mesh_receptive_field_t motor_field = create_field(motor_pat, 0.3f);
        mesh_receptive_field_t hipp_field = create_field(hipp_pat, 0.3f);
        mesh_receptive_field_t amyg_field = create_field(amyg_pat, 0.3f);

        mesh_kg_bridge_register_module(bridge, SENSORY_ID, sensory, &sensory_field);
        mesh_kg_bridge_register_module(bridge, PFC_ID, pfc, &pfc_field);
        mesh_kg_bridge_register_module(bridge, MOTOR_ID, motor, &motor_field);
        mesh_kg_bridge_register_module(bridge, HIPPOCAMPUS_ID, hipp, &hipp_field);
        mesh_kg_bridge_register_module(bridge, AMYGDALA_ID, amyg, &amyg_field);

        kg_module_wiring_destroy(sensory);
        kg_module_wiring_destroy(pfc);
        kg_module_wiring_destroy(motor);
        kg_module_wiring_destroy(hipp);
        kg_module_wiring_destroy(amyg);
    }
};

/* ============================================================================
 * Topology Integration Tests
 * ============================================================================ */

TEST_F(MeshKGBridgeIntegrationTest, BrainTopologyConnections) {
    setup_brain_topology();

    /* Verify expected connections exist */
    EXPECT_TRUE(mesh_kg_bridge_has_connection(bridge, SENSORY_ID, PFC_ID));
    EXPECT_TRUE(mesh_kg_bridge_has_connection(bridge, SENSORY_ID, HIPPOCAMPUS_ID));
    EXPECT_TRUE(mesh_kg_bridge_has_connection(bridge, SENSORY_ID, AMYGDALA_ID));
    EXPECT_TRUE(mesh_kg_bridge_has_connection(bridge, PFC_ID, MOTOR_ID));
    EXPECT_TRUE(mesh_kg_bridge_has_connection(bridge, HIPPOCAMPUS_ID, PFC_ID));

    /* Verify non-existent connections */
    EXPECT_FALSE(mesh_kg_bridge_has_connection(bridge, MOTOR_ID, PFC_ID));
    EXPECT_FALSE(mesh_kg_bridge_has_connection(bridge, MOTOR_ID, SENSORY_ID));
}

TEST_F(MeshKGBridgeIntegrationTest, TopologicalNeighborsFromSensory) {
    setup_brain_topology();

    /* Get all modules reachable from sensory within 2 hops */
    mesh_participant_id_t neighbors[16];
    size_t count = 0;

    nimcp_error_t err = mesh_kg_bridge_get_topological_neighbors(
        bridge, SENSORY_ID, 2, neighbors, 16, &count
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(count, 3u);  /* At least PFC, Hippocampus, Amygdala directly */

    /* Verify specific neighbors */
    bool found_pfc = false, found_hipp = false, found_amyg = false;
    for (size_t i = 0; i < count; i++) {
        if (neighbors[i] == PFC_ID) found_pfc = true;
        if (neighbors[i] == HIPPOCAMPUS_ID) found_hipp = true;
        if (neighbors[i] == AMYGDALA_ID) found_amyg = true;
    }
    EXPECT_TRUE(found_pfc);
    EXPECT_TRUE(found_hipp);
    EXPECT_TRUE(found_amyg);
}

/* ============================================================================
 * Routing Integration Tests
 * ============================================================================ */

TEST_F(MeshKGBridgeIntegrationTest, RouteFromSensory) {
    setup_brain_topology();

    /* Create a sensory-like pattern */
    float sensory_pattern[] = {0.9f, 0.1f, 0.0f, 0.1f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(sensory_pattern, 6);
    mesh_pattern_transaction_t tx = create_transaction(pattern, SENSORY_ID);

    /* Route */
    mesh_activation_t endorsers[16];
    size_t count = 0;
    nimcp_error_t err = mesh_kg_bridge_route(bridge, &tx, endorsers, 16, &count);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(count, 0u);

    /* Should activate sensory strongly */
    bool found_sensory = false;
    for (size_t i = 0; i < count; i++) {
        if (endorsers[i].module_id == SENSORY_ID) {
            found_sensory = true;
            EXPECT_GT(endorsers[i].pattern_similarity, 0.7f);
        }
    }
    EXPECT_TRUE(found_sensory);
}

TEST_F(MeshKGBridgeIntegrationTest, RouteWithExplanationIntegration) {
    setup_brain_topology();

    float pfc_pattern[] = {0.1f, 0.9f, 0.0f, 0.1f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pfc_pattern, 6);
    mesh_pattern_transaction_t tx = create_transaction(pattern, SENSORY_ID);

    mesh_activation_t endorsers[16];
    mesh_kg_routing_explanation_t explanations[16];
    size_t count = 0;

    nimcp_error_t err = mesh_kg_bridge_route_with_explanation(
        bridge, &tx, endorsers, explanations, 16, &count
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Find PFC explanation */
    for (size_t i = 0; i < count; i++) {
        if (endorsers[i].module_id == PFC_ID) {
            EXPECT_GT(explanations[i].pattern_similarity, 0.7f);
            EXPECT_TRUE(explanations[i].has_kg_connection);
            break;
        }
    }
}

/* ============================================================================
 * Cross-Modal Integration Tests
 * ============================================================================ */

TEST_F(MeshKGBridgeIntegrationTest, MultimodalConvergence) {
    /* Create visual and auditory sensory modules */
    float visual_pv[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float auditory_pv[] = {0.0f, 1.0f, 0.0f, 0.0f};
    float sts_pv[] = {0.5f, 0.5f, 0.0f, 0.0f};

    mesh_pattern_t vis_pat = create_pattern(visual_pv, 4);
    mesh_pattern_t aud_pat = create_pattern(auditory_pv, 4);
    mesh_pattern_t sts_pat = create_pattern(sts_pv, 4);

    /* Visual */
    kg_module_wiring_t* visual = kg_module_wiring_create("visual", "SENSORY");
    kg_module_wiring_add_output(visual, "VISUAL_FEATURES", "Visual features");

    /* Auditory */
    kg_module_wiring_t* auditory = kg_module_wiring_create("auditory", "SENSORY");
    kg_module_wiring_add_output(auditory, "AUDIO_FEATURES", "Audio features");

    /* STS - Superior Temporal Sulcus (multimodal integration) */
    kg_module_wiring_t* sts = kg_module_wiring_create("sts", "INTEGRATION");
    kg_module_wiring_add_input(sts, "visual", "VISUAL_FEATURES", true);
    kg_module_wiring_add_input(sts, "auditory", "AUDIO_FEATURES", true);
    kg_module_wiring_add_output(sts, "INTEGRATED", "Multimodal percept");
    kg_module_wiring_add_handler(sts, "VISUAL_FEATURES", 100);
    kg_module_wiring_add_handler(sts, "AUDIO_FEATURES", 100);

    mesh_receptive_field_t vis_field = create_field(vis_pat, 0.3f);
    mesh_receptive_field_t aud_field = create_field(aud_pat, 0.3f);
    mesh_receptive_field_t sts_field = create_field(sts_pat, 0.3f);

    mesh_kg_bridge_register_module(bridge, 0x100, visual, &vis_field);
    mesh_kg_bridge_register_module(bridge, 0x200, auditory, &aud_field);
    mesh_kg_bridge_register_module(bridge, 0x300, sts, &sts_field);

    /* Find convergence points for visual + auditory */
    mesh_participant_id_t sources[] = {0x100, 0x200};
    mesh_participant_id_t convergence[16];
    size_t count = 0;

    nimcp_error_t err = mesh_kg_bridge_find_convergence_points(
        bridge, sources, 2, convergence, 16, &count
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(convergence[0], 0x300u);  /* STS */

    /* Suggest endorsers for multimodal input */
    mesh_pattern_t patterns[] = {vis_pat, aud_pat};
    mesh_activation_t suggested[16];
    size_t suggested_count = 0;

    err = mesh_kg_bridge_suggest_multimodal_endorsers(
        bridge, patterns, sources, 2, suggested, 16, &suggested_count
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(suggested_count, 0u);
    EXPECT_EQ(suggested[0].module_id, 0x300u);

    kg_module_wiring_destroy(visual);
    kg_module_wiring_destroy(auditory);
    kg_module_wiring_destroy(sts);
}

/* ============================================================================
 * Learning Integration Tests
 * ============================================================================ */

TEST_F(MeshKGBridgeIntegrationTest, LearnFromTopology) {
    setup_brain_topology();

    /* Initialize pattern router from KG topology */
    nimcp_error_t err = mesh_kg_bridge_init_from_topology(bridge);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* After initialization, patterns should be biased toward connected modules */
    /* Test by routing a memory-related pattern */
    float memory_pattern[] = {0.0f, 0.1f, 0.0f, 0.9f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(memory_pattern, 6);
    mesh_pattern_transaction_t tx = create_transaction(pattern, HIPPOCAMPUS_ID);

    mesh_activation_t endorsers[16];
    size_t count = 0;
    err = mesh_kg_bridge_route(bridge, &tx, endorsers, 16, &count);

    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Hippocampus should be strongly activated */
    bool found_hipp = false;
    for (size_t i = 0; i < count; i++) {
        if (endorsers[i].module_id == HIPPOCAMPUS_ID) {
            found_hipp = true;
            break;
        }
    }
    EXPECT_TRUE(found_hipp);
}

TEST_F(MeshKGBridgeIntegrationTest, LearnFromSuccessfulRouting) {
    setup_brain_topology();

    /* Create a pattern and route */
    float pattern_values[] = {0.3f, 0.3f, 0.3f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pattern_values, 6);
    mesh_pattern_transaction_t tx = create_transaction(pattern, SENSORY_ID);

    mesh_activation_t endorsers[16];
    size_t count = 0;
    mesh_kg_bridge_route(bridge, &tx, endorsers, 16, &count);

    if (count > 0) {
        /* Learn from successful outcome */
        mesh_participant_id_t endorser_ids[16];
        for (size_t i = 0; i < count; i++) {
            endorser_ids[i] = endorsers[i].module_id;
        }

        nimcp_error_t err = mesh_kg_bridge_learn_outcome(
            bridge, &tx, endorser_ids, count, true, 1.0f
        );
        EXPECT_EQ(err, NIMCP_SUCCESS);

        /* Verify stats updated */
        mesh_kg_bridge_stats_t stats;
        mesh_kg_bridge_get_stats(bridge, &stats);
        EXPECT_GT(stats.total_routings, 0u);
    }
}

/* ============================================================================
 * Validation Integration Tests
 * ============================================================================ */

TEST_F(MeshKGBridgeIntegrationTest, FilterInvalidActivations) {
    setup_brain_topology();

    /* Create a strict bridge that doesn't allow novel connections */
    mesh_kg_bridge_config_t strict_config;
    mesh_kg_bridge_default_config(&strict_config);
    strict_config.allow_novel_connections = false;

    mesh_kg_routing_bridge_t* strict_bridge = mesh_kg_bridge_create(router, &strict_config);

    /* Register same topology */
    float sensory_pv[] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    float motor_pv[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};

    kg_module_wiring_t* sensory = kg_module_wiring_create("sensory", "SENSORY");
    kg_module_wiring_add_output(sensory, "SENSORY_DATA", "Data");

    kg_module_wiring_t* motor = kg_module_wiring_create("motor", "MOTOR");
    /* Motor does NOT receive from sensory directly */

    mesh_pattern_t sensory_pat = create_pattern(sensory_pv, 6);
    mesh_pattern_t motor_pat = create_pattern(motor_pv, 6);

    mesh_receptive_field_t sensory_field = create_field(sensory_pat, 0.3f);
    mesh_receptive_field_t motor_field = create_field(motor_pat, 0.3f);

    mesh_kg_bridge_register_module(strict_bridge, SENSORY_ID, sensory, &sensory_field);
    mesh_kg_bridge_register_module(strict_bridge, MOTOR_ID, motor, &motor_field);

    /* Create activations that include motor (which shouldn't be connected) */
    mesh_activation_t activations[2];
    activations[0].module_id = SENSORY_ID;
    activations[0].activation_level = 0.9f;
    activations[1].module_id = MOTOR_ID;  /* Not connected to sensory */
    activations[1].activation_level = 0.7f;

    size_t count = 2;
    nimcp_error_t err = mesh_kg_bridge_filter_by_structure(
        strict_bridge, SENSORY_ID, activations, &count
    );

    EXPECT_EQ(err, NIMCP_SUCCESS);
    /* Motor should be filtered out since no connection from sensory */

    mesh_kg_bridge_destroy(strict_bridge);
    kg_module_wiring_destroy(sensory);
    kg_module_wiring_destroy(motor);
}

/* ============================================================================
 * Statistics Integration Tests
 * ============================================================================ */

TEST_F(MeshKGBridgeIntegrationTest, StatisticsTracking) {
    setup_brain_topology();

    /* Do multiple routings */
    for (int i = 0; i < 10; i++) {
        float pv[] = {(float)i / 10.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f};
        mesh_pattern_t pattern = create_pattern(pv, 6);
        mesh_pattern_transaction_t tx = create_transaction(pattern, SENSORY_ID);

        mesh_activation_t endorsers[16];
        size_t count = 0;
        mesh_kg_bridge_route(bridge, &tx, endorsers, 16, &count);
    }

    mesh_kg_bridge_stats_t stats;
    nimcp_error_t err = mesh_kg_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_routings, 10u);
    EXPECT_GT(stats.hybrid_routings, 0u);
}

/* ============================================================================
 * End-to-End Flow Tests
 * ============================================================================ */

TEST_F(MeshKGBridgeIntegrationTest, FullSensoryToMotorFlow) {
    setup_brain_topology();

    /* Simulate sensory input flowing through the brain */

    /* Step 1: Sensory input arrives */
    float sensory_input[] = {0.9f, 0.1f, 0.0f, 0.1f, 0.0f, 0.0f};
    mesh_pattern_t input_pattern = create_pattern(sensory_input, 6);
    mesh_pattern_transaction_t tx1 = create_transaction(input_pattern, 0x001);

    mesh_activation_t stage1[16];
    size_t count1 = 0;
    mesh_kg_bridge_route(bridge, &tx1, stage1, 16, &count1);
    EXPECT_GT(count1, 0u);

    /* Step 2: PFC processes and decides */
    float decision_pattern[] = {0.1f, 0.8f, 0.2f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t decision = create_pattern(decision_pattern, 6);
    mesh_pattern_transaction_t tx2 = create_transaction(decision, PFC_ID);

    mesh_activation_t stage2[16];
    size_t count2 = 0;
    mesh_kg_bridge_route(bridge, &tx2, stage2, 16, &count2);
    EXPECT_GT(count2, 0u);

    /* Step 3: Motor executes */
    float motor_cmd[] = {0.0f, 0.1f, 0.9f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t motor = create_pattern(motor_cmd, 6);
    mesh_pattern_transaction_t tx3 = create_transaction(motor, MOTOR_ID);

    mesh_activation_t stage3[16];
    size_t count3 = 0;
    mesh_kg_bridge_route(bridge, &tx3, stage3, 16, &count3);
    EXPECT_GT(count3, 0u);

    /* Verify motor was activated */
    bool motor_activated = false;
    for (size_t i = 0; i < count3; i++) {
        if (stage3[i].module_id == MOTOR_ID) {
            motor_activated = true;
            EXPECT_GT(stage3[i].pattern_similarity, 0.7f);
        }
    }
    EXPECT_TRUE(motor_activated);
}
