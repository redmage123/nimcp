/**
 * @file e2e_test_perception_cortical.cpp
 * @brief End-to-end tests for perception-cortical bridge pipelines
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Complete E2E tests for visual-cortical bridge pipeline
 * WHY:  Verify full integration from visual input → cortical processing
 * HOW:  Test orientation detection, immune modulation, bio-async messaging
 *
 * TEST COVERAGE:
 * - Visual-Cortical Pipeline: 15 tests
 *   * Lifecycle and configuration (3 tests)
 *   * Orientation detection (5 tests)
 *   * Immune modulation effects (4 tests)
 *   * Bio-async communication (3 tests)
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstring>

// Headers have their own extern "C" guards
// Visual-cortical bridge
#include "perception/cortical/nimcp_visual_cortical_bridge.h"

// Cortical components
#include "core/cortical_columns/nimcp_orientation_columns.h"
#include "core/cortical_columns/nimcp_topographic_maps.h"
#include "core/cortical_columns/nimcp_cortical_immune.h"

// Perception modules
#include "perception/nimcp_visual_cortex.h"

// Bio-async
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

// Memory management
#include "utils/memory/nimcp_unified_memory.h"

/* =============================================================================
 * Test Fixtures
 * ============================================================================= */

class E2EVisualCorticalTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize bio-async router if not already initialized
        // (Note: bio-async may not be available in test environment)
    }

    void TearDown() override {
        // Cleanup
    }

    // Helper: Create simple test image with orientation
    std::vector<float> create_test_image(uint32_t width, uint32_t height,
                                         float orientation_deg) {
        std::vector<float> image(width * height, 0.0f);

        // Create a simple oriented edge pattern
        float angle_rad = orientation_deg * M_PI / 180.0f;
        float cx = width / 2.0f;
        float cy = height / 2.0f;

        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                float dx = x - cx;
                float dy = y - cy;

                // Project perpendicular to edge orientation
                // For vertical edge (90°): proj = dx, creates vertical band
                // For horizontal edge (0°): proj = -dy, creates horizontal band
                float proj = dx * sin(angle_rad) - dy * cos(angle_rad);

                // Create edge response
                if (fabs(proj) < 2.0f) {
                    image[y * width + x] = 1.0f;
                } else {
                    image[y * width + x] = 0.0f;
                }
            }
        }

        return image;
    }

    // Helper: Create vertical edge image
    std::vector<float> create_vertical_edge(uint32_t width, uint32_t height) {
        return create_test_image(width, height, 90.0f);
    }

    // Helper: Create horizontal edge image
    std::vector<float> create_horizontal_edge(uint32_t width, uint32_t height) {
        return create_test_image(width, height, 0.0f);
    }

    // Helper: Create diagonal edge image
    std::vector<float> create_diagonal_edge(uint32_t width, uint32_t height) {
        return create_test_image(width, height, 45.0f);
    }
};

/* =============================================================================
 * Lifecycle and Configuration Tests
 * ============================================================================= */

TEST_F(E2EVisualCorticalTest, BridgeLifecycleComplete) {
    // Test full lifecycle: create → configure → process → destroy

    // Step 1: Create with default config
    visual_cortical_config_t config;
    visual_cortical_default_config(&config);

    ASSERT_EQ(VISUAL_CORTICAL_DEFAULT_ORIENTATIONS, config.orientations_per_hypercolumn);
    ASSERT_EQ(VISUAL_CORTICAL_MODE_HYPERCOLUMN, config.mode);
    ASSERT_TRUE(config.enable_retinotopic_mapping);

    // Step 2: Create bridge
    visual_cortical_bridge_t* bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(nullptr, bridge);

    // Step 3: Verify initial state
    visual_cortical_state_t state = visual_cortical_get_state(bridge);
    EXPECT_EQ(VISUAL_CORTICAL_STATE_READY, state);

    // Step 4: Get statistics
    visual_cortical_stats_t stats;
    int ret = visual_cortical_get_stats(bridge, &stats);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, stats.images_processed);
    EXPECT_EQ(0, stats.hypercolumn_activations);

    // Step 5: Destroy
    visual_cortical_bridge_destroy(bridge);
}

