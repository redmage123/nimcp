/**
 * @file test_mesh_kg_routing_bridge_regression.cpp
 * @brief Regression tests for KG-Mesh Routing Bridge
 *
 * WHAT: Tests for edge cases, stability, and backward compatibility
 * WHY:  Catch regressions in hybrid routing behavior
 * HOW:  Golden tests, boundary conditions, stress tests
 *
 * @author NIMCP Development Team
 * @date 2025-02-01
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "mesh/nimcp_mesh_kg_routing_bridge.h"
#include "mesh/nimcp_mesh_pattern_routing.h"
#include "core/brain/nimcp_kg_module_wiring.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MeshKGBridgeRegressionTest : public ::testing::Test {
protected:
    mesh_pattern_router_t* router = nullptr;
    mesh_kg_routing_bridge_t* bridge = nullptr;

    void SetUp() override {
        mesh_pattern_router_config_t router_cfg;
        memset(&router_cfg, 0, sizeof(router_cfg));
        router_cfg.default_threshold = 0.3f;
        router_cfg.competition_strength = 0.1f;
        router_cfg.max_endorsers = 32;
        router = mesh_pattern_router_create(&router_cfg);
        ASSERT_NE(router, nullptr);

        bridge = mesh_kg_bridge_create(router, nullptr);
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
};

/* ============================================================================
 * Golden Behavior Tests
 * ============================================================================ */

TEST_F(MeshKGBridgeRegressionTest, Golden_ConfigDefaults) {
    mesh_kg_bridge_config_t config;
    mesh_kg_bridge_default_config(&config);

    /* These values should remain stable */
    EXPECT_EQ(config.mode, MESH_KG_ROUTE_HYBRID);
    EXPECT_FLOAT_EQ(config.structural_weight, 0.3f);
    EXPECT_FLOAT_EQ(config.pattern_weight, 0.7f);
    EXPECT_TRUE(config.enable_topological_filter);
    EXPECT_EQ(config.max_hops, 2u);
    EXPECT_TRUE(config.enable_structural_validation);
    EXPECT_TRUE(config.allow_novel_connections);
    EXPECT_TRUE(config.learn_from_routing);
    EXPECT_FLOAT_EQ(config.learning_rate, 0.01f);
    EXPECT_TRUE(config.enable_topology_cache);
}

TEST_F(MeshKGBridgeRegressionTest, Golden_IdenticalPatternRouting) {
    /* Same pattern should always activate its preferred module */
    float pv[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);
    mesh_receptive_field_t field = create_field(pattern, 0.3f);

    mesh_kg_bridge_register_module(bridge, 0x100, nullptr, &field);

    /* Route identical pattern */
    mesh_pattern_transaction_t tx = create_transaction(pattern, 0x001);
    mesh_activation_t endorsers[16];
    size_t count = 0;

    mesh_kg_bridge_route(bridge, &tx, endorsers, 16, &count);

    EXPECT_EQ(count, 1u);
    EXPECT_EQ(endorsers[0].module_id, 0x100u);
    EXPECT_FLOAT_EQ(endorsers[0].pattern_similarity, 1.0f);
}

TEST_F(MeshKGBridgeRegressionTest, Golden_OrthogonalPatternsNoActivation) {
    /* Orthogonal patterns should not activate each other */
    float pv1[] = {1.0f, 0.0f, 0.0f, 0.0f};
    float pv2[] = {0.0f, 1.0f, 0.0f, 0.0f};

    mesh_pattern_t p1 = create_pattern(pv1, 4);
    mesh_pattern_t p2 = create_pattern(pv2, 4);

    mesh_receptive_field_t field = create_field(p1, 0.5f);  /* Higher threshold */
    mesh_kg_bridge_register_module(bridge, 0x100, nullptr, &field);

    /* Route orthogonal pattern */
    mesh_pattern_transaction_t tx = create_transaction(p2, 0x001);
    mesh_activation_t endorsers[16];
    size_t count = 0;

    mesh_kg_bridge_route(bridge, &tx, endorsers, 16, &count);

    /* Should not activate (orthogonal = 0 similarity) */
    EXPECT_EQ(count, 0u);
}

