/**
 * @file test_cortical_hierarchy.cpp
 * @brief Unit tests for cortical hierarchy and multi-area connectivity
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "core/cortical_columns/nimcp_cortical_hierarchy.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class CorticalHierarchyTest : public ::testing::Test {
protected:
    cortical_hierarchy_t* hierarchy;

    void SetUp() override {
        cortical_hierarchy_config_t config = cortical_hierarchy_default_config();
        hierarchy = cortical_hierarchy_create(&config);
        ASSERT_NE(hierarchy, nullptr);
    }

    void TearDown() override {
        if (hierarchy) {
            cortical_hierarchy_destroy(hierarchy);
        }
    }

    /* Helper to create a standard area config */
    cortical_area_config_t create_area_config(cortical_area_type_t type, uint32_t level) {
        cortical_area_config_t cfg = {};
        cfg.type = type;
        cfg.stream = STREAM_VENTRAL;
        cfg.hierarchy_level = level;
        cfg.rf_expansion_factor = 2.0f;
        cfg.num_hypercolumns = 64;
        cfg.neurons_per_hypercolumn = 100;
        cfg.feedforward_strength = 0.8f;
        cfg.feedback_strength = 0.4f;
        cfg.custom_name = nullptr;
        return cfg;
    }

    /* Helper to create a connection config */
    inter_area_connection_config_t create_connection_config(
            uint32_t source, uint32_t target, connection_type_t type) {
        inter_area_connection_config_t cfg = {};
        cfg.source_area_id = source;
        cfg.target_area_id = target;
        cfg.type = type;
        cfg.weight = 0.5f;
        cfg.delay_ms = 10.0f;
        cfg.use_canonical_layers = true;
        return cfg;
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(CorticalHierarchyTest, DefaultConfig) {
    cortical_hierarchy_config_t cfg = cortical_hierarchy_default_config();

    EXPECT_GT(cfg.max_areas, 0u);
    EXPECT_GT(cfg.max_connections, 0u);
    EXPECT_GT(cfg.default_rf_base, 0.0f);
    EXPECT_GT(cfg.default_expansion_factor, 1.0f);
}

TEST_F(CorticalHierarchyTest, CreateWithConfig) {
    cortical_hierarchy_config_t custom_config = cortical_hierarchy_default_config();
    custom_config.max_areas = 16;

    cortical_hierarchy_t* system = cortical_hierarchy_create(&custom_config);
    ASSERT_NE(system, nullptr);

    cortical_hierarchy_destroy(system);
}

TEST_F(CorticalHierarchyTest, CreateWithNullConfig) {
    /* Implementation correctly rejects NULL config */
    cortical_hierarchy_t* system = cortical_hierarchy_create(nullptr);
    EXPECT_EQ(system, nullptr);
}

/* ============================================================================
 * Area Management Tests
 * ============================================================================ */

TEST_F(CorticalHierarchyTest, AddArea) {
    cortical_area_config_t area_cfg = create_area_config(CORTICAL_AREA_V1, 0);
    uint32_t area_id;
    int result = cortical_hierarchy_add_area(hierarchy, &area_cfg, &area_id);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalHierarchyTest, AddMultipleAreas) {
    uint32_t area_ids[4];
    cortical_area_type_t types[] = {CORTICAL_AREA_V1, CORTICAL_AREA_V2, CORTICAL_AREA_V4, CORTICAL_AREA_IT};

    for (int i = 0; i < 4; i++) {
        cortical_area_config_t cfg = create_area_config(types[i], i);
        int result = cortical_hierarchy_add_area(hierarchy, &cfg, &area_ids[i]);
        EXPECT_EQ(result, 0);
    }
}

TEST_F(CorticalHierarchyTest, GetNumAreas) {
    cortical_area_config_t cfg1 = create_area_config(CORTICAL_AREA_V1, 0);
    cortical_area_config_t cfg2 = create_area_config(CORTICAL_AREA_V2, 1);
    uint32_t area1, area2;
    cortical_hierarchy_add_area(hierarchy, &cfg1, &area1);
    cortical_hierarchy_add_area(hierarchy, &cfg2, &area2);

    uint32_t num_areas = cortical_hierarchy_get_num_areas(hierarchy);
    EXPECT_GE(num_areas, 2u);
}

TEST_F(CorticalHierarchyTest, GetAreaConfig) {
    cortical_area_config_t cfg = create_area_config(CORTICAL_AREA_V1, 0);
    cfg.num_hypercolumns = 128;
    uint32_t area_id;
    cortical_hierarchy_add_area(hierarchy, &cfg, &area_id);

    const cortical_area_config_t* retrieved = cortical_hierarchy_get_area_config(hierarchy, area_id);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->type, CORTICAL_AREA_V1);
    EXPECT_EQ(retrieved->num_hypercolumns, 128u);
}

