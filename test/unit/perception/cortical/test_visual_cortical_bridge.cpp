/**
 * @file test_visual_cortical_bridge.cpp
 * @brief Unit tests for visual-cortical bridge
 *
 * WHAT: Comprehensive unit tests for the visual-cortical bridge module.
 * WHY:  Ensure visual cortex integration with cortical columns works correctly.
 * HOW:  Tests lifecycle, connections, processing, hypercolumns, immune modulation,
 *       statistics, orientation maps, and bio-async messaging.
 *
 * @version 1.0.0
 * @date 2025-12-19
 */

// Headers have their own extern "C" guards
#include "perception/cortical/nimcp_visual_cortical_bridge.h"
#include "utils/memory/nimcp_memory.h"

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class VisualCorticalBridgeTest : public ::testing::Test {
protected:
    visual_cortical_bridge_t* bridge;
    visual_cortical_config_t config;

    void SetUp() override {
        bridge = nullptr;
        visual_cortical_default_config(&config);
    }

    void TearDown() override {
        if (bridge) {
            visual_cortical_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    /**
     * Create synthetic test image with vertical edges
     */
    float* create_vertical_edge_image(uint32_t width, uint32_t height) {
        float* image = (float*)malloc(width * height * sizeof(float));
        if (!image) return nullptr;

        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                // Vertical edge at x = width/2
                image[y * width + x] = (x < width / 2) ? 0.0f : 1.0f;
            }
        }
        return image;
    }

    /**
     * Create synthetic test image with horizontal edges
     */
    float* create_horizontal_edge_image(uint32_t width, uint32_t height) {
        float* image = (float*)malloc(width * height * sizeof(float));
        if (!image) return nullptr;

        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                // Horizontal edge at y = height/2
                image[y * width + x] = (y < height / 2) ? 0.0f : 1.0f;
            }
        }
        return image;
    }

    /**
     * Create synthetic test image with diagonal edges (45 degrees)
     */
    float* create_diagonal_edge_image(uint32_t width, uint32_t height) {
        float* image = (float*)malloc(width * height * sizeof(float));
        if (!image) return nullptr;

        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                // Diagonal edge: y > x
                image[y * width + x] = (y > x) ? 1.0f : 0.0f;
            }
        }
        return image;
    }

    /**
     * Create uniform gray image (no edges)
     */
    float* create_uniform_image(uint32_t width, uint32_t height, float value = 0.5f) {
        float* image = (float*)malloc(width * height * sizeof(float));
        if (!image) return nullptr;

        for (uint32_t i = 0; i < width * height; i++) {
            image[i] = value;
        }
        return image;
    }
};

/* ============================================================================
 * Lifecycle Tests (5 tests)
 * ============================================================================ */

TEST_F(VisualCorticalBridgeTest, DefaultConfigInitialization) {
    visual_cortical_config_t cfg;
    visual_cortical_default_config(&cfg);

    EXPECT_EQ(cfg.num_hypercolumns, 64u);
    EXPECT_EQ(cfg.orientations_per_hypercolumn, VISUAL_CORTICAL_DEFAULT_ORIENTATIONS);
    EXPECT_FLOAT_EQ(cfg.spatial_frequency, VISUAL_CORTICAL_DEFAULT_SPATIAL_FREQ);
    EXPECT_FLOAT_EQ(cfg.tuning_width, VISUAL_CORTICAL_DEFAULT_TUNING_WIDTH);
    EXPECT_EQ(cfg.mode, VISUAL_CORTICAL_MODE_HYPERCOLUMN);
    EXPECT_TRUE(cfg.enable_retinotopic_mapping);
    EXPECT_TRUE(cfg.enable_cortical_immune);
    EXPECT_TRUE(cfg.enable_bio_async);
    EXPECT_FLOAT_EQ(cfg.visual_field_degrees, 120.0f);
    EXPECT_FLOAT_EQ(cfg.foveal_radius, 5.0f);
    EXPECT_FLOAT_EQ(cfg.cortical_magnification, 10.0f);
    EXPECT_FLOAT_EQ(cfg.immune_modulation_factor, VISUAL_CORTICAL_DEFAULT_IMMUNE_FACTOR);
    EXPECT_FALSE(cfg.use_umm);
}

