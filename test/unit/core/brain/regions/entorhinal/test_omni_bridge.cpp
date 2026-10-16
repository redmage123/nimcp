/**
 * @file test_omni_bridge.cpp
 * @brief Unit tests for Entorhinal-Omnidirectional System Bridge
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 */

#include <gtest/gtest.h>
#include <cmath>

extern "C" {
#include "core/brain/regions/entorhinal/nimcp_entorhinal_omni_bridge.h"
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class OmniBridgeTest : public ::testing::Test {
protected:
    entorhinal_omni_bridge_state_t* bridge = nullptr;
    nimcp_entorhinal_t* ec = nullptr;

    void SetUp() override {
        entorhinal_omni_config_t config = entorhinal_omni_default_config();
        bridge = entorhinal_omni_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);

        entorhinal_config_t ec_config = entorhinal_default_config();
        ec = entorhinal_create(&ec_config);
        ASSERT_NE(ec, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            entorhinal_omni_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (ec) {
            entorhinal_destroy(ec);
            ec = nullptr;
        }
    }
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(OmniBridgeTest, CreateWithDefaultConfig) {
    entorhinal_omni_bridge_state_t* b = entorhinal_omni_bridge_create(nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(b->config.enable_threat_integration);
    EXPECT_TRUE(b->config.enable_opportunity_integration);
    EXPECT_TRUE(b->config.enable_object_tracking);
    entorhinal_omni_bridge_destroy(b);
}

TEST_F(OmniBridgeTest, CreateWithCustomConfig) {
    entorhinal_omni_config_t config = entorhinal_omni_default_config();
    config.enable_3d_mapping = true;
    config.max_tracking_distance = 100.0f;

    entorhinal_omni_bridge_state_t* b = entorhinal_omni_bridge_create(&config);
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(b->config.enable_3d_mapping);
    EXPECT_FLOAT_EQ(b->config.max_tracking_distance, 100.0f);
    entorhinal_omni_bridge_destroy(b);
}

TEST_F(OmniBridgeTest, DestroyNull) {
    entorhinal_omni_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(OmniBridgeTest, Connect) {
    EXPECT_EQ(entorhinal_omni_bridge_connect(bridge, ec, nullptr), 0);
    EXPECT_TRUE(bridge->connected);
    EXPECT_EQ(bridge->entorhinal, ec);
}

TEST_F(OmniBridgeTest, ConnectNull) {
    EXPECT_EQ(entorhinal_omni_bridge_connect(nullptr, ec, nullptr), -1);
}

TEST_F(OmniBridgeTest, Disconnect) {
    entorhinal_omni_bridge_connect(bridge, ec, nullptr);
    EXPECT_EQ(entorhinal_omni_bridge_disconnect(bridge), 0);
    EXPECT_FALSE(bridge->connected);
}

TEST_F(OmniBridgeTest, DisconnectNull) {
    EXPECT_EQ(entorhinal_omni_bridge_disconnect(nullptr), -1);
}

TEST_F(OmniBridgeTest, Reset) {
    entorhinal_omni_bridge_connect(bridge, ec, nullptr);
    bridge->updates_processed = 100;
    bridge->num_threats = 5;

    EXPECT_EQ(entorhinal_omni_bridge_reset(bridge), 0);
    EXPECT_EQ(bridge->updates_processed, 0u);
    EXPECT_EQ(bridge->num_threats, 0u);
}

TEST_F(OmniBridgeTest, ResetNull) {
    EXPECT_EQ(entorhinal_omni_bridge_reset(nullptr), -1);
}

/*=============================================================================
 * UPDATE TESTS
 *===========================================================================*/

TEST_F(OmniBridgeTest, UpdateBasic) {
    entorhinal_omni_bridge_connect(bridge, ec, nullptr);
    EXPECT_EQ(entorhinal_omni_bridge_update(bridge, 0.01f), 0);
    EXPECT_EQ(bridge->updates_processed, 1u);
}

TEST_F(OmniBridgeTest, UpdateNull) {
    EXPECT_EQ(entorhinal_omni_bridge_update(nullptr, 0.01f), -1);
}

TEST_F(OmniBridgeTest, UpdateMultiple) {
    entorhinal_omni_bridge_connect(bridge, ec, nullptr);
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(entorhinal_omni_bridge_update(bridge, 0.01f), 0);
    }
    EXPECT_EQ(bridge->updates_processed, 100u);
}

/*=============================================================================
 * SPATIAL MAP TESTS
 *===========================================================================*/

TEST_F(OmniBridgeTest, ReceiveSpatialMap) {
    omni_spatial_map_t map = {0};
    map.azimuth_salience[0] = 1.0f;
    map.azimuth_salience[90] = 0.5f;

    EXPECT_EQ(entorhinal_omni_receive_spatial_map(bridge, &map), 0);
    EXPECT_FLOAT_EQ(bridge->spatial_map.azimuth_salience[0], 1.0f);
    EXPECT_FLOAT_EQ(bridge->spatial_map.azimuth_salience[90], 0.5f);
}

TEST_F(OmniBridgeTest, ReceiveSpatialMapNull) {
    omni_spatial_map_t map = {0};
    EXPECT_EQ(entorhinal_omni_receive_spatial_map(nullptr, &map), -1);
    EXPECT_EQ(entorhinal_omni_receive_spatial_map(bridge, nullptr), -1);
}

/*=============================================================================
 * THREAT RECEPTION TESTS
 *===========================================================================*/

TEST_F(OmniBridgeTest, ReceiveThreats) {
    omni_threat_vector_t threats[2];
    threats[0].direction = 0.0f;
    threats[0].distance = 5.0f;
    threats[0].magnitude = 0.8f;
    threats[1].direction = M_PI;
    threats[1].distance = 10.0f;
    threats[1].magnitude = 0.5f;

    EXPECT_EQ(entorhinal_omni_receive_threats(bridge, threats, 2), 0);
    EXPECT_EQ(bridge->num_threats, 2u);
    EXPECT_GT(bridge->threats_detected, 0u);
}

TEST_F(OmniBridgeTest, ReceiveThreatsNull) {
    EXPECT_EQ(entorhinal_omni_receive_threats(nullptr, nullptr, 0), -1);
}

TEST_F(OmniBridgeTest, ReceiveThreatsZero) {
    EXPECT_EQ(entorhinal_omni_receive_threats(bridge, nullptr, 0), 0);
    EXPECT_EQ(bridge->num_threats, 0u);
}

/*=============================================================================
 * OPPORTUNITY RECEPTION TESTS
 *===========================================================================*/

TEST_F(OmniBridgeTest, ReceiveOpportunities) {
    omni_opportunity_vector_t opportunities[2];
    opportunities[0].direction = M_PI / 4.0f;
    opportunities[0].distance = 3.0f;
    opportunities[0].value = 0.9f;
    opportunities[0].accessibility = 0.8f;
    opportunities[1].direction = -M_PI / 2.0f;
    opportunities[1].distance = 8.0f;
    opportunities[1].value = 0.6f;
    opportunities[1].accessibility = 0.5f;

    EXPECT_EQ(entorhinal_omni_receive_opportunities(bridge, opportunities, 2), 0);
    EXPECT_EQ(bridge->num_opportunities, 2u);
    EXPECT_GT(bridge->opportunities_detected, 0u);
}

TEST_F(OmniBridgeTest, ReceiveOpportunitiesNull) {
    EXPECT_EQ(entorhinal_omni_receive_opportunities(nullptr, nullptr, 0), -1);
}

/*=============================================================================
 * SPATIAL QUERY TESTS
 *===========================================================================*/

TEST_F(OmniBridgeTest, GetSalienceAtDirection) {
    bridge->spatial_map.azimuth_salience[45] = 0.75f;
    float salience = entorhinal_omni_get_salience_at_direction(bridge, M_PI / 4.0f);
    EXPECT_FLOAT_EQ(salience, 0.75f);
}

TEST_F(OmniBridgeTest, GetSalienceAtDirectionNull) {
    EXPECT_FLOAT_EQ(entorhinal_omni_get_salience_at_direction(nullptr, 0.0f), 0.0f);
}

TEST_F(OmniBridgeTest, GetThreatAtDirection) {
    // Add threat at direction 0
    bridge->spatial_map.polar_threat[0][5] = 0.9f;
    float threat = entorhinal_omni_get_threat_at_direction(bridge, 0.0f);
    EXPECT_FLOAT_EQ(threat, 0.9f);
}

TEST_F(OmniBridgeTest, GetOpportunityAtDirection) {
    bridge->spatial_map.polar_opportunity[90][10] = 0.7f;
    float opportunity = entorhinal_omni_get_opportunity_at_direction(bridge, M_PI / 2.0f);
    EXPECT_FLOAT_EQ(opportunity, 0.7f);
}

TEST_F(OmniBridgeTest, GetFamiliarityAtDirection) {
    bridge->memory_familiarity[180] = 0.6f;
    float familiarity = entorhinal_omni_get_familiarity_at_direction(bridge, M_PI);
    EXPECT_FLOAT_EQ(familiarity, 0.6f);
}

TEST_F(OmniBridgeTest, GetSectorSummary) {
    bridge->spatial_map.sector_salience[OMNI_SECTOR_FRONT] = 0.8f;
    bridge->spatial_map.sector_threat[OMNI_SECTOR_FRONT] = 0.3f;
    bridge->spatial_map.sector_opportunity[OMNI_SECTOR_FRONT] = 0.5f;

    float salience, threat, opportunity;
    EXPECT_EQ(entorhinal_omni_get_sector_summary(bridge, OMNI_SECTOR_FRONT,
        &salience, &threat, &opportunity), 0);
    EXPECT_FLOAT_EQ(salience, 0.8f);
    EXPECT_FLOAT_EQ(threat, 0.3f);
    EXPECT_FLOAT_EQ(opportunity, 0.5f);
}

TEST_F(OmniBridgeTest, GetSectorSummaryNull) {
    float s, t, o;
    EXPECT_EQ(entorhinal_omni_get_sector_summary(nullptr, OMNI_SECTOR_FRONT, &s, &t, &o), -1);
}

TEST_F(OmniBridgeTest, GetSectorSummaryInvalid) {
    float s, t, o;
    EXPECT_EQ(entorhinal_omni_get_sector_summary(bridge, OMNI_SECTOR_COUNT, &s, &t, &o), -1);
}

/*=============================================================================
 * OBJECT TRACKING TESTS
 *===========================================================================*/

TEST_F(OmniBridgeTest, GetTrackedObjectNotFound) {
    const omni_tracked_object_t* obj = entorhinal_omni_get_tracked_object(bridge, 999);
    EXPECT_EQ(obj, nullptr);
}

TEST_F(OmniBridgeTest, GetTrackedObjectNull) {
    EXPECT_EQ(entorhinal_omni_get_tracked_object(nullptr, 0), nullptr);
}

TEST_F(OmniBridgeTest, GetNearestThreat) {
    omni_threat_vector_t threats[2];
    threats[0].direction = 0.0f;
    threats[0].distance = 10.0f;
    threats[0].magnitude = 0.5f;
    threats[1].direction = M_PI;
    threats[1].distance = 5.0f;  // Nearer
    threats[1].magnitude = 0.8f;
    entorhinal_omni_receive_threats(bridge, threats, 2);

    omni_threat_vector_t nearest;
    EXPECT_EQ(entorhinal_omni_get_nearest_threat(bridge, &nearest), 0);
    EXPECT_FLOAT_EQ(nearest.distance, 5.0f);
}

TEST_F(OmniBridgeTest, GetNearestThreatNoThreats) {
    omni_threat_vector_t nearest;
    EXPECT_EQ(entorhinal_omni_get_nearest_threat(bridge, &nearest), -1);
}

TEST_F(OmniBridgeTest, GetBestOpportunity) {
    omni_opportunity_vector_t opportunities[2];
    opportunities[0].direction = 0.0f;
    opportunities[0].distance = 10.0f;
    opportunities[0].value = 0.5f;
    opportunities[0].accessibility = 0.5f;
    opportunities[1].direction = M_PI;
    opportunities[1].distance = 2.0f;  // Better score (closer + valuable)
    opportunities[1].value = 0.8f;
    opportunities[1].accessibility = 0.9f;
    entorhinal_omni_receive_opportunities(bridge, opportunities, 2);

    omni_opportunity_vector_t best;
    EXPECT_EQ(entorhinal_omni_get_best_opportunity(bridge, &best), 0);
    EXPECT_FLOAT_EQ(best.value, 0.8f);
}

TEST_F(OmniBridgeTest, GetBestOpportunityNoOpportunities) {
    omni_opportunity_vector_t best;
    EXPECT_EQ(entorhinal_omni_get_best_opportunity(bridge, &best), -1);
}

/*=============================================================================
 * ESCAPE/APPROACH VECTOR TESTS
 *===========================================================================*/

TEST_F(OmniBridgeTest, GetEscapeVector) {
    // Add threat from front
    omni_threat_vector_t threat;
    threat.direction = 0.0f;
    threat.distance = 5.0f;
    threat.magnitude = 0.9f;
    entorhinal_omni_receive_threats(bridge, &threat, 1);
    entorhinal_omni_bridge_update(bridge, 0.01f);

    float escape[3];
    EXPECT_EQ(entorhinal_omni_get_escape_vector(bridge, escape), 0);
    // Escape should be opposite to threat (backward)
    EXPECT_LT(escape[0], 0.0f);  // Negative X = backward
}

TEST_F(OmniBridgeTest, GetEscapeVectorNull) {
    float escape[3];
    EXPECT_EQ(entorhinal_omni_get_escape_vector(nullptr, escape), -1);
    EXPECT_EQ(entorhinal_omni_get_escape_vector(bridge, nullptr), -1);
}

TEST_F(OmniBridgeTest, GetApproachVector) {
    // Add opportunity to the right
    omni_opportunity_vector_t opportunity;
    opportunity.direction = M_PI / 2.0f;
    opportunity.distance = 5.0f;
    opportunity.value = 0.9f;
    opportunity.accessibility = 0.8f;
    entorhinal_omni_receive_opportunities(bridge, &opportunity, 1);
    entorhinal_omni_bridge_update(bridge, 0.01f);

    float approach[3];
    EXPECT_EQ(entorhinal_omni_get_approach_vector(bridge, approach), 0);
}

/*=============================================================================
 * ATTENTION TESTS
 *===========================================================================*/

TEST_F(OmniBridgeTest, SetAttentionFocus) {
    EXPECT_EQ(entorhinal_omni_set_attention_focus(bridge, M_PI / 4.0f, 10.0f), 0);
    EXPECT_NEAR(bridge->attention_direction, M_PI / 4.0f, 0.01f);
    EXPECT_FLOAT_EQ(bridge->attention_distance, 10.0f);
    EXPECT_FLOAT_EQ(bridge->attention_strength, 1.0f);
}

TEST_F(OmniBridgeTest, SetAttentionFocusNull) {
    EXPECT_EQ(entorhinal_omni_set_attention_focus(nullptr, 0.0f, 5.0f), -1);
}

TEST_F(OmniBridgeTest, GetAttentionFocus) {
    bridge->attention_direction = M_PI / 3.0f;
    bridge->attention_distance = 8.0f;
    bridge->attention_strength = 0.7f;

    float azimuth, distance, strength;
    EXPECT_EQ(entorhinal_omni_get_attention_focus(bridge, &azimuth, &distance, &strength), 0);
    EXPECT_NEAR(azimuth, M_PI / 3.0f, 0.01f);
    EXPECT_FLOAT_EQ(distance, 8.0f);
    EXPECT_FLOAT_EQ(strength, 0.7f);
}

TEST_F(OmniBridgeTest, AttentionDecaysOverTime) {
    entorhinal_omni_set_attention_focus(bridge, 0.0f, 5.0f);
    float initial_strength = bridge->attention_strength;

    // Run updates
    for (int i = 0; i < 100; i++) {
        entorhinal_omni_bridge_update(bridge, 0.1f);
    }

    EXPECT_LT(bridge->attention_strength, initial_strength);
}

TEST_F(OmniBridgeTest, ComputeAttendedRepresentation) {
    // Set some salience values
    for (int i = 0; i < OMNI_ANGULAR_RESOLUTION; i++) {
        bridge->spatial_map.azimuth_salience[i] = 0.5f;
    }
    bridge->spatial_map.azimuth_salience[45] = 1.0f;  // Peak at 45 degrees

    // Focus attention at 45 degrees
    entorhinal_omni_set_attention_focus(bridge, M_PI / 4.0f, 5.0f);

    float representation[OMNI_ANGULAR_RESOLUTION];
    EXPECT_EQ(entorhinal_omni_compute_attended_representation(bridge,
        representation, OMNI_ANGULAR_RESOLUTION), 0);

    // Values near attention focus should be emphasized
    EXPECT_GT(representation[45], representation[180]);
}

/*=============================================================================
 * BOUNDARY SIGNAL TESTS
 *===========================================================================*/

TEST_F(OmniBridgeTest, GetBoundarySignals) {
    // Set up some nearby boundaries
    bridge->spatial_map.azimuth_distance[0] = 2.0f;
    bridge->spatial_map.azimuth_distance[1] = 3.0f;
    bridge->spatial_map.azimuth_distance[359] = 3.0f;

    float boundary_distances[8];
    float boundary_directions[8];
    uint32_t num_boundaries = 0;

    EXPECT_EQ(entorhinal_omni_get_boundary_signals(bridge,
        boundary_distances, boundary_directions, 8, &num_boundaries), 0);
}

TEST_F(OmniBridgeTest, GetBoundarySignalsNull) {
    float distances[8], directions[8];
    uint32_t num;
    EXPECT_EQ(entorhinal_omni_get_boundary_signals(nullptr, distances, directions, 8, &num), -1);
    EXPECT_EQ(entorhinal_omni_get_boundary_signals(bridge, nullptr, directions, 8, &num), -1);
    EXPECT_EQ(entorhinal_omni_get_boundary_signals(bridge, distances, nullptr, 8, &num), -1);
    EXPECT_EQ(entorhinal_omni_get_boundary_signals(bridge, distances, directions, 8, nullptr), -1);
}

/*=============================================================================
 * GRID CELL INTEGRATION TESTS
 *===========================================================================*/

TEST_F(OmniBridgeTest, ModulateGridCells) {
    entorhinal_omni_bridge_connect(bridge, ec, nullptr);
    EXPECT_EQ(entorhinal_omni_modulate_grid_cells(bridge), 0);
}

TEST_F(OmniBridgeTest, ModulateGridCellsNull) {
    EXPECT_EQ(entorhinal_omni_modulate_grid_cells(nullptr), -1);
}

TEST_F(OmniBridgeTest, SendGridRepresentation) {
    entorhinal_omni_bridge_connect(bridge, ec, nullptr);
    EXPECT_EQ(entorhinal_omni_send_grid_representation(bridge), 0);
}

/*=============================================================================
 * DIAGNOSTICS TESTS
 *===========================================================================*/

TEST_F(OmniBridgeTest, GetStats) {
    uint64_t updates, threats, opportunities;
    EXPECT_EQ(entorhinal_omni_bridge_get_stats(bridge,
        &updates, &threats, &opportunities), 0);
    EXPECT_EQ(updates, 0u);
}

TEST_F(OmniBridgeTest, GetStatsNull) {
    EXPECT_EQ(entorhinal_omni_bridge_get_stats(nullptr, nullptr, nullptr, nullptr), -1);
}

TEST_F(OmniBridgeTest, LogDiagnostics) {
    EXPECT_EQ(entorhinal_omni_bridge_log_diagnostics(bridge), 0);
}

TEST_F(OmniBridgeTest, LogDiagnosticsNull) {
    EXPECT_EQ(entorhinal_omni_bridge_log_diagnostics(nullptr), -1);
}