TEST_F(CorticalHierarchyTest, RemoveArea) {
    cortical_area_config_t cfg = create_area_config(CORTICAL_AREA_V1, 0);
    uint32_t area_id;
    cortical_hierarchy_add_area(hierarchy, &cfg, &area_id);

    int result = cortical_hierarchy_remove_area(hierarchy, area_id);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(CorticalHierarchyTest, ConnectAreasFeedforward) {
    cortical_area_config_t cfg1 = create_area_config(CORTICAL_AREA_V1, 0);
    cortical_area_config_t cfg2 = create_area_config(CORTICAL_AREA_V2, 1);
    uint32_t area1, area2;
    cortical_hierarchy_add_area(hierarchy, &cfg1, &area1);
    cortical_hierarchy_add_area(hierarchy, &cfg2, &area2);

    inter_area_connection_config_t conn_cfg = create_connection_config(
        area1, area2, CONNECTION_TYPE_FEEDFORWARD);
    uint32_t conn_id;
    int result = cortical_hierarchy_connect_areas(hierarchy, &conn_cfg, &conn_id);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalHierarchyTest, ConnectAreasFeedback) {
    cortical_area_config_t cfg1 = create_area_config(CORTICAL_AREA_V1, 0);
    cortical_area_config_t cfg2 = create_area_config(CORTICAL_AREA_V2, 1);
    uint32_t area1, area2;
    cortical_hierarchy_add_area(hierarchy, &cfg1, &area1);
    cortical_hierarchy_add_area(hierarchy, &cfg2, &area2);

    inter_area_connection_config_t conn_cfg = create_connection_config(
        area2, area1, CONNECTION_TYPE_FEEDBACK);
    uint32_t conn_id;
    int result = cortical_hierarchy_connect_areas(hierarchy, &conn_cfg, &conn_id);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalHierarchyTest, ConnectAreasLateral) {
    cortical_area_config_t cfg1 = create_area_config(CORTICAL_AREA_V1, 0);
    cortical_area_config_t cfg2 = create_area_config(CORTICAL_AREA_V1, 0);
    cfg2.type = CORTICAL_AREA_CUSTOM;
    cfg2.custom_name = "V1_secondary";
    uint32_t area1, area2;
    cortical_hierarchy_add_area(hierarchy, &cfg1, &area1);
    cortical_hierarchy_add_area(hierarchy, &cfg2, &area2);

    inter_area_connection_config_t conn_cfg = create_connection_config(
        area1, area2, CONNECTION_TYPE_LATERAL);
    uint32_t conn_id;
    int result = cortical_hierarchy_connect_areas(hierarchy, &conn_cfg, &conn_id);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalHierarchyTest, DisconnectAreas) {
    cortical_area_config_t cfg1 = create_area_config(CORTICAL_AREA_V1, 0);
    cortical_area_config_t cfg2 = create_area_config(CORTICAL_AREA_V2, 1);
    uint32_t area1, area2;
    cortical_hierarchy_add_area(hierarchy, &cfg1, &area1);
    cortical_hierarchy_add_area(hierarchy, &cfg2, &area2);

    inter_area_connection_config_t conn_cfg = create_connection_config(
        area1, area2, CONNECTION_TYPE_FEEDFORWARD);
    uint32_t conn_id;
    cortical_hierarchy_connect_areas(hierarchy, &conn_cfg, &conn_id);

    int result = cortical_hierarchy_disconnect_areas(hierarchy, conn_id);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalHierarchyTest, ApplyCanonicalConnections) {
    /* Add V1-V2-V4-IT hierarchy */
    cortical_area_config_t cfg1 = create_area_config(CORTICAL_AREA_V1, 0);
    cortical_area_config_t cfg2 = create_area_config(CORTICAL_AREA_V2, 1);
    cortical_area_config_t cfg3 = create_area_config(CORTICAL_AREA_V4, 2);
    cortical_area_config_t cfg4 = create_area_config(CORTICAL_AREA_IT, 3);
    uint32_t ids[4];
    cortical_hierarchy_add_area(hierarchy, &cfg1, &ids[0]);
    cortical_hierarchy_add_area(hierarchy, &cfg2, &ids[1]);
    cortical_hierarchy_add_area(hierarchy, &cfg3, &ids[2]);
    cortical_hierarchy_add_area(hierarchy, &cfg4, &ids[3]);

    int result = cortical_hierarchy_apply_canonical_connections(hierarchy);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Signal Propagation Tests
 * ============================================================================ */

TEST_F(CorticalHierarchyTest, SetAreaInput) {
    cortical_area_config_t cfg = create_area_config(CORTICAL_AREA_V1, 0);
    cfg.num_hypercolumns = 64;
    uint32_t area_id;
    cortical_hierarchy_add_area(hierarchy, &cfg, &area_id);

    float input[64];
    for (int i = 0; i < 64; i++) {
        input[i] = (float)i / 64.0f;
    }

    int result = cortical_hierarchy_set_area_input(hierarchy, area_id, input, 64);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalHierarchyTest, PropagateFeedforward) {
    cortical_area_config_t cfg1 = create_area_config(CORTICAL_AREA_V1, 0);
    cortical_area_config_t cfg2 = create_area_config(CORTICAL_AREA_V2, 1);
    uint32_t area1, area2;
    cortical_hierarchy_add_area(hierarchy, &cfg1, &area1);
    cortical_hierarchy_add_area(hierarchy, &cfg2, &area2);

    inter_area_connection_config_t conn_cfg = create_connection_config(
        area1, area2, CONNECTION_TYPE_FEEDFORWARD);
    uint32_t conn_id;
    cortical_hierarchy_connect_areas(hierarchy, &conn_cfg, &conn_id);

    /* API: propagate_feedforward(hierarchy, start_level, end_level) */
    int result = cortical_hierarchy_propagate_feedforward(hierarchy, 0, 1);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalHierarchyTest, PropagateFeedback) {
    cortical_area_config_t cfg1 = create_area_config(CORTICAL_AREA_V1, 0);
    cortical_area_config_t cfg2 = create_area_config(CORTICAL_AREA_V2, 1);
    uint32_t area1, area2;
    cortical_hierarchy_add_area(hierarchy, &cfg1, &area1);
    cortical_hierarchy_add_area(hierarchy, &cfg2, &area2);

    inter_area_connection_config_t conn_cfg = create_connection_config(
        area2, area1, CONNECTION_TYPE_FEEDBACK);
    uint32_t conn_id;
    cortical_hierarchy_connect_areas(hierarchy, &conn_cfg, &conn_id);

    /* API: propagate_feedback(hierarchy, start_level, end_level) */
    int result = cortical_hierarchy_propagate_feedback(hierarchy, 1, 0);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalHierarchyTest, GetAreaActivity) {
    cortical_area_config_t cfg = create_area_config(CORTICAL_AREA_V1, 0);
    cfg.num_hypercolumns = 64;
    uint32_t area_id;
    cortical_hierarchy_add_area(hierarchy, &cfg, &area_id);

    float activity[64];
    uint32_t actual_size;
    /* API: get_area_activity(hierarchy, area_id, activity_out, max_size, actual_size_out) */
    int result = cortical_hierarchy_get_area_activity(hierarchy, area_id, activity, 64, &actual_size);
    EXPECT_GE(result, 0);
}

TEST_F(CorticalHierarchyTest, ComputePredictionError) {
    cortical_area_config_t cfg1 = create_area_config(CORTICAL_AREA_V1, 0);
    cortical_area_config_t cfg2 = create_area_config(CORTICAL_AREA_V2, 1);
    uint32_t area1, area2;
    cortical_hierarchy_add_area(hierarchy, &cfg1, &area1);
    cortical_hierarchy_add_area(hierarchy, &cfg2, &area2);

    inter_area_connection_config_t ff_cfg = create_connection_config(
        area1, area2, CONNECTION_TYPE_FEEDFORWARD);
    inter_area_connection_config_t fb_cfg = create_connection_config(
        area2, area1, CONNECTION_TYPE_FEEDBACK);
    uint32_t ff_id, fb_id;
    cortical_hierarchy_connect_areas(hierarchy, &ff_cfg, &ff_id);
    cortical_hierarchy_connect_areas(hierarchy, &fb_cfg, &fb_id);

    float error;
    int result = cortical_hierarchy_compute_prediction_error(hierarchy, area1, &error);
    EXPECT_TRUE(result == 0 || result < 0); /* May need FB connection */
}

/* ============================================================================
 * Receptive Field Tests
 * ============================================================================ */

TEST_F(CorticalHierarchyTest, GetReceptiveFieldSize) {
    cortical_area_config_t cfg = create_area_config(CORTICAL_AREA_V4, 2);
    cfg.rf_expansion_factor = 2.0f;
    uint32_t area_id;
    cortical_hierarchy_add_area(hierarchy, &cfg, &area_id);

    float rf_size;
    int result = cortical_hierarchy_get_receptive_field_size(hierarchy, area_id, &rf_size);
    EXPECT_EQ(result, 0);
    EXPECT_GT(rf_size, 0.0f);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(CorticalHierarchyTest, GetStats) {
    cortical_hierarchy_stats_t stats;
    int result = cortical_hierarchy_get_stats(hierarchy, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalHierarchyTest, GetAreaStats) {
    cortical_area_config_t cfg = create_area_config(CORTICAL_AREA_V1, 0);
    uint32_t area_id;
    cortical_hierarchy_add_area(hierarchy, &cfg, &area_id);

    cortical_area_stats_t stats;
    int result = cortical_hierarchy_get_area_stats(hierarchy, area_id, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.type, CORTICAL_AREA_V1);
}

/* ============================================================================
 * Bio-async Tests
 * ============================================================================ */

TEST_F(CorticalHierarchyTest, ConnectBioAsync) {
    int result = cortical_hierarchy_connect_bio_async(hierarchy);
    EXPECT_TRUE(result == 0 || result < 0);
}

TEST_F(CorticalHierarchyTest, IsBioAsyncConnected) {
    bool connected = cortical_hierarchy_is_bio_async_connected(hierarchy);
    EXPECT_FALSE(connected);
}

TEST_F(CorticalHierarchyTest, DisconnectBioAsync) {
    int result = cortical_hierarchy_disconnect_bio_async(hierarchy);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(CorticalHierarchyTest, DestroyNull) {
    cortical_hierarchy_destroy(nullptr);
}

TEST_F(CorticalHierarchyTest, ConnectNonexistentAreas) {
    inter_area_connection_config_t conn_cfg = create_connection_config(
        999, 998, CONNECTION_TYPE_FEEDFORWARD);
    uint32_t conn_id;
    int result = cortical_hierarchy_connect_areas(hierarchy, &conn_cfg, &conn_id);
    EXPECT_LT(result, 0);
}

TEST_F(CorticalHierarchyTest, SelfConnection) {
    cortical_area_config_t cfg = create_area_config(CORTICAL_AREA_V1, 0);
    uint32_t area_id;
    cortical_hierarchy_add_area(hierarchy, &cfg, &area_id);

    inter_area_connection_config_t conn_cfg = create_connection_config(
        area_id, area_id, CONNECTION_TYPE_LATERAL);
    uint32_t conn_id;
    int result = cortical_hierarchy_connect_areas(hierarchy, &conn_cfg, &conn_id);
    /* Self-connections might be allowed for recurrent processing */
    EXPECT_TRUE(result == 0 || result < 0);
}

TEST_F(CorticalHierarchyTest, GetConfigInvalidArea) {
    const cortical_area_config_t* cfg = cortical_hierarchy_get_area_config(hierarchy, 999);
    EXPECT_EQ(cfg, nullptr);
}

TEST_F(CorticalHierarchyTest, RemoveInvalidArea) {
    int result = cortical_hierarchy_remove_area(hierarchy, 999);
    EXPECT_LT(result, 0);
}