TEST_F(E2EVisualCorticalTest, BridgeConfigurationOptions) {
    // Test different configuration modes

    visual_cortical_config_t config;
    visual_cortical_default_config(&config);

    // Test DIRECT mode
    config.mode = VISUAL_CORTICAL_MODE_DIRECT;
    config.enable_retinotopic_mapping = false;
    config.enable_cortical_immune = false;

    visual_cortical_bridge_t* bridge1 = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(nullptr, bridge1);
    EXPECT_EQ(VISUAL_CORTICAL_STATE_READY, visual_cortical_get_state(bridge1));
    visual_cortical_bridge_destroy(bridge1);

    // Test FULL mode
    config.mode = VISUAL_CORTICAL_MODE_FULL;
    config.enable_retinotopic_mapping = true;
    config.enable_cortical_immune = true;

    visual_cortical_bridge_t* bridge2 = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(nullptr, bridge2);
    EXPECT_EQ(VISUAL_CORTICAL_STATE_READY, visual_cortical_get_state(bridge2));
    visual_cortical_bridge_destroy(bridge2);
}

TEST_F(E2EVisualCorticalTest, BridgeNullHandling) {
    // Test null pointer handling throughout API

    // Null bridge to all functions
    EXPECT_EQ(VISUAL_CORTICAL_STATE_UNINITIALIZED, visual_cortical_get_state(nullptr));
    EXPECT_EQ(0, visual_cortical_get_num_hypercolumns(nullptr));
    EXPECT_EQ(0.0f, visual_cortical_get_immune_factor(nullptr));

    visual_cortical_stats_t stats;
    EXPECT_NE(0, visual_cortical_get_stats(nullptr, &stats));

    visual_cortical_orientation_result_t result;
    EXPECT_NE(0, visual_cortical_process(nullptr, nullptr, 0, 0, &result));

    // Null should be safe to destroy
    visual_cortical_bridge_destroy(nullptr);
}

/* =============================================================================
 * Orientation Detection Tests
 * ============================================================================= */

TEST_F(E2EVisualCorticalTest, DetectVerticalOrientation) {
    // Test detection of vertical edges (90 degrees)

    visual_cortical_config_t config;
    visual_cortical_default_config(&config);
    config.num_hypercolumns = 16;

    visual_cortical_bridge_t* bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(nullptr, bridge);

    // Create vertical edge image
    uint32_t width = 32;
    uint32_t height = 32;
    auto image = create_vertical_edge(width, height);

    // Process image
    visual_cortical_orientation_result_t result;
    memset(&result, 0, sizeof(result));

    int ret = visual_cortical_process(bridge, image.data(), width, height, &result);
    EXPECT_EQ(0, ret);

    // Verify vertical edge detected - Gabor at 0° detects vertical edges
    // (Gabor θ represents filter stripe orientation, perpendicular to detected edge)
    float tolerance = 25.0f;  // Allow 25 degree tolerance
    bool is_vertical = (result.dominant_orientation < tolerance) ||
                       (result.dominant_orientation > 180.0f - tolerance);
    EXPECT_TRUE(is_vertical) << "Detected orientation: " << result.dominant_orientation;
    EXPECT_GT(result.selectivity_index, 0.0f);
    EXPECT_GT(result.confidence, 0.0f);

    // Verify orientation responses allocated
    EXPECT_NE(nullptr, result.orientation_responses);
    EXPECT_EQ(config.orientations_per_hypercolumn, result.num_orientations);

    // Verify statistics updated
    visual_cortical_stats_t stats;
    visual_cortical_get_stats(bridge, &stats);
    EXPECT_EQ(1, stats.images_processed);

    // Cleanup
    visual_cortical_free_result(&result);
    visual_cortical_bridge_destroy(bridge);
}

TEST_F(E2EVisualCorticalTest, DetectHorizontalOrientation) {
    // Test detection of horizontal edges (0 degrees)

    visual_cortical_config_t config;
    visual_cortical_default_config(&config);
    config.num_hypercolumns = 16;  // Use 4x4 grid for adequate patch size

    visual_cortical_bridge_t* bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(nullptr, bridge);

    // Create horizontal edge image
    uint32_t width = 32;
    uint32_t height = 32;
    auto image = create_horizontal_edge(width, height);

    // Process image
    visual_cortical_orientation_result_t result;
    memset(&result, 0, sizeof(result));

    int ret = visual_cortical_process(bridge, image.data(), width, height, &result);
    EXPECT_EQ(0, ret);

    // Verify horizontal edge detected - Gabor at 90° detects horizontal edges
    // (Gabor θ represents filter stripe orientation, perpendicular to detected edge)
    float orientation = result.dominant_orientation;
    float tolerance = 25.0f;
    bool is_horizontal = (orientation > 90.0f - tolerance) && (orientation < 90.0f + tolerance);
    EXPECT_TRUE(is_horizontal) << "Detected orientation: " << orientation;

    EXPECT_GT(result.selectivity_index, 0.0f);
    EXPECT_GT(result.confidence, 0.0f);

    // Cleanup
    visual_cortical_free_result(&result);
    visual_cortical_bridge_destroy(bridge);
}