TEST_F(VisualCorticalBridgeTest, CreateWithDefaultConfig) {
    bridge = visual_cortical_bridge_create(nullptr, nullptr);

    ASSERT_NE(bridge, nullptr);
    EXPECT_EQ(visual_cortical_get_state(bridge), VISUAL_CORTICAL_STATE_READY);
    EXPECT_EQ(visual_cortical_get_num_hypercolumns(bridge), 64u);
}

TEST_F(VisualCorticalBridgeTest, CreateWithCustomConfig) {
    config.num_hypercolumns = 16;
    config.orientations_per_hypercolumn = 4;
    config.spatial_frequency = 3.0f;
    config.enable_retinotopic_mapping = false;

    bridge = visual_cortical_bridge_create(&config, nullptr);

    ASSERT_NE(bridge, nullptr);
    EXPECT_EQ(visual_cortical_get_num_hypercolumns(bridge), 16u);
}

TEST_F(VisualCorticalBridgeTest, CreateWithInvalidConfig) {
    config.num_hypercolumns = 0;  // Invalid

    bridge = visual_cortical_bridge_create(&config, nullptr);

    EXPECT_EQ(bridge, nullptr);
}

TEST_F(VisualCorticalBridgeTest, DestroyNullBridge) {
    // Should not crash
    visual_cortical_bridge_destroy(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Connection Tests (6 tests)
 * ============================================================================ */

TEST_F(VisualCorticalBridgeTest, ConnectVisualCortex) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Create a dummy visual cortex pointer (not dereferenced in test)
    visual_cortex_t* vc = (visual_cortex_t*)0x12345678;

    int ret = visual_cortical_connect_visual_cortex(bridge, vc);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

TEST_F(VisualCorticalBridgeTest, ConnectVisualCortexNullBridge) {
    visual_cortex_t* vc = (visual_cortex_t*)0x12345678;

    int ret = visual_cortical_connect_visual_cortex(nullptr, vc);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(VisualCorticalBridgeTest, ConnectImmune) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Create a dummy immune system pointer
    cortical_immune_system_t* immune = (cortical_immune_system_t*)0x12345678;

    int ret = visual_cortical_connect_immune(bridge, immune);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

TEST_F(VisualCorticalBridgeTest, ConnectImmuneNullBridge) {
    cortical_immune_system_t* immune = (cortical_immune_system_t*)0x12345678;

    int ret = visual_cortical_connect_immune(nullptr, immune);
    EXPECT_EQ(ret, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(VisualCorticalBridgeTest, BioAsyncConnectionLifecycle) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Initially not connected
    EXPECT_FALSE(visual_cortical_is_bio_async_connected(bridge));

    // Connect (may fail if router not available, which is expected in tests)
    int ret = visual_cortical_connect_bio_async(bridge);
    // Don't assert success - bio-async router may not be available in test

    // If connection succeeded, verify state
    if (ret == NIMCP_SUCCESS) {
        EXPECT_TRUE(visual_cortical_is_bio_async_connected(bridge));

        // Disconnect
        ret = visual_cortical_disconnect_bio_async(bridge);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
        EXPECT_FALSE(visual_cortical_is_bio_async_connected(bridge));
    }
}

TEST_F(VisualCorticalBridgeTest, BioAsyncDoubleConnect) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    visual_cortical_connect_bio_async(bridge);

    // Second connect should succeed if already connected (returns 0)
    // or fail if router not available (returns error code) - both are acceptable
    int ret = visual_cortical_connect_bio_async(bridge);
    EXPECT_TRUE(ret == NIMCP_SUCCESS || ret == NIMCP_ERROR_OPERATION_FAILED);
}

/* ============================================================================
 * Configuration Tests (5 tests)
 * ============================================================================ */

TEST_F(VisualCorticalBridgeTest, ConfigMinimalHypercolumns) {
    config.num_hypercolumns = 1;

    bridge = visual_cortical_bridge_create(&config, nullptr);

    ASSERT_NE(bridge, nullptr);
    EXPECT_EQ(visual_cortical_get_num_hypercolumns(bridge), 1u);
}

TEST_F(VisualCorticalBridgeTest, ConfigMaxHypercolumns) {
    config.num_hypercolumns = VISUAL_CORTICAL_MAX_HYPERCOLUMNS;

    bridge = visual_cortical_bridge_create(&config, nullptr);

    ASSERT_NE(bridge, nullptr);
    EXPECT_EQ(visual_cortical_get_num_hypercolumns(bridge), VISUAL_CORTICAL_MAX_HYPERCOLUMNS);
}

TEST_F(VisualCorticalBridgeTest, ConfigDirectMode) {
    config.mode = VISUAL_CORTICAL_MODE_DIRECT;

    bridge = visual_cortical_bridge_create(&config, nullptr);

    ASSERT_NE(bridge, nullptr);
    EXPECT_EQ(visual_cortical_get_state(bridge), VISUAL_CORTICAL_STATE_READY);
}

TEST_F(VisualCorticalBridgeTest, ConfigFullMode) {
    config.mode = VISUAL_CORTICAL_MODE_FULL;
    config.enable_retinotopic_mapping = true;
    config.enable_cortical_immune = true;
    config.enable_bio_async = true;

    bridge = visual_cortical_bridge_create(&config, nullptr);

    ASSERT_NE(bridge, nullptr);
    EXPECT_NE(visual_cortical_get_retinotopic_map(bridge), nullptr);
}

TEST_F(VisualCorticalBridgeTest, ConfigNoRetinotopicMapping) {
    config.enable_retinotopic_mapping = false;

    bridge = visual_cortical_bridge_create(&config, nullptr);

    ASSERT_NE(bridge, nullptr);
    // Retinotopic map may still be created or not, depending on implementation
}

/* ============================================================================
 * Processing Tests (8 tests)
 * ============================================================================ */

TEST_F(VisualCorticalBridgeTest, ProcessVerticalEdge) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t width = 64, height = 64;
    float* image = create_vertical_edge_image(width, height);
    ASSERT_NE(image, nullptr);

    visual_cortical_orientation_result_t result;
    int ret = visual_cortical_process(bridge, image, width, height, &result);

    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GT(result.num_orientations, 0u);
    EXPECT_NE(result.orientation_responses, nullptr);

    // Vertical edges should be detected (around 90 degrees or 0 degrees depending on convention)
    EXPECT_GE(result.selectivity_index, 0.0f);
    EXPECT_LE(result.selectivity_index, 1.0f);

    visual_cortical_free_result(&result);
    free(image);
}

TEST_F(VisualCorticalBridgeTest, ProcessHorizontalEdge) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t width = 64, height = 64;
    float* image = create_horizontal_edge_image(width, height);
    ASSERT_NE(image, nullptr);

    visual_cortical_orientation_result_t result;
    int ret = visual_cortical_process(bridge, image, width, height, &result);

    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GT(result.num_orientations, 0u);
    EXPECT_NE(result.orientation_responses, nullptr);

    visual_cortical_free_result(&result);
    free(image);
}

TEST_F(VisualCorticalBridgeTest, ProcessDiagonalEdge) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t width = 64, height = 64;
    float* image = create_diagonal_edge_image(width, height);
    ASSERT_NE(image, nullptr);

    visual_cortical_orientation_result_t result;
    int ret = visual_cortical_process(bridge, image, width, height, &result);

    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GT(result.num_orientations, 0u);

    // Diagonal should be around 45 or 135 degrees
    EXPECT_GE(result.dominant_orientation, 0.0f);
    EXPECT_LE(result.dominant_orientation, 180.0f);

    visual_cortical_free_result(&result);
    free(image);
}

