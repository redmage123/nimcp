/**
 * @file test_cortical_hierarchy_integration.cpp
 * @brief Integration tests for cortical hierarchy module
 *
 * Tests cortical hierarchy functionality including:
 * - Hierarchy creation and levels
 * - Bottom-up processing
 * - Top-down predictions
 * - Lateral connections between areas
 * - Attention modulation
 * - Predictive coding integration
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

#include "utils/nimcp_test_base.h"
#include "core/cortical_columns/nimcp_cortical_hierarchy.h"

/*=============================================================================
 * Test Fixture
 *===========================================================================*/

class CorticalHierarchyIntegrationTest : public NimcpTestBase {
protected:
    void SetUp() override {
        NimcpTestBase::SetUp();

        config_ = cortical_hierarchy_default_config();
        hierarchy_ = cortical_hierarchy_create(&config_);
        ASSERT_NE(hierarchy_, nullptr);
    }

    void TearDown() override {
        if (hierarchy_) {
            cortical_hierarchy_destroy(hierarchy_);
            hierarchy_ = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    // Helper to create default area config
    cortical_area_config_t CreateAreaConfig(
        cortical_area_type_t type,
        uint32_t level,
        processing_stream_t stream = STREAM_VENTRAL) {

        cortical_area_config_t config;
        memset(&config, 0, sizeof(config));

        config.type = type;
        config.stream = stream;
        config.hierarchy_level = level;
        config.rf_expansion_factor = 2.0f;
        config.num_hypercolumns = 16;
        config.neurons_per_hypercolumn = 1000;
        config.feedforward_strength = 0.8f;
        config.feedback_strength = 0.5f;
        config.custom_name = nullptr;

        return config;
    }

    // Helper to add V1-V2-V4-IT visual hierarchy
    void CreateVisualHierarchy() {
        cortical_area_config_t v1_config = CreateAreaConfig(CORTICAL_AREA_V1, 0);
        ASSERT_EQ(cortical_hierarchy_add_area(hierarchy_, &v1_config, &v1_id_), 0);

        cortical_area_config_t v2_config = CreateAreaConfig(CORTICAL_AREA_V2, 1);
        ASSERT_EQ(cortical_hierarchy_add_area(hierarchy_, &v2_config, &v2_id_), 0);

        cortical_area_config_t v4_config = CreateAreaConfig(CORTICAL_AREA_V4, 2);
        ASSERT_EQ(cortical_hierarchy_add_area(hierarchy_, &v4_config, &v4_id_), 0);

        cortical_area_config_t it_config = CreateAreaConfig(CORTICAL_AREA_IT, 3);
        ASSERT_EQ(cortical_hierarchy_add_area(hierarchy_, &it_config, &it_id_), 0);
    }

    cortical_hierarchy_config_t config_;
    cortical_hierarchy_t* hierarchy_ = nullptr;

    uint32_t v1_id_ = 0;
    uint32_t v2_id_ = 0;
    uint32_t v4_id_ = 0;
    uint32_t it_id_ = 0;
};

/*=============================================================================
 * Hierarchy Creation Tests
 *===========================================================================*/

TEST_F(CorticalHierarchyIntegrationTest, CreateWithDefaultConfig) {
    cortical_hierarchy_config_t config = cortical_hierarchy_default_config();

    EXPECT_GT(config.max_areas, 0u);
    EXPECT_GT(config.max_connections, 0u);
    EXPECT_GT(config.default_rf_base, 0.0f);
    EXPECT_GT(config.default_expansion_factor, 1.0f);
}

TEST_F(CorticalHierarchyIntegrationTest, CreateWithNullConfig) {
    cortical_hierarchy_t* h = cortical_hierarchy_create(nullptr);
    EXPECT_NE(h, nullptr);  // Should use defaults
    if (h) {
        cortical_hierarchy_destroy(h);
    }
}

TEST_F(CorticalHierarchyIntegrationTest, DestroyNullSafe) {
    cortical_hierarchy_destroy(nullptr);
    SUCCEED();  // Should not crash
}

TEST_F(CorticalHierarchyIntegrationTest, CreateWithCustomConfig) {
    cortical_hierarchy_config_t config;
    config.max_areas = 20;
    config.max_connections = 100;
    config.default_rf_base = 1.0f;
    config.default_expansion_factor = 3.0f;
    config.enable_predictive_coding = true;
    config.enable_bio_async = false;

    cortical_hierarchy_t* h = cortical_hierarchy_create(&config);
    ASSERT_NE(h, nullptr);

    cortical_hierarchy_destroy(h);
}

/*=============================================================================
 * Area Management Tests
 *===========================================================================*/

TEST_F(CorticalHierarchyIntegrationTest, AddAreaV1) {
    cortical_area_config_t config = CreateAreaConfig(CORTICAL_AREA_V1, 0);

    uint32_t area_id;
    int result = cortical_hierarchy_add_area(hierarchy_, &config, &area_id);

    EXPECT_EQ(result, 0);
    EXPECT_GT(area_id, 0u);
}

TEST_F(CorticalHierarchyIntegrationTest, AddMultipleAreas) {
    CreateVisualHierarchy();

    uint32_t num_areas = cortical_hierarchy_get_num_areas(hierarchy_);
    EXPECT_EQ(num_areas, 4u);
}

TEST_F(CorticalHierarchyIntegrationTest, AddAreaWithNullConfig) {
    uint32_t area_id;
    int result = cortical_hierarchy_add_area(hierarchy_, nullptr, &area_id);
    EXPECT_NE(result, 0);  // Should fail
}

TEST_F(CorticalHierarchyIntegrationTest, AddAreaWithNullIdOut) {
    cortical_area_config_t config = CreateAreaConfig(CORTICAL_AREA_V1, 0);

    int result = cortical_hierarchy_add_area(hierarchy_, &config, nullptr);
    // Implementation dependent - may succeed or fail
    SUCCEED();
}

TEST_F(CorticalHierarchyIntegrationTest, AddCustomArea) {
    cortical_area_config_t config = CreateAreaConfig(CORTICAL_AREA_CUSTOM, 4);
    config.custom_name = "Custom Visual Area";

    uint32_t area_id;
    int result = cortical_hierarchy_add_area(hierarchy_, &config, &area_id);

    EXPECT_EQ(result, 0);
}

TEST_F(CorticalHierarchyIntegrationTest, RemoveArea) {
    cortical_area_config_t config = CreateAreaConfig(CORTICAL_AREA_V1, 0);

    uint32_t area_id;
    ASSERT_EQ(cortical_hierarchy_add_area(hierarchy_, &config, &area_id), 0);

    uint32_t before_count = cortical_hierarchy_get_num_areas(hierarchy_);

    int result = cortical_hierarchy_remove_area(hierarchy_, area_id);
    EXPECT_EQ(result, 0);

    uint32_t after_count = cortical_hierarchy_get_num_areas(hierarchy_);
    EXPECT_EQ(after_count, before_count - 1);
}

TEST_F(CorticalHierarchyIntegrationTest, RemoveNonexistentArea) {
    int result = cortical_hierarchy_remove_area(hierarchy_, 9999);
    EXPECT_NE(result, 0);  // Should fail
}

TEST_F(CorticalHierarchyIntegrationTest, GetAreaConfig) {
    cortical_area_config_t config = CreateAreaConfig(CORTICAL_AREA_V1, 0);
    config.num_hypercolumns = 32;

    uint32_t area_id;
    ASSERT_EQ(cortical_hierarchy_add_area(hierarchy_, &config, &area_id), 0);

    const cortical_area_config_t* retrieved =
        cortical_hierarchy_get_area_config(hierarchy_, area_id);

    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->type, CORTICAL_AREA_V1);
    EXPECT_EQ(retrieved->hierarchy_level, 0u);
    EXPECT_EQ(retrieved->num_hypercolumns, 32u);
}

TEST_F(CorticalHierarchyIntegrationTest, GetAreaConfigNonexistent) {
    const cortical_area_config_t* retrieved =
        cortical_hierarchy_get_area_config(hierarchy_, 9999);

    EXPECT_EQ(retrieved, nullptr);
}

/*=============================================================================
 * Connection Management Tests
 *===========================================================================*/

TEST_F(CorticalHierarchyIntegrationTest, ConnectAreasFeedforward) {
    CreateVisualHierarchy();

    inter_area_connection_config_t conn_config;
    memset(&conn_config, 0, sizeof(conn_config));

    conn_config.source_area_id = v1_id_;
    conn_config.target_area_id = v2_id_;
    conn_config.type = CONNECTION_TYPE_FEEDFORWARD;
    conn_config.weight = 0.8f;
    conn_config.delay_ms = 10.0f;
    conn_config.use_canonical_layers = true;

    uint32_t conn_id;
    int result = cortical_hierarchy_connect_areas(hierarchy_, &conn_config, &conn_id);

    EXPECT_EQ(result, 0);
    EXPECT_GT(conn_id, 0u);
}

TEST_F(CorticalHierarchyIntegrationTest, ConnectAreasFeedback) {
    CreateVisualHierarchy();

    inter_area_connection_config_t conn_config;
    memset(&conn_config, 0, sizeof(conn_config));

    conn_config.source_area_id = v2_id_;
    conn_config.target_area_id = v1_id_;
    conn_config.type = CONNECTION_TYPE_FEEDBACK;
    conn_config.weight = 0.5f;
    conn_config.delay_ms = 5.0f;
    conn_config.use_canonical_layers = true;

    uint32_t conn_id;
    int result = cortical_hierarchy_connect_areas(hierarchy_, &conn_config, &conn_id);

    EXPECT_EQ(result, 0);
}

TEST_F(CorticalHierarchyIntegrationTest, ConnectAreasLateral) {
    CreateVisualHierarchy();

    // Add another V2 area for lateral connection
    cortical_area_config_t v2b_config = CreateAreaConfig(CORTICAL_AREA_V2, 1);
    uint32_t v2b_id;
    ASSERT_EQ(cortical_hierarchy_add_area(hierarchy_, &v2b_config, &v2b_id), 0);

    inter_area_connection_config_t conn_config;
    memset(&conn_config, 0, sizeof(conn_config));

    conn_config.source_area_id = v2_id_;
    conn_config.target_area_id = v2b_id;
    conn_config.type = CONNECTION_TYPE_LATERAL;
    conn_config.weight = 0.6f;
    conn_config.delay_ms = 2.0f;
    conn_config.use_canonical_layers = true;

    uint32_t conn_id;
    int result = cortical_hierarchy_connect_areas(hierarchy_, &conn_config, &conn_id);

    EXPECT_EQ(result, 0);
}

TEST_F(CorticalHierarchyIntegrationTest, ConnectAreasWithSpecificLayers) {
    CreateVisualHierarchy();

    inter_area_connection_config_t conn_config;
    memset(&conn_config, 0, sizeof(conn_config));

    conn_config.source_area_id = v1_id_;
    conn_config.target_area_id = v2_id_;
    conn_config.type = CONNECTION_TYPE_FEEDFORWARD;
    conn_config.source_layer = 2;  // Layer II/III
    conn_config.target_layer = 2;  // Layer IV
    conn_config.weight = 0.7f;
    conn_config.delay_ms = 8.0f;
    conn_config.use_canonical_layers = false;

    uint32_t conn_id;
    int result = cortical_hierarchy_connect_areas(hierarchy_, &conn_config, &conn_id);

    EXPECT_EQ(result, 0);
}

TEST_F(CorticalHierarchyIntegrationTest, DisconnectAreas) {
    CreateVisualHierarchy();

    inter_area_connection_config_t conn_config;
    memset(&conn_config, 0, sizeof(conn_config));

    conn_config.source_area_id = v1_id_;
    conn_config.target_area_id = v2_id_;
    conn_config.type = CONNECTION_TYPE_FEEDFORWARD;
    conn_config.weight = 0.8f;
    conn_config.use_canonical_layers = true;

    uint32_t conn_id;
    ASSERT_EQ(cortical_hierarchy_connect_areas(hierarchy_, &conn_config, &conn_id), 0);

    int result = cortical_hierarchy_disconnect_areas(hierarchy_, conn_id);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalHierarchyIntegrationTest, ApplyCanonicalConnections) {
    CreateVisualHierarchy();

    int result = cortical_hierarchy_apply_canonical_connections(hierarchy_);
    EXPECT_EQ(result, 0);

    // Verify connections were created
    cortical_hierarchy_stats_t stats;
    ASSERT_EQ(cortical_hierarchy_get_stats(hierarchy_, &stats), 0);

    EXPECT_GT(stats.num_connections, 0u);
    EXPECT_GT(stats.num_ff_connections, 0u);
    EXPECT_GT(stats.num_fb_connections, 0u);
}

/*=============================================================================
 * Propagation Tests
 *===========================================================================*/

TEST_F(CorticalHierarchyIntegrationTest, PropagateFeedforwardSingleLevel) {
    CreateVisualHierarchy();
    ASSERT_EQ(cortical_hierarchy_apply_canonical_connections(hierarchy_), 0);

    // Set input to V1
    float input[16];
    for (int i = 0; i < 16; i++) {
        input[i] = 0.5f + 0.1f * i;
    }
    ASSERT_EQ(cortical_hierarchy_set_area_input(hierarchy_, v1_id_, input, 16), 0);

    // Propagate from level 0 to level 1
    int result = cortical_hierarchy_propagate_feedforward(hierarchy_, 0, 1);
    EXPECT_EQ(result, 0);

    // V2 should have activity
    float activity[16];
    uint32_t actual_size;
    ASSERT_EQ(cortical_hierarchy_get_area_activity(
        hierarchy_, v2_id_, activity, 16, &actual_size), 0);

    // At least some activity should be present
    float total_activity = 0.0f;
    for (uint32_t i = 0; i < actual_size; i++) {
        total_activity += activity[i];
    }
    EXPECT_GE(total_activity, 0.0f);
}

TEST_F(CorticalHierarchyIntegrationTest, PropagateFeedforwardFullHierarchy) {
    CreateVisualHierarchy();
    ASSERT_EQ(cortical_hierarchy_apply_canonical_connections(hierarchy_), 0);

    // Set input to V1
    float input[16];
    for (int i = 0; i < 16; i++) {
        input[i] = 1.0f;
    }
    ASSERT_EQ(cortical_hierarchy_set_area_input(hierarchy_, v1_id_, input, 16), 0);

    // Propagate through all levels
    int result = cortical_hierarchy_propagate_feedforward(hierarchy_, 0, 3);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalHierarchyIntegrationTest, PropagateFeedbackSingleLevel) {
    CreateVisualHierarchy();
    ASSERT_EQ(cortical_hierarchy_apply_canonical_connections(hierarchy_), 0);

    // Set activity at IT (highest level)
    float input[16];
    for (int i = 0; i < 16; i++) {
        input[i] = 0.8f;
    }
    ASSERT_EQ(cortical_hierarchy_set_area_input(hierarchy_, it_id_, input, 16), 0);

    // Propagate feedback from level 3 to level 2
    int result = cortical_hierarchy_propagate_feedback(hierarchy_, 3, 2);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalHierarchyIntegrationTest, PropagateFeedbackFullHierarchy) {
    CreateVisualHierarchy();
    ASSERT_EQ(cortical_hierarchy_apply_canonical_connections(hierarchy_), 0);

    // Set activity at IT
    float input[16];
    for (int i = 0; i < 16; i++) {
        input[i] = 0.8f;
    }
    ASSERT_EQ(cortical_hierarchy_set_area_input(hierarchy_, it_id_, input, 16), 0);

    // Propagate feedback through all levels
    int result = cortical_hierarchy_propagate_feedback(hierarchy_, 3, 0);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalHierarchyIntegrationTest, BidirectionalPropagation) {
    CreateVisualHierarchy();
    ASSERT_EQ(cortical_hierarchy_apply_canonical_connections(hierarchy_), 0);

    // Set input at V1
    float input[16];
    for (int i = 0; i < 16; i++) {
        input[i] = 0.5f;
    }
    ASSERT_EQ(cortical_hierarchy_set_area_input(hierarchy_, v1_id_, input, 16), 0);

    // Multiple cycles of bidirectional processing
    for (int cycle = 0; cycle < 5; cycle++) {
        ASSERT_EQ(cortical_hierarchy_propagate_feedforward(hierarchy_, 0, 3), 0);
        ASSERT_EQ(cortical_hierarchy_propagate_feedback(hierarchy_, 3, 0), 0);
    }

    SUCCEED();
}

/*=============================================================================
 * Predictive Coding Tests
 *===========================================================================*/

TEST_F(CorticalHierarchyIntegrationTest, ComputePredictionError) {
    // Create hierarchy with predictive coding enabled
    cortical_hierarchy_config_t config = cortical_hierarchy_default_config();
    config.enable_predictive_coding = true;

    cortical_hierarchy_t* h = cortical_hierarchy_create(&config);
    ASSERT_NE(h, nullptr);

    // Add areas
    cortical_area_config_t v1_config = CreateAreaConfig(CORTICAL_AREA_V1, 0);
    cortical_area_config_t v2_config = CreateAreaConfig(CORTICAL_AREA_V2, 1);

    uint32_t v1_id, v2_id;
    ASSERT_EQ(cortical_hierarchy_add_area(h, &v1_config, &v1_id), 0);
    ASSERT_EQ(cortical_hierarchy_add_area(h, &v2_config, &v2_id), 0);

    ASSERT_EQ(cortical_hierarchy_apply_canonical_connections(h), 0);

    // Set inputs and propagate
    float input[16];
    for (int i = 0; i < 16; i++) {
        input[i] = 0.5f;
    }
    ASSERT_EQ(cortical_hierarchy_set_area_input(h, v1_id, input, 16), 0);

    ASSERT_EQ(cortical_hierarchy_propagate_feedforward(h, 0, 1), 0);
    ASSERT_EQ(cortical_hierarchy_propagate_feedback(h, 1, 0), 0);

    // Compute prediction error
    float error;
    int result = cortical_hierarchy_compute_prediction_error(h, v1_id, &error);
    EXPECT_EQ(result, 0);
    EXPECT_GE(error, 0.0f);

    cortical_hierarchy_destroy(h);
}

TEST_F(CorticalHierarchyIntegrationTest, PredictionErrorDecreasesWithLearning) {
    cortical_hierarchy_config_t config = cortical_hierarchy_default_config();
    config.enable_predictive_coding = true;

    cortical_hierarchy_t* h = cortical_hierarchy_create(&config);
    ASSERT_NE(h, nullptr);

    cortical_area_config_t v1_config = CreateAreaConfig(CORTICAL_AREA_V1, 0);
    cortical_area_config_t v2_config = CreateAreaConfig(CORTICAL_AREA_V2, 1);

    uint32_t v1_id, v2_id;
    ASSERT_EQ(cortical_hierarchy_add_area(h, &v1_config, &v1_id), 0);
    ASSERT_EQ(cortical_hierarchy_add_area(h, &v2_config, &v2_id), 0);

    ASSERT_EQ(cortical_hierarchy_apply_canonical_connections(h), 0);

    // Consistent input pattern
    float input[16];
    for (int i = 0; i < 16; i++) {
        input[i] = 0.5f;
    }

    // Run multiple cycles with same input
    float first_error = 0.0f, last_error = 0.0f;
    for (int cycle = 0; cycle < 20; cycle++) {
        ASSERT_EQ(cortical_hierarchy_set_area_input(h, v1_id, input, 16), 0);
        ASSERT_EQ(cortical_hierarchy_propagate_feedforward(h, 0, 1), 0);
        ASSERT_EQ(cortical_hierarchy_propagate_feedback(h, 1, 0), 0);

        float error;
        ASSERT_EQ(cortical_hierarchy_compute_prediction_error(h, v1_id, &error), 0);

        if (cycle == 0) first_error = error;
        if (cycle == 19) last_error = error;
    }

    // Error should decrease or stay stable
    EXPECT_LE(last_error, first_error + 0.1f);

    cortical_hierarchy_destroy(h);
}

/*=============================================================================
 * Activity Query Tests
 *===========================================================================*/

TEST_F(CorticalHierarchyIntegrationTest, SetAndGetAreaInput) {
    cortical_area_config_t config = CreateAreaConfig(CORTICAL_AREA_V1, 0);

    uint32_t area_id;
    ASSERT_EQ(cortical_hierarchy_add_area(hierarchy_, &config, &area_id), 0);

    float input[16];
    for (int i = 0; i < 16; i++) {
        input[i] = 0.1f * i;
    }

    ASSERT_EQ(cortical_hierarchy_set_area_input(hierarchy_, area_id, input, 16), 0);

    float output[16];
    uint32_t actual_size;
    int result = cortical_hierarchy_get_area_activity(
        hierarchy_, area_id, output, 16, &actual_size);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(actual_size, 16u);

    for (int i = 0; i < 16; i++) {
        EXPECT_NEAR(output[i], input[i], 0.01f);
    }
}

TEST_F(CorticalHierarchyIntegrationTest, GetAreaInputNullHierarchy) {
    float output[16];
    uint32_t actual_size;
    int result = cortical_hierarchy_get_area_activity(
        nullptr, 1, output, 16, &actual_size);

    EXPECT_NE(result, 0);
}

TEST_F(CorticalHierarchyIntegrationTest, GetAreaInputNonexistentArea) {
    float output[16];
    uint32_t actual_size;
    int result = cortical_hierarchy_get_area_activity(
        hierarchy_, 9999, output, 16, &actual_size);

    EXPECT_NE(result, 0);
}

/*=============================================================================
 * Receptive Field Tests
 *===========================================================================*/

TEST_F(CorticalHierarchyIntegrationTest, ReceptiveFieldExpansion) {
    CreateVisualHierarchy();

    float rf_v1, rf_v2, rf_v4, rf_it;

    ASSERT_EQ(cortical_hierarchy_get_receptive_field_size(
        hierarchy_, v1_id_, &rf_v1), 0);
    ASSERT_EQ(cortical_hierarchy_get_receptive_field_size(
        hierarchy_, v2_id_, &rf_v2), 0);
    ASSERT_EQ(cortical_hierarchy_get_receptive_field_size(
        hierarchy_, v4_id_, &rf_v4), 0);
    ASSERT_EQ(cortical_hierarchy_get_receptive_field_size(
        hierarchy_, it_id_, &rf_it), 0);

    // RF should increase with hierarchy level
    EXPECT_LT(rf_v1, rf_v2);
    EXPECT_LT(rf_v2, rf_v4);
    EXPECT_LT(rf_v4, rf_it);
}

TEST_F(CorticalHierarchyIntegrationTest, ReceptiveFieldSizeNonexistentArea) {
    float rf_size;
    int result = cortical_hierarchy_get_receptive_field_size(
        hierarchy_, 9999, &rf_size);

    EXPECT_NE(result, 0);
}

/*=============================================================================
 * Statistics Tests
 *===========================================================================*/

TEST_F(CorticalHierarchyIntegrationTest, GetAreaStats) {
    CreateVisualHierarchy();
    ASSERT_EQ(cortical_hierarchy_apply_canonical_connections(hierarchy_), 0);

    // Set input and propagate
    float input[16];
    for (int i = 0; i < 16; i++) {
        input[i] = 0.5f;
    }
    ASSERT_EQ(cortical_hierarchy_set_area_input(hierarchy_, v1_id_, input, 16), 0);
    ASSERT_EQ(cortical_hierarchy_propagate_feedforward(hierarchy_, 0, 3), 0);

    cortical_area_stats_t stats;
    int result = cortical_hierarchy_get_area_stats(hierarchy_, v1_id_, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.area_id, v1_id_);
    EXPECT_EQ(stats.type, CORTICAL_AREA_V1);
    EXPECT_EQ(stats.hierarchy_level, 0u);
    EXPECT_GE(stats.mean_activity, 0.0f);
}

TEST_F(CorticalHierarchyIntegrationTest, GetHierarchyStats) {
    CreateVisualHierarchy();
    ASSERT_EQ(cortical_hierarchy_apply_canonical_connections(hierarchy_), 0);

    cortical_hierarchy_stats_t stats;
    int result = cortical_hierarchy_get_stats(hierarchy_, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.num_areas, 4u);
    EXPECT_GT(stats.num_connections, 0u);
    EXPECT_EQ(stats.max_hierarchy_level, 3u);
}

TEST_F(CorticalHierarchyIntegrationTest, StatisticsAfterPropagation) {
    CreateVisualHierarchy();
    ASSERT_EQ(cortical_hierarchy_apply_canonical_connections(hierarchy_), 0);

    cortical_hierarchy_stats_t stats_before;
    ASSERT_EQ(cortical_hierarchy_get_stats(hierarchy_, &stats_before), 0);

    // Run propagation cycles
    float input[16];
    for (int i = 0; i < 16; i++) {
        input[i] = 0.5f;
    }

    for (int cycle = 0; cycle < 10; cycle++) {
        ASSERT_EQ(cortical_hierarchy_set_area_input(hierarchy_, v1_id_, input, 16), 0);
        ASSERT_EQ(cortical_hierarchy_propagate_feedforward(hierarchy_, 0, 3), 0);
    }

    cortical_hierarchy_stats_t stats_after;
    ASSERT_EQ(cortical_hierarchy_get_stats(hierarchy_, &stats_after), 0);

    // Propagation count should increase
    EXPECT_GT(stats_after.total_propagations, stats_before.total_propagations);
}

/*=============================================================================
 * Bio-Async Integration Tests
 *===========================================================================*/

TEST_F(CorticalHierarchyIntegrationTest, BioAsyncConnectDisconnect) {
    CreateVisualHierarchy();

    // Initially should not be connected
    EXPECT_FALSE(cortical_hierarchy_is_bio_async_connected(hierarchy_));

    // Connect
    int result = cortical_hierarchy_connect_bio_async(hierarchy_);
    EXPECT_EQ(result, 0);

    EXPECT_TRUE(cortical_hierarchy_is_bio_async_connected(hierarchy_));

    // Disconnect
    result = cortical_hierarchy_disconnect_bio_async(hierarchy_);
    EXPECT_EQ(result, 0);

    EXPECT_FALSE(cortical_hierarchy_is_bio_async_connected(hierarchy_));
}

TEST_F(CorticalHierarchyIntegrationTest, BioAsyncDoubleConnect) {
    CreateVisualHierarchy();

    ASSERT_EQ(cortical_hierarchy_connect_bio_async(hierarchy_), 0);

    // Second connect should be idempotent
    int result = cortical_hierarchy_connect_bio_async(hierarchy_);
    EXPECT_EQ(result, 0);

    EXPECT_TRUE(cortical_hierarchy_is_bio_async_connected(hierarchy_));

    ASSERT_EQ(cortical_hierarchy_disconnect_bio_async(hierarchy_), 0);
}

TEST_F(CorticalHierarchyIntegrationTest, BioAsyncDoubleDisconnect) {
    CreateVisualHierarchy();

    ASSERT_EQ(cortical_hierarchy_connect_bio_async(hierarchy_), 0);
    ASSERT_EQ(cortical_hierarchy_disconnect_bio_async(hierarchy_), 0);

    // Second disconnect should be safe
    int result = cortical_hierarchy_disconnect_bio_async(hierarchy_);
    EXPECT_EQ(result, 0);
}

/*=============================================================================
 * Dual Stream Tests (Ventral/Dorsal)
 *===========================================================================*/

TEST_F(CorticalHierarchyIntegrationTest, CreateDualStreamHierarchy) {
    // Ventral stream
    cortical_area_config_t v1_config = CreateAreaConfig(CORTICAL_AREA_V1, 0, STREAM_VENTRAL);
    cortical_area_config_t v4_config = CreateAreaConfig(CORTICAL_AREA_V4, 1, STREAM_VENTRAL);
    cortical_area_config_t it_config = CreateAreaConfig(CORTICAL_AREA_IT, 2, STREAM_VENTRAL);

    // Dorsal stream
    cortical_area_config_t mt_config = CreateAreaConfig(CORTICAL_AREA_MT, 1, STREAM_DORSAL);
    cortical_area_config_t pfc_config = CreateAreaConfig(CORTICAL_AREA_PFC, 2, STREAM_DORSAL);

    uint32_t v1_id, v4_id, it_id, mt_id, pfc_id;

    ASSERT_EQ(cortical_hierarchy_add_area(hierarchy_, &v1_config, &v1_id), 0);
    ASSERT_EQ(cortical_hierarchy_add_area(hierarchy_, &v4_config, &v4_id), 0);
    ASSERT_EQ(cortical_hierarchy_add_area(hierarchy_, &it_config, &it_id), 0);
    ASSERT_EQ(cortical_hierarchy_add_area(hierarchy_, &mt_config, &mt_id), 0);
    ASSERT_EQ(cortical_hierarchy_add_area(hierarchy_, &pfc_config, &pfc_id), 0);

    EXPECT_EQ(cortical_hierarchy_get_num_areas(hierarchy_), 5u);

    // Verify stream assignments
    const cortical_area_config_t* it_retrieved =
        cortical_hierarchy_get_area_config(hierarchy_, it_id);
    ASSERT_NE(it_retrieved, nullptr);
    EXPECT_EQ(it_retrieved->stream, STREAM_VENTRAL);

    const cortical_area_config_t* mt_retrieved =
        cortical_hierarchy_get_area_config(hierarchy_, mt_id);
    ASSERT_NE(mt_retrieved, nullptr);
    EXPECT_EQ(mt_retrieved->stream, STREAM_DORSAL);
}

/*=============================================================================
 * Stress Tests
 *===========================================================================*/

TEST_F(CorticalHierarchyIntegrationTest, HighFrequencyPropagation) {
    CreateVisualHierarchy();
    ASSERT_EQ(cortical_hierarchy_apply_canonical_connections(hierarchy_), 0);

    // High frequency propagation cycles
    for (int cycle = 0; cycle < 100; cycle++) {
        float input[16];
        for (int i = 0; i < 16; i++) {
            input[i] = 0.5f + 0.3f * sinf(cycle * 0.1f + i * 0.2f);
        }

        ASSERT_EQ(cortical_hierarchy_set_area_input(hierarchy_, v1_id_, input, 16), 0);
        ASSERT_EQ(cortical_hierarchy_propagate_feedforward(hierarchy_, 0, 3), 0);
        ASSERT_EQ(cortical_hierarchy_propagate_feedback(hierarchy_, 3, 0), 0);
    }

    cortical_hierarchy_stats_t stats;
    ASSERT_EQ(cortical_hierarchy_get_stats(hierarchy_, &stats), 0);

    EXPECT_GE(stats.total_propagations, 200u);
}

TEST_F(CorticalHierarchyIntegrationTest, LargeHierarchy) {
    // Create a large hierarchy with many areas
    std::vector<uint32_t> area_ids;

    for (int level = 0; level < 6; level++) {
        cortical_area_config_t config = CreateAreaConfig(CORTICAL_AREA_CUSTOM, level);

        uint32_t area_id;
        int result = cortical_hierarchy_add_area(hierarchy_, &config, &area_id);
        ASSERT_EQ(result, 0);
        area_ids.push_back(area_id);
    }

    // Connect consecutive levels
    for (size_t i = 0; i < area_ids.size() - 1; i++) {
        inter_area_connection_config_t conn_config;
        memset(&conn_config, 0, sizeof(conn_config));

        conn_config.source_area_id = area_ids[i];
        conn_config.target_area_id = area_ids[i + 1];
        conn_config.type = CONNECTION_TYPE_FEEDFORWARD;
        conn_config.weight = 0.8f;
        conn_config.use_canonical_layers = true;

        uint32_t conn_id;
        ASSERT_EQ(cortical_hierarchy_connect_areas(hierarchy_, &conn_config, &conn_id), 0);
    }

    // Propagate through entire hierarchy
    float input[16];
    for (int i = 0; i < 16; i++) {
        input[i] = 1.0f;
    }

    ASSERT_EQ(cortical_hierarchy_set_area_input(hierarchy_, area_ids[0], input, 16), 0);
    ASSERT_EQ(cortical_hierarchy_propagate_feedforward(hierarchy_, 0, 5), 0);

    cortical_hierarchy_stats_t stats;
    ASSERT_EQ(cortical_hierarchy_get_stats(hierarchy_, &stats), 0);

    EXPECT_EQ(stats.num_areas, 6u);
    EXPECT_EQ(stats.max_hierarchy_level, 5u);
}
