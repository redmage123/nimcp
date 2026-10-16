/**
 * @file test_dragonfly_visual_bridge.cpp
 * @brief Unit tests for dragonfly visual cortex bridge
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly_visual_bridge.h"

class VisualBridgeTest : public ::testing::Test {
protected:
    dragonfly_visual_bridge_t* bridge = nullptr;
    dragonfly_system_t* dragonfly = nullptr;

    void SetUp() override {
        // Create dragonfly system first
        dragonfly = dragonfly_system_create(nullptr);
        ASSERT_NE(dragonfly, nullptr);

        // Create visual bridge without visual cortex (test mode)
        bridge = dragonfly_visual_bridge_create(dragonfly, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            dragonfly_visual_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (dragonfly) {
            dragonfly_system_destroy(dragonfly);
            dragonfly = nullptr;
        }
    }

    motion_blob_t create_test_blob(float x, float y, float size, float vx, float vy) {
        motion_blob_t blob;
        memset(&blob, 0, sizeof(blob));
        blob.center_x = x;
        blob.center_y = y;
        blob.size_pixels = size;
        blob.velocity_x = vx;
        blob.velocity_y = vy;
        blob.contrast = 0.8f;
        blob.salience = 0.7f;
        return blob;
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(VisualBridgeTest, DefaultConfig) {
    visual_bridge_config_t config = visual_bridge_default_config();

    EXPECT_GT(config.min_motion_speed, 0.0f);
    EXPECT_GT(config.min_blob_size, 0.0f);
    EXPECT_GT(config.max_blob_size, config.min_blob_size);
    EXPECT_GE(config.contrast_threshold, 0.0f);
    EXPECT_LE(config.contrast_threshold, 1.0f);
    EXPECT_GT(config.assumed_target_size_m, 0.0f);
    EXPECT_GT(config.calibration.focal_length, 0.0f);
    EXPECT_GT(config.calibration.image_width, 0u);
    EXPECT_GT(config.calibration.image_height, 0u);
}

TEST_F(VisualBridgeTest, ValidateConfig) {
    visual_bridge_config_t config = visual_bridge_default_config();
    EXPECT_TRUE(visual_bridge_validate_config(&config));

    // Invalid: null
    EXPECT_FALSE(visual_bridge_validate_config(nullptr));

    // Invalid: negative motion speed
    config = visual_bridge_default_config();
    config.min_motion_speed = -1.0f;
    EXPECT_FALSE(visual_bridge_validate_config(&config));

    // Invalid: zero focal length
    config = visual_bridge_default_config();
    config.calibration.focal_length = 0.0f;
    EXPECT_FALSE(visual_bridge_validate_config(&config));
}

TEST_F(VisualBridgeTest, CreateWithCustomConfig) {
    visual_bridge_config_t config = visual_bridge_default_config();
    config.min_motion_speed = 5.0f;
    config.assumed_target_size_m = 0.1f;

    dragonfly_visual_bridge_t* custom =
        dragonfly_visual_bridge_create(dragonfly, nullptr, &config);
    ASSERT_NE(custom, nullptr);

    visual_bridge_config_t retrieved;
    EXPECT_EQ(dragonfly_visual_bridge_get_config(custom, &retrieved), 0);
    EXPECT_FLOAT_EQ(retrieved.min_motion_speed, 5.0f);
    EXPECT_FLOAT_EQ(retrieved.assumed_target_size_m, 0.1f);

    dragonfly_visual_bridge_destroy(custom);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(VisualBridgeTest, CreateAndDestroy) {
    dragonfly_visual_bridge_t* b =
        dragonfly_visual_bridge_create(dragonfly, nullptr, nullptr);
    ASSERT_NE(b, nullptr);
    dragonfly_visual_bridge_destroy(b);
}

TEST_F(VisualBridgeTest, DestroyNull) {
    dragonfly_visual_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(VisualBridgeTest, Reset) {
    // Inject a blob
    motion_blob_t blob = create_test_blob(100.0f, 100.0f, 20.0f, 5.0f, 3.0f);
    dragonfly_visual_bridge_inject_blob(bridge, &blob);

    // Reset
    EXPECT_EQ(dragonfly_visual_bridge_reset(bridge), 0);

    // Result should be empty
    visual_motion_result_t result;
    dragonfly_visual_bridge_get_result(bridge, &result);
    EXPECT_EQ(result.num_blobs, 0u);
}

//=============================================================================
// Blob Injection Tests
//=============================================================================

TEST_F(VisualBridgeTest, InjectBlob) {
    motion_blob_t blob = create_test_blob(320.0f, 240.0f, 30.0f, 10.0f, 5.0f);
    EXPECT_EQ(dragonfly_visual_bridge_inject_blob(bridge, &blob), 0);

    visual_motion_result_t result;
    dragonfly_visual_bridge_get_result(bridge, &result);
    EXPECT_EQ(result.num_blobs, 1u);
    EXPECT_FLOAT_EQ(result.blobs[0].center_x, 320.0f);
    EXPECT_FLOAT_EQ(result.blobs[0].center_y, 240.0f);
}

TEST_F(VisualBridgeTest, InjectMultipleBlobs) {
    for (int i = 0; i < 5; i++) {
        motion_blob_t blob = create_test_blob(100.0f + i*50, 100.0f, 20.0f, 5.0f, 0.0f);
        EXPECT_EQ(dragonfly_visual_bridge_inject_blob(bridge, &blob), 0);
    }

    visual_motion_result_t result;
    dragonfly_visual_bridge_get_result(bridge, &result);
    EXPECT_EQ(result.num_blobs, 5u);
}

TEST_F(VisualBridgeTest, AttentionPeakFromBlobs) {
    // Inject multiple blobs with different salience
    motion_blob_t blob1 = create_test_blob(100.0f, 100.0f, 20.0f, 5.0f, 0.0f);
    blob1.salience = 0.3f;
    dragonfly_visual_bridge_inject_blob(bridge, &blob1);

    motion_blob_t blob2 = create_test_blob(300.0f, 200.0f, 25.0f, 5.0f, 0.0f);
    blob2.salience = 0.9f;  // Most salient
    dragonfly_visual_bridge_inject_blob(bridge, &blob2);

    motion_blob_t blob3 = create_test_blob(500.0f, 300.0f, 15.0f, 5.0f, 0.0f);
    blob3.salience = 0.5f;
    dragonfly_visual_bridge_inject_blob(bridge, &blob3);

    visual_motion_result_t result;
    dragonfly_visual_bridge_get_result(bridge, &result);

    // Attention peak should be at most salient blob
    EXPECT_FLOAT_EQ(result.attention_peak_x, 300.0f);
    EXPECT_FLOAT_EQ(result.attention_peak_y, 200.0f);
    EXPECT_FLOAT_EQ(result.peak_salience, 0.9f);
}

TEST_F(VisualBridgeTest, TrackIdAssignment) {
    motion_blob_t blob = create_test_blob(100.0f, 100.0f, 20.0f, 5.0f, 0.0f);
    blob.track_id = 0;  // Unassigned

    dragonfly_visual_bridge_inject_blob(bridge, &blob);

    visual_motion_result_t result;
    dragonfly_visual_bridge_get_result(bridge, &result);

    EXPECT_GT(result.blobs[0].track_id, 0u);  // Should be assigned
}

//=============================================================================
// Coordinate Conversion Tests
//=============================================================================

TEST_F(VisualBridgeTest, PixelTo3D) {
    float position[3];

    // Center of image at 1m depth
    EXPECT_EQ(dragonfly_visual_bridge_pixel_to_3d(bridge, 320.0f, 240.0f, 1.0f, position), 0);
    EXPECT_NEAR(position[0], 0.0f, 0.01f);  // Should be at center
    EXPECT_NEAR(position[1], 0.0f, 0.01f);
    EXPECT_FLOAT_EQ(position[2], 1.0f);
}

TEST_F(VisualBridgeTest, PixelTo3DOffset) {
    float position[3];

    // 100 pixels right of center at 1m
    EXPECT_EQ(dragonfly_visual_bridge_pixel_to_3d(bridge, 420.0f, 240.0f, 1.0f, position), 0);
    EXPECT_GT(position[0], 0.0f);  // Should be right of center
    EXPECT_NEAR(position[1], 0.0f, 0.01f);
    EXPECT_FLOAT_EQ(position[2], 1.0f);
}

TEST_F(VisualBridgeTest, DepthEstimation) {
    // With default config: assumed size 0.05m, focal 500
    // At 1m, blob should be 25 pixels (0.05 * 500 / 1.0)
    float depth = dragonfly_visual_bridge_estimate_depth(bridge, 25.0f);
    EXPECT_NEAR(depth, 1.0f, 0.01f);

    // At 2m, blob should be 12.5 pixels
    depth = dragonfly_visual_bridge_estimate_depth(bridge, 12.5f);
    EXPECT_NEAR(depth, 2.0f, 0.01f);
}

TEST_F(VisualBridgeTest, VelocityConversion) {
    // At 30 FPS with focal 500:
    // 500 pixels/frame = 1 radian/frame = 30 rad/s
    float angular = dragonfly_visual_bridge_pixel_velocity_to_angular(bridge, 500.0f);
    EXPECT_NEAR(angular, 30.0f, 0.1f);

    // 50 pixels/frame = 0.1 rad/frame = 3 rad/s
    angular = dragonfly_visual_bridge_pixel_velocity_to_angular(bridge, 50.0f);
    EXPECT_NEAR(angular, 3.0f, 0.1f);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(VisualBridgeTest, GetStats) {
    visual_bridge_stats_t stats;
    EXPECT_EQ(dragonfly_visual_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.blobs_detected, 0u);
}

TEST_F(VisualBridgeTest, StatsTrackBlobs) {
    motion_blob_t blob = create_test_blob(100.0f, 100.0f, 20.0f, 5.0f, 0.0f);

    for (int i = 0; i < 10; i++) {
        dragonfly_visual_bridge_inject_blob(bridge, &blob);
    }

    visual_bridge_stats_t stats;
    dragonfly_visual_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.blobs_detected, 10u);
    EXPECT_EQ(stats.detections_sent, 10u);  // Connected to dragonfly
}

TEST_F(VisualBridgeTest, ResetStats) {
    motion_blob_t blob = create_test_blob(100.0f, 100.0f, 20.0f, 5.0f, 0.0f);
    dragonfly_visual_bridge_inject_blob(bridge, &blob);

    EXPECT_EQ(dragonfly_visual_bridge_reset_stats(bridge), 0);

    visual_bridge_stats_t stats;
    dragonfly_visual_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.blobs_detected, 0u);
}

//=============================================================================
// Configuration Update Tests
//=============================================================================

TEST_F(VisualBridgeTest, SetConfig) {
    visual_bridge_config_t config = visual_bridge_default_config();
    config.min_motion_speed = 10.0f;

    EXPECT_EQ(dragonfly_visual_bridge_set_config(bridge, &config), 0);

    visual_bridge_config_t retrieved;
    dragonfly_visual_bridge_get_config(bridge, &retrieved);
    EXPECT_FLOAT_EQ(retrieved.min_motion_speed, 10.0f);
}

TEST_F(VisualBridgeTest, SetCalibration) {
    visual_calibration_t cal;
    cal.focal_length = 1000.0f;
    cal.principal_x = 640.0f;
    cal.principal_y = 512.0f;
    cal.image_width = 1280;
    cal.image_height = 1024;
    cal.baseline_distance = 0.1f;

    EXPECT_EQ(dragonfly_visual_bridge_set_calibration(bridge, &cal), 0);

    visual_bridge_config_t config;
    dragonfly_visual_bridge_get_config(bridge, &config);
    EXPECT_FLOAT_EQ(config.calibration.focal_length, 1000.0f);
    EXPECT_EQ(config.calibration.image_width, 1280u);
}

//=============================================================================
// Null Pointer Tests
//=============================================================================

TEST_F(VisualBridgeTest, NullPointerHandling) {
    motion_blob_t blob = create_test_blob(100.0f, 100.0f, 20.0f, 5.0f, 0.0f);
    visual_motion_result_t result;
    visual_bridge_stats_t stats;
    visual_bridge_config_t config;
    visual_calibration_t cal = visual_bridge_default_config().calibration;
    float position[3];

    // Null bridge
    EXPECT_EQ(dragonfly_visual_bridge_reset(nullptr), -1);
    EXPECT_EQ(dragonfly_visual_bridge_inject_blob(nullptr, &blob), -1);
    EXPECT_EQ(dragonfly_visual_bridge_get_result(nullptr, &result), -1);
    EXPECT_EQ(dragonfly_visual_bridge_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(dragonfly_visual_bridge_reset_stats(nullptr), -1);
    EXPECT_EQ(dragonfly_visual_bridge_set_config(nullptr, &config), -1);
    EXPECT_EQ(dragonfly_visual_bridge_get_config(nullptr, &config), -1);
    EXPECT_EQ(dragonfly_visual_bridge_set_calibration(nullptr, &cal), -1);
    EXPECT_EQ(dragonfly_visual_bridge_pixel_to_3d(nullptr, 0, 0, 1, position), -1);
    EXPECT_LT(dragonfly_visual_bridge_estimate_depth(nullptr, 10.0f), 0.0f);

    // Null outputs
    EXPECT_EQ(dragonfly_visual_bridge_inject_blob(bridge, nullptr), -1);
    EXPECT_EQ(dragonfly_visual_bridge_get_result(bridge, nullptr), -1);
    EXPECT_EQ(dragonfly_visual_bridge_get_stats(bridge, nullptr), -1);
    EXPECT_EQ(dragonfly_visual_bridge_set_config(bridge, nullptr), -1);
    EXPECT_EQ(dragonfly_visual_bridge_get_config(bridge, nullptr), -1);
    EXPECT_EQ(dragonfly_visual_bridge_set_calibration(bridge, nullptr), -1);
    EXPECT_EQ(dragonfly_visual_bridge_pixel_to_3d(bridge, 0, 0, 1, nullptr), -1);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(VisualBridgeTest, InvalidDepth) {
    float position[3];
    EXPECT_EQ(dragonfly_visual_bridge_pixel_to_3d(bridge, 320.0f, 240.0f, 0.0f, position), -1);
    EXPECT_EQ(dragonfly_visual_bridge_pixel_to_3d(bridge, 320.0f, 240.0f, -1.0f, position), -1);
}

TEST_F(VisualBridgeTest, InvalidBlobSize) {
    float depth = dragonfly_visual_bridge_estimate_depth(bridge, 0.0f);
    EXPECT_LT(depth, 0.0f);

    depth = dragonfly_visual_bridge_estimate_depth(bridge, -10.0f);
    EXPECT_LT(depth, 0.0f);
}

TEST_F(VisualBridgeTest, ProcessFrameWithNullImage) {
    EXPECT_EQ(dragonfly_visual_bridge_process_frame(bridge, nullptr, 640, 480, 1), -1);
}

TEST_F(VisualBridgeTest, ProcessFrameWithZeroDimensions) {
    float image[640 * 480];
    EXPECT_EQ(dragonfly_visual_bridge_process_frame(bridge, image, 0, 480, 1), -1);
    EXPECT_EQ(dragonfly_visual_bridge_process_frame(bridge, image, 640, 0, 1), -1);
}