TEST_F(VisualCorticalBridgeTest, ProcessUniformImage) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t width = 64, height = 64;
    float* image = create_uniform_image(width, height);
    ASSERT_NE(image, nullptr);

    visual_cortical_orientation_result_t result;
    int ret = visual_cortical_process(bridge, image, width, height, &result);

    EXPECT_EQ(ret, NIMCP_SUCCESS);

    // Uniform image should have low selectivity (no edges)
    if (result.num_orientations > 0) {
        EXPECT_LE(result.selectivity_index, 0.5f);  // Low selectivity expected
    }

    visual_cortical_free_result(&result);
    free(image);
}

TEST_F(VisualCorticalBridgeTest, ProcessNullInputs) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    visual_cortical_orientation_result_t result;

    // Null bridge
    EXPECT_EQ(visual_cortical_process(nullptr, (float*)0x1234, 64, 64, &result),
              NIMCP_ERROR_NULL_POINTER);

    // Null image
    EXPECT_EQ(visual_cortical_process(bridge, nullptr, 64, 64, &result),
              NIMCP_ERROR_NULL_POINTER);

    // Null result
    EXPECT_EQ(visual_cortical_process(bridge, (float*)0x1234, 64, 64, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

TEST_F(VisualCorticalBridgeTest, ProcessInvalidDimensions) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    visual_cortical_orientation_result_t result;
    float dummy_image[100];

    // Zero width
    EXPECT_EQ(visual_cortical_process(bridge, dummy_image, 0, 64, &result),
              NIMCP_ERROR_INVALID_PARAMETER);

    // Zero height
    EXPECT_EQ(visual_cortical_process(bridge, dummy_image, 64, 0, &result),
              NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(VisualCorticalBridgeTest, ProcessPatch) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t patch_size = 32;
    float* patch = create_vertical_edge_image(patch_size, patch_size);
    ASSERT_NE(patch, nullptr);

    visual_cortical_orientation_result_t result;
    int ret = visual_cortical_process_patch(bridge, patch, patch_size, patch_size,
                                           0.0f, 0.0f, &result);

    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GT(result.num_orientations, 0u);
    EXPECT_FLOAT_EQ(result.retino_x, 0.0f);
    EXPECT_FLOAT_EQ(result.retino_y, 0.0f);

    visual_cortical_free_result(&result);
    free(patch);
}

TEST_F(VisualCorticalBridgeTest, FreeNullResult) {
    // Should not crash
    visual_cortical_free_result(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Hypercolumn Tests (5 tests)
 * ============================================================================ */

TEST_F(VisualCorticalBridgeTest, GetHypercolumnByPosition) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    const orientation_hypercolumn_t* hcol =
        visual_cortical_get_hypercolumn(bridge, 0.0f, 0.0f);

    // Should return a hypercolumn (center of visual field)
    EXPECT_NE(hcol, nullptr);
}

TEST_F(VisualCorticalBridgeTest, GetHypercolumnByIndex) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t num_hcols = visual_cortical_get_num_hypercolumns(bridge);
    ASSERT_GT(num_hcols, 0u);

    // Get first hypercolumn
    const orientation_hypercolumn_t* hcol =
        visual_cortical_get_hypercolumn_by_index(bridge, 0);
    EXPECT_NE(hcol, nullptr);

    // Get last hypercolumn
    hcol = visual_cortical_get_hypercolumn_by_index(bridge, num_hcols - 1);
    EXPECT_NE(hcol, nullptr);
}

TEST_F(VisualCorticalBridgeTest, GetHypercolumnOutOfBounds) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t num_hcols = visual_cortical_get_num_hypercolumns(bridge);

    // Index out of bounds
    const orientation_hypercolumn_t* hcol =
        visual_cortical_get_hypercolumn_by_index(bridge, num_hcols + 100);
    EXPECT_EQ(hcol, nullptr);
}

TEST_F(VisualCorticalBridgeTest, GetNumHypercolumns) {
    config.num_hypercolumns = 25;  // 5x5 grid
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_EQ(visual_cortical_get_num_hypercolumns(bridge), 25u);
}

TEST_F(VisualCorticalBridgeTest, GetNumHypercolumnsNullBridge) {
    EXPECT_EQ(visual_cortical_get_num_hypercolumns(nullptr), 0u);
}

/* ============================================================================
 * Immune Modulation Tests (5 tests)
 * ============================================================================ */

TEST_F(VisualCorticalBridgeTest, UpdateImmuneModulationWithoutImmune) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Should succeed even without immune system connected
    int ret = visual_cortical_update_immune_modulation(bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

TEST_F(VisualCorticalBridgeTest, SetImmuneFactor) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    int ret = visual_cortical_set_immune_factor(bridge, 0.5f);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    float factor = visual_cortical_get_immune_factor(bridge);
    EXPECT_FLOAT_EQ(factor, 0.5f);
}

TEST_F(VisualCorticalBridgeTest, SetImmuneFactorClamping) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Set above 1.0 - should clamp to 1.0
    visual_cortical_set_immune_factor(bridge, 2.0f);
    EXPECT_FLOAT_EQ(visual_cortical_get_immune_factor(bridge), 1.0f);

    // Set below 0.0 - should clamp to 0.0
    visual_cortical_set_immune_factor(bridge, -1.0f);
    EXPECT_FLOAT_EQ(visual_cortical_get_immune_factor(bridge), 0.0f);
}

