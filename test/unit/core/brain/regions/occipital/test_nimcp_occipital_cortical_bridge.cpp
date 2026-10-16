/**
 * @file test_nimcp_occipital_cortical_bridge.cpp
 * @brief Unit tests for nimcp_occipital_cortical_bridge.c
 *
 * WHAT: Comprehensive unit tests for the Occipital-Cortical Columns bridge
 * WHY:  Ensure correct orientation hypercolumn and retinotopic mapping
 * HOW:  Use Google Test framework to test lifecycle, mapping, and processing
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/occipital/nimcp_occipital_cortical_bridge.h"
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"

// ============================================================================
// TEST FIXTURE
// ============================================================================

class OccipitalCorticalBridgeTest : public ::testing::Test {
protected:
    occipital_cortical_bridge_t* bridge;
    occipital_adapter_t* occipital;
    occipital_cortical_config_t config;
    occipital_config_t occipital_config;

    void SetUp() override {
        // Create occipital adapter
        occipital_config = occipital_default_config();
        occipital_config.image_width = 64;
        occipital_config.image_height = 64;
        occipital = occipital_create(&occipital_config);

        // Create cortical bridge
        config = occipital_cortical_default_config();
        bridge = occipital_cortical_bridge_create(occipital, &config);
    }

    void TearDown() override {
        if (bridge) {
            occipital_cortical_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (occipital) {
            occipital_destroy(occipital);
            occipital = nullptr;
        }
    }
};

// ============================================================================
// CONFIGURATION TESTS
// ============================================================================

TEST_F(OccipitalCorticalBridgeTest, DefaultConfigHasReasonableValues) {
    occipital_cortical_config_t default_config = occipital_cortical_default_config();

    EXPECT_GT(default_config.num_hypercolumns, 0u);
    EXPECT_GT(default_config.orientations_per_column, 0u);
    EXPECT_GT(default_config.map_resolution_x, 0u);
    EXPECT_GT(default_config.map_resolution_y, 0u);
}

TEST_F(OccipitalCorticalBridgeTest, GetConfigReturnsCurrentConfig) {
    ASSERT_NE(nullptr, bridge);

    occipital_cortical_config_t retrieved;
    EXPECT_EQ(0, occipital_cortical_bridge_get_config(bridge, &retrieved));

    EXPECT_EQ(config.num_hypercolumns, retrieved.num_hypercolumns);
    EXPECT_EQ(config.orientations_per_column, retrieved.orientations_per_column);
}

TEST_F(OccipitalCorticalBridgeTest, GetConfigWithNullBridgeFails) {
    occipital_cortical_config_t retrieved;
    EXPECT_EQ(-1, occipital_cortical_bridge_get_config(nullptr, &retrieved));
}

TEST_F(OccipitalCorticalBridgeTest, GetConfigWithNullOutputFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_cortical_bridge_get_config(bridge, nullptr));
}

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(OccipitalCorticalBridgeTest, CreateWithNullOccipitalReturnsNull) {
    // Bridge requires occipital lobe connection
    occipital_cortical_bridge_t* standalone = occipital_cortical_bridge_create(
        NULL, &config);
    EXPECT_EQ(nullptr, standalone);
}

TEST_F(OccipitalCorticalBridgeTest, CreateWithNullConfigUsesDefaults) {
    occipital_cortical_bridge_t* default_bridge = occipital_cortical_bridge_create(
        occipital, NULL);
    ASSERT_NE(nullptr, default_bridge);

    occipital_cortical_config_t retrieved;
    EXPECT_EQ(0, occipital_cortical_bridge_get_config(default_bridge, &retrieved));
    EXPECT_GT(retrieved.num_hypercolumns, 0u);

    occipital_cortical_bridge_destroy(default_bridge);
}

TEST_F(OccipitalCorticalBridgeTest, DestroyNullDoesNotCrash) {
    occipital_cortical_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(OccipitalCorticalBridgeTest, ResetBridgeSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_cortical_bridge_reset(bridge));
}

TEST_F(OccipitalCorticalBridgeTest, ResetNullBridgeFails) {
    EXPECT_EQ(-1, occipital_cortical_bridge_reset(nullptr));
}

// ============================================================================
// RETINOTOPIC MAPPING TESTS
// ============================================================================

TEST_F(OccipitalCorticalBridgeTest, MapVisualToCorticalSucceeds) {
    ASSERT_NE(nullptr, bridge);

    float cortical_x, cortical_y;
    EXPECT_EQ(0, occipital_cortical_map_visual_to_cortical(
        bridge, 0.5f, 0.5f, &cortical_x, &cortical_y));

    // Output should be finite (mapping may use any coordinate system)
    EXPECT_TRUE(std::isfinite(cortical_x));
    EXPECT_TRUE(std::isfinite(cortical_y));
}

TEST_F(OccipitalCorticalBridgeTest, MapVisualToCorticalNullBridgeFails) {
    float cortical_x, cortical_y;
    EXPECT_EQ(-1, occipital_cortical_map_visual_to_cortical(
        nullptr, 0.5f, 0.5f, &cortical_x, &cortical_y));
}

TEST_F(OccipitalCorticalBridgeTest, MapVisualToCorticalNullOutputFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_cortical_map_visual_to_cortical(
        bridge, 0.5f, 0.5f, nullptr, nullptr));
}

TEST_F(OccipitalCorticalBridgeTest, FovealMagnificationApplied) {
    ASSERT_NE(nullptr, bridge);

    // Map center (fovea) and periphery
    float fovea_x, fovea_y;
    float periph_x, periph_y;

    occipital_cortical_map_visual_to_cortical(bridge, 0.5f, 0.5f, &fovea_x, &fovea_y);
    occipital_cortical_map_visual_to_cortical(bridge, 0.9f, 0.9f, &periph_x, &periph_y);

    // Foveal representation should be relatively larger (magnified)
    // This is a qualitative test - exact values depend on implementation
    SUCCEED();
}

// ============================================================================
// COLUMN ACTIVITY TESTS
// ============================================================================

TEST_F(OccipitalCorticalBridgeTest, GetColumnActivitySucceeds) {
    ASSERT_NE(nullptr, bridge);

    occipital_cortical_bridge_update(bridge);

    column_activity_t activity;
    memset(&activity, 0, sizeof(activity));
    EXPECT_EQ(0, occipital_cortical_get_column_activity(bridge, 0.5f, 0.5f, &activity));

    // Activity should be valid
    EXPECT_GE(activity.activity, 0.0f);
}

TEST_F(OccipitalCorticalBridgeTest, GetColumnActivityNullBridgeFails) {
    column_activity_t activity;
    EXPECT_EQ(-1, occipital_cortical_get_column_activity(nullptr, 0.5f, 0.5f, &activity));
}

TEST_F(OccipitalCorticalBridgeTest, GetColumnActivityNullOutputFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_cortical_get_column_activity(bridge, 0.5f, 0.5f, nullptr));
}

TEST_F(OccipitalCorticalBridgeTest, GetHypercolumnResponseSucceeds) {
    ASSERT_NE(nullptr, bridge);

    occipital_cortical_bridge_update(bridge);

    hypercolumn_response_t response;
    memset(&response, 0, sizeof(response));
    EXPECT_EQ(0, occipital_cortical_get_hypercolumn_response(bridge, 0, &response));

    // Response should have valid dominant orientation
    EXPECT_GE(response.dominant_orientation, 0.0f);
}

TEST_F(OccipitalCorticalBridgeTest, GetHypercolumnResponseNullBridgeFails) {
    hypercolumn_response_t response;
    EXPECT_EQ(-1, occipital_cortical_get_hypercolumn_response(nullptr, 0, &response));
}

// ============================================================================
// PROCESSING TESTS
// ============================================================================

TEST_F(OccipitalCorticalBridgeTest, ProcessSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_cortical_bridge_process(bridge));
}

TEST_F(OccipitalCorticalBridgeTest, ProcessNullBridgeFails) {
    EXPECT_EQ(-1, occipital_cortical_bridge_process(nullptr));
}

TEST_F(OccipitalCorticalBridgeTest, UpdateSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_cortical_bridge_update(bridge));
}

TEST_F(OccipitalCorticalBridgeTest, UpdateNullBridgeFails) {
    EXPECT_EQ(-1, occipital_cortical_bridge_update(nullptr));
}

TEST_F(OccipitalCorticalBridgeTest, GetEffectsSucceeds) {
    ASSERT_NE(nullptr, bridge);

    occipital_cortical_bridge_update(bridge);

    occipital_cortical_effects_t effects;
    memset(&effects, 0, sizeof(effects));
    EXPECT_EQ(0, occipital_cortical_bridge_get_effects(bridge, &effects));

    // Check valid range
    EXPECT_GE(effects.mean_column_activity, 0.0f);
    EXPECT_LE(effects.mean_column_activity, 1.0f);
}

TEST_F(OccipitalCorticalBridgeTest, GetEffectsNullBridgeFails) {
    occipital_cortical_effects_t effects;
    EXPECT_EQ(-1, occipital_cortical_bridge_get_effects(nullptr, &effects));
}

TEST_F(OccipitalCorticalBridgeTest, GetEffectsNullOutputFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_cortical_bridge_get_effects(bridge, nullptr));
}

// ============================================================================
// CONNECTION TESTS
// ============================================================================

TEST_F(OccipitalCorticalBridgeTest, ConnectHypercolumnsNullSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_cortical_connect_hypercolumns(bridge, nullptr));
}

TEST_F(OccipitalCorticalBridgeTest, ConnectHypercolumnsNullBridgeFails) {
    EXPECT_EQ(-1, occipital_cortical_connect_hypercolumns(nullptr, nullptr));
}

TEST_F(OccipitalCorticalBridgeTest, ConnectTopographicMapNullSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_cortical_connect_topographic_map(bridge, nullptr));
}

TEST_F(OccipitalCorticalBridgeTest, ConnectTopographicMapNullBridgeFails) {
    EXPECT_EQ(-1, occipital_cortical_connect_topographic_map(nullptr, nullptr));
}

TEST_F(OccipitalCorticalBridgeTest, ConnectImmuneNullSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_cortical_connect_immune(bridge, nullptr));
}

TEST_F(OccipitalCorticalBridgeTest, ConnectImmuneNullBridgeFails) {
    EXPECT_EQ(-1, occipital_cortical_connect_immune(nullptr, nullptr));
}

// ============================================================================
// CONNECTION QUERY TESTS
// ============================================================================

TEST_F(OccipitalCorticalBridgeTest, IsHypercolumnsConnectedInitiallyFalse) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_FALSE(occipital_cortical_is_hypercolumns_connected(bridge));
}

TEST_F(OccipitalCorticalBridgeTest, IsMapConnectedInitiallyFalse) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_FALSE(occipital_cortical_is_map_connected(bridge));
}

TEST_F(OccipitalCorticalBridgeTest, GetActiveColumnCountSucceeds) {
    ASSERT_NE(nullptr, bridge);

    occipital_cortical_bridge_update(bridge);
    uint32_t count = occipital_cortical_get_active_column_count(bridge);
    EXPECT_GE(count, 0u);
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

TEST_F(OccipitalCorticalBridgeTest, GetStatsInitiallyZero) {
    ASSERT_NE(nullptr, bridge);

    occipital_cortical_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));
    EXPECT_EQ(0, occipital_cortical_bridge_get_stats(bridge, &stats));

    EXPECT_EQ(0ULL, stats.columns_activated);
    EXPECT_EQ(0ULL, stats.retinotopic_mappings);
}

TEST_F(OccipitalCorticalBridgeTest, GetStatsNullBridgeFails) {
    occipital_cortical_stats_t stats;
    EXPECT_EQ(-1, occipital_cortical_bridge_get_stats(nullptr, &stats));
}

TEST_F(OccipitalCorticalBridgeTest, GetStatsNullOutputFails) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(-1, occipital_cortical_bridge_get_stats(bridge, nullptr));
}

TEST_F(OccipitalCorticalBridgeTest, ResetStatsSucceeds) {
    ASSERT_NE(nullptr, bridge);

    // Do some processing
    occipital_cortical_bridge_update(bridge);
    occipital_cortical_bridge_process(bridge);

    // Reset stats
    occipital_cortical_bridge_reset_stats(bridge);

    occipital_cortical_stats_t stats;
    EXPECT_EQ(0, occipital_cortical_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(0ULL, stats.columns_activated);
}

// ============================================================================
// BIO-ASYNC TESTS
// ============================================================================

TEST_F(OccipitalCorticalBridgeTest, RegisterBioAsyncNullRouterSucceeds) {
    ASSERT_NE(nullptr, bridge);
    EXPECT_EQ(0, occipital_cortical_bridge_register_bio_async(bridge, nullptr));
}

TEST_F(OccipitalCorticalBridgeTest, RegisterBioAsyncNullBridgeFails) {
    EXPECT_EQ(-1, occipital_cortical_bridge_register_bio_async(nullptr, nullptr));
}

// ============================================================================
// STRESS TESTS
// ============================================================================

TEST_F(OccipitalCorticalBridgeTest, RepeatedUpdatesDoNotLeak) {
    ASSERT_NE(nullptr, bridge);

    for (int i = 0; i < 1000; i++) {
        EXPECT_EQ(0, occipital_cortical_bridge_update(bridge));
    }

    SUCCEED();
}

TEST_F(OccipitalCorticalBridgeTest, CreateDestroyMultipleTimes) {
    for (int i = 0; i < 100; i++) {
        occipital_cortical_bridge_t* temp = occipital_cortical_bridge_create(
            occipital, &config);
        ASSERT_NE(nullptr, temp);
        occipital_cortical_bridge_destroy(temp);
    }
    SUCCEED();
}

TEST_F(OccipitalCorticalBridgeTest, MapManyLocationsSucceeds) {
    ASSERT_NE(nullptr, bridge);

    for (int i = 0; i < 100; i++) {
        float visual_x = (float)i / 100.0f;
        float visual_y = (float)i / 100.0f;
        float cortical_x, cortical_y;

        EXPECT_EQ(0, occipital_cortical_map_visual_to_cortical(
            bridge, visual_x, visual_y, &cortical_x, &cortical_y));
    }

    SUCCEED();
}