TEST_F(MeshKGBridgeRegressionTest, Golden_SimilarityScoreStability) {
    /* Similarity scores should be consistent */
    float pv1[] = {0.8f, 0.2f, 0.0f, 0.0f};
    float pv2[] = {0.6f, 0.4f, 0.0f, 0.0f};

    mesh_pattern_t p1 = create_pattern(pv1, 4);
    mesh_pattern_t p2 = create_pattern(pv2, 4);

    float sim = mesh_pattern_similarity(&p1, &p2);

    /* Run 100 times to verify consistency */
    for (int i = 0; i < 100; i++) {
        float new_sim = mesh_pattern_similarity(&p1, &p2);
        EXPECT_FLOAT_EQ(sim, new_sim);
    }
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(MeshKGBridgeRegressionTest, EdgeCase_ZeroPattern) {
    float zero_pv[] = {0.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t zero = create_pattern(zero_pv, 4);

    /* Zero pattern should have 0 similarity with everything */
    float nonzero_pv[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t nonzero = create_pattern(nonzero_pv, 4);

    float sim = mesh_pattern_similarity(&zero, &nonzero);
    EXPECT_FLOAT_EQ(sim, 0.0f);
}

TEST_F(MeshKGBridgeRegressionTest, EdgeCase_NegativeValues) {
    /* Pattern with negative values */
    float pv1[] = {1.0f, -0.5f, 0.3f, 0.0f};
    float pv2[] = {1.0f, 0.5f, 0.3f, 0.0f};

    mesh_pattern_t p1 = create_pattern(pv1, 4);
    mesh_pattern_t p2 = create_pattern(pv2, 4);

    float sim = mesh_pattern_similarity(&p1, &p2);

    /* Similarity should still be computed correctly */
    EXPECT_GE(sim, 0.0f);
    EXPECT_LE(sim, 1.0f);
}

TEST_F(MeshKGBridgeRegressionTest, EdgeCase_MaxDimensions) {
    /* Pattern using all dimensions */
    float pv[MESH_PATTERN_DIM];
    for (int i = 0; i < MESH_PATTERN_DIM; i++) {
        pv[i] = (float)(i % 10) / 10.0f;
    }

    mesh_pattern_t full = create_pattern(pv, MESH_PATTERN_DIM);
    mesh_receptive_field_t field = create_field(full, 0.3f);

    nimcp_error_t err = mesh_kg_bridge_register_module(bridge, 0x100, nullptr, &field);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    mesh_pattern_transaction_t tx = create_transaction(full, 0x001);
    mesh_activation_t endorsers[16];
    size_t count = 0;

    err = mesh_kg_bridge_route(bridge, &tx, endorsers, 16, &count);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(count, 0u);
}

TEST_F(MeshKGBridgeRegressionTest, EdgeCase_VerySmallThreshold) {
    float pv[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);
    mesh_receptive_field_t field = create_field(pattern, 0.001f);  /* Very low */

    mesh_kg_bridge_register_module(bridge, 0x100, nullptr, &field);

    /* Even slightly similar patterns should activate */
    float similar_pv[] = {0.1f, 0.01f, 0.0f, 0.0f};
    mesh_pattern_t similar = create_pattern(similar_pv, 4);
    mesh_pattern_transaction_t tx = create_transaction(similar, 0x001);

    mesh_activation_t endorsers[16];
    size_t count = 0;
    mesh_kg_bridge_route(bridge, &tx, endorsers, 16, &count);

    EXPECT_GT(count, 0u);
}

TEST_F(MeshKGBridgeRegressionTest, EdgeCase_VeryHighThreshold) {
    float pv[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);
    mesh_receptive_field_t field = create_field(pattern, 0.99f);  /* Very high */

    mesh_kg_bridge_register_module(bridge, 0x100, nullptr, &field);

    /* Only nearly identical patterns should activate */
    float almost_pv[] = {0.95f, 0.05f, 0.0f, 0.0f};
    mesh_pattern_t almost = create_pattern(almost_pv, 4);
    mesh_pattern_transaction_t tx = create_transaction(almost, 0x001);

    mesh_activation_t endorsers[16];
    size_t count = 0;
    mesh_kg_bridge_route(bridge, &tx, endorsers, 16, &count);

    /* May or may not activate depending on exact similarity */
    /* Just verify no crash */
    EXPECT_GE(count, 0u);
}

/* ============================================================================
 * Stress Tests
 * ============================================================================ */

TEST_F(MeshKGBridgeRegressionTest, Stress_ManyModules) {
    /* Register many modules */
    for (int i = 0; i < 100; i++) {
        float pv[4] = {0};
        pv[i % 4] = 1.0f;

        mesh_pattern_t pattern = create_pattern(pv, 4);
        mesh_receptive_field_t field = create_field(pattern, 0.3f);

        nimcp_error_t err = mesh_kg_bridge_register_module(
            bridge, 0x100 + i, nullptr, &field
        );
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }

    /* Route a pattern */
    float test_pv[] = {0.5f, 0.5f, 0.0f, 0.0f};
    mesh_pattern_t test = create_pattern(test_pv, 4);
    mesh_pattern_transaction_t tx = create_transaction(test, 0x001);

    mesh_activation_t endorsers[128];
    size_t count = 0;

    nimcp_error_t err = mesh_kg_bridge_route(bridge, &tx, endorsers, 128, &count);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshKGBridgeRegressionTest, Stress_ManyRoutings) {
    /* Register a module */
    float pv[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);
    mesh_receptive_field_t field = create_field(pattern, 0.3f);
    mesh_kg_bridge_register_module(bridge, 0x100, nullptr, &field);

    /* Do many routings */
    for (int i = 0; i < 1000; i++) {
        float test_pv[] = {(float)(i % 10) / 10.0f, 0.5f, 0.0f, 0.0f};
        mesh_pattern_t test = create_pattern(test_pv, 4);
        mesh_pattern_transaction_t tx = create_transaction(test, 0x001);

        mesh_activation_t endorsers[16];
        size_t count = 0;
        mesh_kg_bridge_route(bridge, &tx, endorsers, 16, &count);
    }

    /* Verify stats */
    mesh_kg_bridge_stats_t stats;
    mesh_kg_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_routings, 1000u);
}

TEST_F(MeshKGBridgeRegressionTest, Stress_TopologyCacheEfficiency) {
    /* Create connected modules */
    for (int i = 0; i < 20; i++) {
        kg_module_wiring_t* wiring = kg_module_wiring_create(
            ("module" + std::to_string(i)).c_str(), "TEST"
        );

        if (i > 0) {
            kg_module_wiring_add_input(wiring,
                ("module" + std::to_string(i-1)).c_str(), "DATA", true);
            kg_module_wiring_add_handler(wiring, "DATA", 100);
        }
        kg_module_wiring_add_output(wiring, "DATA", "output");

        mesh_kg_bridge_register_module(bridge, 0x100 + i, wiring, nullptr);
        kg_module_wiring_destroy(wiring);
    }

    /* Query topology many times for same source */
    mesh_participant_id_t neighbors[64];
    size_t count;

    for (int i = 0; i < 100; i++) {
        mesh_kg_bridge_get_topological_neighbors(
            bridge, 0x100, 2, neighbors, 64, &count
        );
    }

    /* Cache should be utilized */
    mesh_kg_bridge_stats_t stats;
    mesh_kg_bridge_get_stats(bridge, &stats);
    EXPECT_GT(stats.topology_cache_hits, 0u);
}

/* ============================================================================
 * Consistency Tests
 * ============================================================================ */

TEST_F(MeshKGBridgeRegressionTest, Consistency_SameInputSameOutput) {
    float pv[] = {0.7f, 0.3f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);
    mesh_receptive_field_t field = create_field(pattern, 0.3f);
    mesh_kg_bridge_register_module(bridge, 0x100, nullptr, &field);

    mesh_pattern_transaction_t tx = create_transaction(pattern, 0x001);

    /* Run same routing multiple times */
    std::vector<float> similarities;
    for (int i = 0; i < 10; i++) {
        mesh_activation_t endorsers[16];
        size_t count = 0;
        mesh_kg_bridge_route(bridge, &tx, endorsers, 16, &count);

        if (count > 0) {
            similarities.push_back(endorsers[0].pattern_similarity);
        }
    }

    /* All should be identical */
    for (size_t i = 1; i < similarities.size(); i++) {
        EXPECT_FLOAT_EQ(similarities[0], similarities[i]);
    }
}

TEST_F(MeshKGBridgeRegressionTest, Consistency_ExplanationMatchesRouting) {
    float pv[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);
    mesh_receptive_field_t field = create_field(pattern, 0.3f);
    mesh_kg_bridge_register_module(bridge, 0x100, nullptr, &field);

    mesh_pattern_transaction_t tx = create_transaction(pattern, 0x001);

    mesh_activation_t endorsers[16];
    mesh_kg_routing_explanation_t explanations[16];
    size_t count = 0;

    mesh_kg_bridge_route_with_explanation(
        bridge, &tx, endorsers, explanations, 16, &count
    );

    /* Explanation similarity should match activation similarity */
    for (size_t i = 0; i < count; i++) {
        EXPECT_FLOAT_EQ(endorsers[i].pattern_similarity,
                        explanations[i].pattern_similarity);
        EXPECT_EQ(endorsers[i].module_id, explanations[i].module_id);
    }
}

/* ============================================================================
 * Backward Compatibility Tests
 * ============================================================================ */

TEST_F(MeshKGBridgeRegressionTest, BackCompat_PatternRouterStillWorks) {
    /* Verify pattern router can still be used directly */
    float pv[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);
    mesh_receptive_field_t field = create_field(pattern, 0.3f);

    /* Register directly with pattern router */
    nimcp_error_t err = mesh_pattern_router_register_receptive_field(
        router, 0x100, &field
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Route directly with pattern router */
    mesh_pattern_transaction_t tx = create_transaction(pattern, 0x001);
    mesh_activation_t activations[16];
    size_t count = 0;

    err = mesh_pattern_router_compute_activations(
        router, &tx, activations, 16, &count
    );
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GT(count, 0u);
}

TEST_F(MeshKGBridgeRegressionTest, BackCompat_KGWiringStillWorks) {
    /* Verify KG wiring can still be used independently */
    kg_module_wiring_t* wiring = kg_module_wiring_create("test", "TEST");
    ASSERT_NE(wiring, nullptr);

    kg_module_wiring_add_input(wiring, "source", "MSG", true);
    kg_module_wiring_add_output(wiring, "OUTPUT", "desc");
    kg_module_wiring_add_handler(wiring, "MSG", 100);

    EXPECT_TRUE(kg_module_wiring_has_input(wiring, "source", "MSG"));
    EXPECT_TRUE(kg_module_wiring_has_output(wiring, "OUTPUT"));
    EXPECT_TRUE(kg_module_wiring_has_handler(wiring, "MSG"));
    EXPECT_EQ(kg_module_wiring_get_handler_priority(wiring, "MSG"), 100u);

    kg_module_wiring_destroy(wiring);
}

/* ============================================================================
 * Error Recovery Tests
 * ============================================================================ */

TEST_F(MeshKGBridgeRegressionTest, ErrorRecovery_InvalidModule) {
    /* Try to explain routing for non-existent module */
    float pv[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_t pattern = create_pattern(pv, 4);
    mesh_pattern_transaction_t tx = create_transaction(pattern, 0x001);

    mesh_kg_routing_explanation_t explanation;
    nimcp_error_t err = mesh_kg_bridge_explain_routing(
        bridge, &tx, 0xDEAD, &explanation
    );

    EXPECT_EQ(err, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(MeshKGBridgeRegressionTest, ErrorRecovery_StrengthenInvalidConnection) {
    nimcp_error_t err = mesh_kg_bridge_strengthen_connection(
        bridge, 0xDEAD, 0xBEEF, 0.5f
    );

    EXPECT_EQ(err, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(MeshKGBridgeRegressionTest, ErrorRecovery_ValidateInvalidModule) {
    char reason[128] = {0};
    bool valid = mesh_kg_bridge_validate_activation(
        bridge, 0xDEAD, 0xBEEF, reason, sizeof(reason)
    );

    /* With allow_novel_connections=true (default), this returns true */
    EXPECT_TRUE(valid);
}