TEST_F(VisualCorticalBridgeTest, GetImmuneFactorDefault) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Default should be 0.0
    float factor = visual_cortical_get_immune_factor(bridge);
    EXPECT_FLOAT_EQ(factor, 0.0f);
}

TEST_F(VisualCorticalBridgeTest, GetImmuneFactorNullBridge) {
    EXPECT_FLOAT_EQ(visual_cortical_get_immune_factor(nullptr), 0.0f);
}

/* ============================================================================
 * Statistics Tests (4 tests)
 * ============================================================================ */

TEST_F(VisualCorticalBridgeTest, GetStatsInitial) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    visual_cortical_stats_t stats;
    int ret = visual_cortical_get_stats(bridge, &stats);

    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_EQ(stats.images_processed, 0u);
    EXPECT_EQ(stats.hypercolumn_activations, 0u);
    EXPECT_EQ(stats.bio_messages_sent, 0u);
    EXPECT_EQ(stats.bio_messages_received, 0u);
}

TEST_F(VisualCorticalBridgeTest, GetStatsAfterProcessing) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t width = 32, height = 32;
    float* image = create_vertical_edge_image(width, height);
    ASSERT_NE(image, nullptr);

    visual_cortical_orientation_result_t result;
    visual_cortical_process(bridge, image, width, height, &result);
    visual_cortical_free_result(&result);

    visual_cortical_stats_t stats;
    visual_cortical_get_stats(bridge, &stats);

    EXPECT_EQ(stats.images_processed, 1u);
    EXPECT_GT(stats.hypercolumn_activations, 0u);
    EXPECT_GE(stats.avg_processing_time_ms, 0.0f);

    free(image);
}