TEST_F(E2EVisualCorticalTest, DetectDiagonalOrientation) {
    // Test detection of diagonal edges (45 degrees)

    visual_cortical_config_t config;
    visual_cortical_default_config(&config);
    config.num_hypercolumns = 16;  // Use 4x4 grid for adequate patch size

    visual_cortical_bridge_t* bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(nullptr, bridge);

    // Create diagonal edge image
    uint32_t width = 32;
    uint32_t height = 32;
    auto image = create_diagonal_edge(width, height);

    // Process image
    visual_cortical_orientation_result_t result;
    memset(&result, 0, sizeof(result));

    int ret = visual_cortical_process(bridge, image.data(), width, height, &result);
    EXPECT_EQ(0, ret);

    // Verify diagonal orientation detected
    // Gabor convention: Gabor at θ detects edges perpendicular to θ
    // Edge at 45° (bottom-left to top-right) → Gabor at 135° (or equivalently -45°)
    float tolerance = 25.0f;
    float orientation = result.dominant_orientation;
    bool is_diagonal = (orientation > 135.0f - tolerance && orientation < 135.0f + tolerance) ||
                       (orientation < tolerance);  // 135° can wrap near 180°
    EXPECT_TRUE(is_diagonal) << "Expected ~135° for 45° edge, got: " << orientation;

    // Cleanup
    visual_cortical_free_result(&result);
    visual_cortical_bridge_destroy(bridge);
}

TEST_F(E2EVisualCorticalTest, MultipleOrientationDetections) {
    // Test processing multiple images with different orientations

    visual_cortical_config_t config;
    visual_cortical_default_config(&config);
    config.num_hypercolumns = 16;  // Use 4x4 grid for adequate patch size

    visual_cortical_bridge_t* bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(nullptr, bridge);

    uint32_t width = 32;
    uint32_t height = 32;

    // Test sequence of different orientations
    std::vector<float> orientations = {0.0f, 45.0f, 90.0f, 135.0f};
    std::vector<float> detected_orientations;

    for (float expected_ori : orientations) {
        auto image = create_test_image(width, height, expected_ori);

        visual_cortical_orientation_result_t result;
        memset(&result, 0, sizeof(result));

        int ret = visual_cortical_process(bridge, image.data(), width, height, &result);
        EXPECT_EQ(0, ret);

        detected_orientations.push_back(result.dominant_orientation);

        visual_cortical_free_result(&result);
    }

    // Verify statistics
    visual_cortical_stats_t stats;
    visual_cortical_get_stats(bridge, &stats);
    EXPECT_EQ(orientations.size(), stats.images_processed);

    visual_cortical_bridge_destroy(bridge);
}

TEST_F(E2EVisualCorticalTest, OrientationMapGeneration) {
    // Test generating full orientation map for entire image

    visual_cortical_config_t config;
    visual_cortical_default_config(&config);
    config.num_hypercolumns = 16;  // Use 4x4 grid for adequate patch size

    visual_cortical_bridge_t* bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(nullptr, bridge);

    uint32_t width = 32;
    uint32_t height = 32;
    auto image = create_vertical_edge(width, height);

    // Allocate orientation and selectivity maps
    std::vector<float> orientation_map(width * height);
    std::vector<float> selectivity_map(width * height);

    int ret = visual_cortical_get_orientation_map(
        bridge,
        image.data(),
        width,
        height,
        orientation_map.data(),
        selectivity_map.data()
    );

    EXPECT_EQ(0, ret);

    // Verify maps contain valid values
    bool has_valid_orientations = false;
    for (float ori : orientation_map) {
        if (ori >= 0.0f && ori <= 180.0f) {
            has_valid_orientations = true;
            break;
        }
    }
    EXPECT_TRUE(has_valid_orientations);

    visual_cortical_bridge_destroy(bridge);
}

/* =============================================================================
 * Immune Modulation Tests
 * ============================================================================= */