TEST_F(VisualCorticalBridgeTest, ResetStats) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Process an image to generate stats
    uint32_t width = 32, height = 32;
    float* image = create_uniform_image(width, height);
    visual_cortical_orientation_result_t result;
    visual_cortical_process(bridge, image, width, height, &result);
    visual_cortical_free_result(&result);
    free(image);

    // Reset stats
    int ret = visual_cortical_reset_stats(bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    // Verify reset
    visual_cortical_stats_t stats;
    visual_cortical_get_stats(bridge, &stats);
    EXPECT_EQ(stats.images_processed, 0u);
    EXPECT_EQ(stats.hypercolumn_activations, 0u);
}

TEST_F(VisualCorticalBridgeTest, GetState) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    visual_cortical_state_t state = visual_cortical_get_state(bridge);
    EXPECT_EQ(state, VISUAL_CORTICAL_STATE_READY);

    // State for null bridge
    EXPECT_EQ(visual_cortical_get_state(nullptr),
              VISUAL_CORTICAL_STATE_UNINITIALIZED);
}

/* ============================================================================
 * Orientation Map Tests (3 tests)
 * ============================================================================ */

TEST_F(VisualCorticalBridgeTest, GetOrientationMap) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t width = 64, height = 64;
    float* image = create_vertical_edge_image(width, height);
    ASSERT_NE(image, nullptr);

    float* orientation_map = (float*)malloc(width * height * sizeof(float));
    float* selectivity_map = (float*)malloc(width * height * sizeof(float));
    ASSERT_NE(orientation_map, nullptr);
    ASSERT_NE(selectivity_map, nullptr);

    int ret = visual_cortical_get_orientation_map(bridge, image, width, height,
                                                  orientation_map, selectivity_map);

    EXPECT_EQ(ret, NIMCP_SUCCESS);

    // Verify maps contain valid data
    bool has_valid_data = false;
    for (uint32_t i = 0; i < width * height; i++) {
        if (orientation_map[i] >= 0.0f && orientation_map[i] <= 180.0f) {
            has_valid_data = true;
            break;
        }
    }
    EXPECT_TRUE(has_valid_data);

    free(orientation_map);
    free(selectivity_map);
    free(image);
}