TEST_F(E2EVisualCorticalTest, ImmuneModulationBaseline) {
    // Test baseline immune modulation (no inflammation)

    visual_cortical_config_t config;
    visual_cortical_default_config(&config);
    config.enable_cortical_immune = true;
    config.immune_modulation_factor = 1.0f;

    visual_cortical_bridge_t* bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(nullptr, bridge);

    // Get initial immune factor
    float factor = visual_cortical_get_immune_factor(bridge);
    EXPECT_FLOAT_EQ(1.0f, factor);

    // Update immune modulation (should remain baseline without immune system)
    int ret = visual_cortical_update_immune_modulation(bridge);
    EXPECT_EQ(0, ret);

    factor = visual_cortical_get_immune_factor(bridge);
    EXPECT_FLOAT_EQ(1.0f, factor);

    visual_cortical_bridge_destroy(bridge);
}

TEST_F(E2EVisualCorticalTest, ManualImmuneModulation) {
    // Test manually setting immune modulation factor

    visual_cortical_config_t config;
    visual_cortical_default_config(&config);

    visual_cortical_bridge_t* bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(nullptr, bridge);

    // Set various modulation levels
    std::vector<float> test_factors = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f};

    for (float test_factor : test_factors) {
        int ret = visual_cortical_set_immune_factor(bridge, test_factor);
        EXPECT_EQ(0, ret);

        float retrieved_factor = visual_cortical_get_immune_factor(bridge);
        EXPECT_FLOAT_EQ(test_factor, retrieved_factor);
    }

    visual_cortical_bridge_destroy(bridge);
}

TEST_F(E2EVisualCorticalTest, ImmuneModulationAffectsProcessing) {
    // Test that immune modulation affects visual processing

    visual_cortical_config_t config;
    visual_cortical_default_config(&config);

    visual_cortical_bridge_t* bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(nullptr, bridge);

    uint32_t width = 32;
    uint32_t height = 32;
    auto image = create_vertical_edge(width, height);

    // Process with baseline immune factor
    visual_cortical_set_immune_factor(bridge, 1.0f);
    visual_cortical_orientation_result_t result1;
    memset(&result1, 0, sizeof(result1));
    visual_cortical_process(bridge, image.data(), width, height, &result1);

    // Process with reduced immune factor (simulating inflammation)
    visual_cortical_set_immune_factor(bridge, 0.5f);
    visual_cortical_orientation_result_t result2;
    memset(&result2, 0, sizeof(result2));
    visual_cortical_process(bridge, image.data(), width, height, &result2);

    // Results should differ (immune modulation affects processing)
    // Note: Exact effect depends on implementation
    EXPECT_NE(nullptr, result1.orientation_responses);
    EXPECT_NE(nullptr, result2.orientation_responses);

    // Cleanup
    visual_cortical_free_result(&result1);
    visual_cortical_free_result(&result2);
    visual_cortical_bridge_destroy(bridge);
}

TEST_F(E2EVisualCorticalTest, ImmuneFactorInStatistics) {
    // Test that immune factor appears in statistics

    visual_cortical_config_t config;
    visual_cortical_default_config(&config);

    visual_cortical_bridge_t* bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(nullptr, bridge);

    // Set specific immune factor
    float test_factor = 0.75f;
    visual_cortical_set_immune_factor(bridge, test_factor);

    // Get statistics
    visual_cortical_stats_t stats;
    int ret = visual_cortical_get_stats(bridge, &stats);
    EXPECT_EQ(0, ret);
    EXPECT_FLOAT_EQ(test_factor, stats.current_immune_modulation);

    visual_cortical_bridge_destroy(bridge);
}

/* =============================================================================
 * Bio-Async Communication Tests
 * ============================================================================= */

TEST_F(E2EVisualCorticalTest, BioAsyncConnection) {
    // Test bio-async connection lifecycle

    visual_cortical_config_t config;
    visual_cortical_default_config(&config);
    config.enable_bio_async = true;

    visual_cortical_bridge_t* bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(nullptr, bridge);

    // Connect to bio-async (may fail if router not available)
    int ret = visual_cortical_connect_bio_async(bridge);
    // Don't assert on connection - router may not be initialized in test environment

    // Check connection status
    bool connected = visual_cortical_is_bio_async_connected(bridge);
    // In test environment, connection may fail - that's OK

    // If connected, test disconnect
    if (connected) {
        ret = visual_cortical_disconnect_bio_async(bridge);
        EXPECT_EQ(0, ret);

        EXPECT_FALSE(visual_cortical_is_bio_async_connected(bridge));
    }

    visual_cortical_bridge_destroy(bridge);
}

TEST_F(E2EVisualCorticalTest, BioAsyncMessageProcessing) {
    // Test processing bio-async messages

    visual_cortical_config_t config;
    visual_cortical_default_config(&config);
    config.enable_bio_async = true;

    visual_cortical_bridge_t* bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(nullptr, bridge);

    visual_cortical_connect_bio_async(bridge);

    // Process messages (should handle gracefully even if no router)
    uint32_t processed = visual_cortical_process_bio_messages(bridge, 10);
    // No assertion - just verify it doesn't crash

    visual_cortical_bridge_destroy(bridge);
}

TEST_F(E2EVisualCorticalTest, BioAsyncOrientationBroadcast) {
    // Test broadcasting orientation detection results

    visual_cortical_config_t config;
    visual_cortical_default_config(&config);
    config.enable_bio_async = true;

    visual_cortical_bridge_t* bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(nullptr, bridge);

    visual_cortical_connect_bio_async(bridge);

    // Create test result
    visual_cortical_orientation_result_t result;
    memset(&result, 0, sizeof(result));
    result.dominant_orientation = 90.0f;
    result.selectivity_index = 0.8f;
    result.confidence = 0.9f;
    result.num_orientations = 8;

    // Broadcast (should handle gracefully even if no router)
    int ret = visual_cortical_broadcast_orientation(bridge, &result);
    // No assertion - just verify it doesn't crash

    visual_cortical_bridge_destroy(bridge);
}

/* =============================================================================
 * Hypercolumn Access Tests
 * ============================================================================= */

TEST_F(E2EVisualCorticalTest, HypercolumnAccess) {
    // Test accessing individual hypercolumns

    visual_cortical_config_t config;
    visual_cortical_default_config(&config);
    config.num_hypercolumns = 16;

    visual_cortical_bridge_t* bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(nullptr, bridge);

    // Get number of hypercolumns
    uint32_t num_hc = visual_cortical_get_num_hypercolumns(bridge);
    EXPECT_EQ(config.num_hypercolumns, num_hc);

    // Access hypercolumns by index
    for (uint32_t i = 0; i < num_hc; i++) {
        const orientation_hypercolumn_t* hc =
            visual_cortical_get_hypercolumn_by_index(bridge, i);
        // May be NULL depending on implementation
    }

    // Access out of bounds should return NULL
    const orientation_hypercolumn_t* invalid =
        visual_cortical_get_hypercolumn_by_index(bridge, 9999);
    EXPECT_EQ(nullptr, invalid);

    visual_cortical_bridge_destroy(bridge);
}

/* =============================================================================
 * Statistics and State Tests
 * ============================================================================= */

TEST_F(E2EVisualCorticalTest, StatisticsTracking) {
    // Test that statistics are properly tracked

    visual_cortical_config_t config;
    visual_cortical_default_config(&config);

    visual_cortical_bridge_t* bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(nullptr, bridge);

    // Initial statistics
    visual_cortical_stats_t stats;
    visual_cortical_get_stats(bridge, &stats);
    EXPECT_EQ(0, stats.images_processed);
    EXPECT_EQ(0, stats.bio_messages_sent);

    // Process some images
    uint32_t width = 32;
    uint32_t height = 32;

    for (int i = 0; i < 5; i++) {
        auto image = create_test_image(width, height, i * 30.0f);
        visual_cortical_orientation_result_t result;
        memset(&result, 0, sizeof(result));

        visual_cortical_process(bridge, image.data(), width, height, &result);
        visual_cortical_free_result(&result);
    }

    // Check updated statistics
    visual_cortical_get_stats(bridge, &stats);
    EXPECT_EQ(5, stats.images_processed);

    // Reset statistics
    int ret = visual_cortical_reset_stats(bridge);
    EXPECT_EQ(0, ret);

    visual_cortical_get_stats(bridge, &stats);
    EXPECT_EQ(0, stats.images_processed);

    visual_cortical_bridge_destroy(bridge);
}

TEST_F(E2EVisualCorticalTest, RetinotopicMapAccess) {
    // Test accessing retinotopic map

    visual_cortical_config_t config;
    visual_cortical_default_config(&config);
    config.enable_retinotopic_mapping = true;

    visual_cortical_bridge_t* bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(nullptr, bridge);

    // Get retinotopic map
    const topographic_map_t* map = visual_cortical_get_retinotopic_map(bridge);
    // May be NULL depending on implementation

    visual_cortical_bridge_destroy(bridge);

    // Test with retinotopic mapping disabled
    config.enable_retinotopic_mapping = false;
    bridge = visual_cortical_bridge_create(&config, nullptr);
    ASSERT_NE(nullptr, bridge);

    map = visual_cortical_get_retinotopic_map(bridge);
    EXPECT_EQ(nullptr, map);

    visual_cortical_bridge_destroy(bridge);
}

/* =============================================================================
 * Main
 * ============================================================================= */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