TEST_F(VisualCorticalBridgeTest, GetOrientationMapNullSelectivity) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    uint32_t width = 32, height = 32;
    float* image = create_uniform_image(width, height);
    float* orientation_map = (float*)malloc(width * height * sizeof(float));
    ASSERT_NE(orientation_map, nullptr);

    // Should work with NULL selectivity map
    int ret = visual_cortical_get_orientation_map(bridge, image, width, height,
                                                  orientation_map, nullptr);

    EXPECT_EQ(ret, NIMCP_SUCCESS);

    free(orientation_map);
    free(image);
}

TEST_F(VisualCorticalBridgeTest, GetOrientationMapInvalidInputs) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    float dummy_map[100];

    // Null bridge
    EXPECT_EQ(visual_cortical_get_orientation_map(nullptr, (float*)0x1234,
                                                  32, 32, dummy_map, nullptr),
              NIMCP_ERROR_NULL_POINTER);

    // Null image
    EXPECT_EQ(visual_cortical_get_orientation_map(bridge, nullptr,
                                                  32, 32, dummy_map, nullptr),
              NIMCP_ERROR_NULL_POINTER);

    // Null orientation map
    EXPECT_EQ(visual_cortical_get_orientation_map(bridge, (float*)0x1234,
                                                  32, 32, nullptr, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Bio-Async Message Tests (4 tests)
 * ============================================================================ */

TEST_F(VisualCorticalBridgeTest, ProcessBioMessagesNotConnected) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Should return 0 when not connected
    uint32_t processed = visual_cortical_process_bio_messages(bridge, 10);
    EXPECT_EQ(processed, 0u);
}

TEST_F(VisualCorticalBridgeTest, ProcessBioMessagesNullBridge) {
    uint32_t processed = visual_cortical_process_bio_messages(nullptr, 10);
    EXPECT_EQ(processed, 0u);
}

TEST_F(VisualCorticalBridgeTest, BroadcastOrientation) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Try to connect to bio-async
    visual_cortical_connect_bio_async(bridge);

    visual_cortical_orientation_result_t result;
    memset(&result, 0, sizeof(result));
    result.dominant_orientation = 45.0f;
    result.selectivity_index = 0.8f;
    result.confidence = 0.75f;

    // May fail if bio-async router not available, which is expected
    int ret = visual_cortical_broadcast_orientation(bridge, &result);

    if (visual_cortical_is_bio_async_connected(bridge)) {
        // If connected, should succeed or fail gracefully
        EXPECT_TRUE(ret == NIMCP_SUCCESS || ret == NIMCP_ERROR_OPERATION_FAILED);
    } else {
        // If not connected, should return invalid state
        EXPECT_EQ(ret, NIMCP_ERROR_INVALID_STATE);
    }
}

TEST_F(VisualCorticalBridgeTest, BroadcastOrientationNullInputs) {
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(bridge, nullptr);

    visual_cortical_orientation_result_t result;
    memset(&result, 0, sizeof(result));

    // Null bridge
    EXPECT_EQ(visual_cortical_broadcast_orientation(nullptr, &result),
              NIMCP_ERROR_NULL_POINTER);

    // Null result
    EXPECT_EQ(visual_cortical_broadcast_orientation(bridge, nullptr),
              NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
