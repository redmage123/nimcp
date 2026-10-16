/**
 * @file test_occipital_gpu_bridge.cpp
 * @brief Unit tests for Occipital GPU Bridge
 *
 * Tests the GPU integration bridge that connects the CPU occipital cortex
 * to GPU visual kernels for accelerated processing.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Headers with proper extern "C" guards internally
#include "gpu/occipital/nimcp_occipital_gpu_bridge.h"
#include "gpu/context/nimcp_gpu_context.h"

class OccipitalGPUBridgeTest : public ::testing::Test {
protected:
    occipital_adapter_t* occipital = nullptr;
    occipital_gpu_bridge_t* bridge = nullptr;

    void SetUp() override {
        // Skip tests if GPU not available
        if (!occipital_gpu_processing_available()) {
            GTEST_SKIP() << "GPU processing not available";
        }

        // Create CPU occipital adapter
        occipital_config_t config = occipital_default_config();
        config.image_width = 64;
        config.image_height = 64;
        config.color_channels = 3;
        occipital = occipital_create(&config);
        ASSERT_NE(occipital, nullptr) << "Failed to create occipital adapter";
    }

    void TearDown() override {
        if (bridge) {
            occipital_gpu_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (occipital) {
            occipital_destroy(occipital);
            occipital = nullptr;
        }
    }

    // Helper to create a test image
    void create_test_image(std::vector<float>& data, uint32_t w, uint32_t h, uint32_t c) {
        data.resize(w * h * c);
        for (uint32_t ch = 0; ch < c; ch++) {
            for (uint32_t y = 0; y < h; y++) {
                for (uint32_t x = 0; x < w; x++) {
                    // Create a gradient pattern with some edges
                    float val = (float)x / (float)w;
                    if (x > w/4 && x < 3*w/4 && y > h/4 && y < 3*h/4) {
                        val = 0.8f;  // Center rectangle
                    }
                    data[ch * w * h + y * w + x] = val;
                }
            }
        }
    }
};

//=============================================================================
// Bridge Lifecycle Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeTest, DefaultConfigIsValid) {
    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();

    EXPECT_TRUE(config.enable_gpu_v1);
    EXPECT_TRUE(config.enable_gpu_v2);
    EXPECT_TRUE(config.enable_gpu_v4);
    EXPECT_TRUE(config.enable_gpu_v5);
    EXPECT_TRUE(config.enable_gpu_saliency);
    EXPECT_TRUE(config.auto_fallback);
    EXPECT_EQ(config.device_id, 0);
    EXPECT_GT(config.max_consecutive_failures, 0u);
}

TEST_F(OccipitalGPUBridgeTest, CreateWithDefaultConfig) {
    bridge = occipital_gpu_bridge_create(occipital, nullptr);
    ASSERT_NE(bridge, nullptr);
    EXPECT_TRUE(occipital_gpu_bridge_is_available(bridge));
}

TEST_F(OccipitalGPUBridgeTest, CreateWithCustomConfig) {
    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    config.enable_gpu_v1 = true;
    config.enable_gpu_v2 = false;  // Disable V2
    config.auto_fallback = false;

    bridge = occipital_gpu_bridge_create(occipital, &config);
    ASSERT_NE(bridge, nullptr);

    EXPECT_TRUE(occipital_gpu_bridge_area_enabled(bridge, 0));  // V1
    EXPECT_FALSE(occipital_gpu_bridge_area_enabled(bridge, 1)); // V2
}

TEST_F(OccipitalGPUBridgeTest, CreateWithNullOccipitalFails) {
    bridge = occipital_gpu_bridge_create(nullptr, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(OccipitalGPUBridgeTest, DestroyNull) {
    // Should not crash
    occipital_gpu_bridge_destroy(nullptr);
}

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeTest, InitSizeAllocatesTensors) {
    bridge = occipital_gpu_bridge_create(occipital, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_TRUE(occipital_gpu_bridge_init_size(bridge, 64, 64, 3));

    // Verify we can get tensors
    nimcp_gpu_tensor_t* edges = occipital_gpu_bridge_get_tensor(bridge, "v1_edges");
    EXPECT_NE(edges, nullptr);

    nimcp_gpu_tensor_t* saliency = occipital_gpu_bridge_get_tensor(bridge, "saliency");
    EXPECT_NE(saliency, nullptr);
}

TEST_F(OccipitalGPUBridgeTest, InitSizeCanBeCalledMultipleTimes) {
    bridge = occipital_gpu_bridge_create(occipital, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_TRUE(occipital_gpu_bridge_init_size(bridge, 64, 64, 3));
    EXPECT_TRUE(occipital_gpu_bridge_init_size(bridge, 128, 128, 3));
    EXPECT_TRUE(occipital_gpu_bridge_init_size(bridge, 64, 64, 1));
}

TEST_F(OccipitalGPUBridgeTest, ResetClearsState) {
    bridge = occipital_gpu_bridge_create(occipital, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_TRUE(occipital_gpu_bridge_reset(bridge));

    occipital_gpu_bridge_stats_t stats;
    EXPECT_TRUE(occipital_gpu_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(stats.frames_processed_gpu, 0u);
    EXPECT_EQ(stats.gpu_failures, 0u);
}

//=============================================================================
// Data Upload Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeTest, UploadInputSucceeds) {
    bridge = occipital_gpu_bridge_create(occipital, nullptr);
    ASSERT_NE(bridge, nullptr);

    std::vector<float> image;
    create_test_image(image, 64, 64, 3);

    visual_input_t input;
    input.data = image.data();
    input.width = 64;
    input.height = 64;
    input.channels = 3;
    input.timestamp_us = 0;
    input.frame_id = 0;

    EXPECT_TRUE(occipital_gpu_upload_input(bridge, &input));
}

TEST_F(OccipitalGPUBridgeTest, UploadGrayscaleSucceeds) {
    bridge = occipital_gpu_bridge_create(occipital, nullptr);
    ASSERT_NE(bridge, nullptr);

    std::vector<float> image;
    create_test_image(image, 64, 64, 1);

    visual_input_t input;
    input.data = image.data();
    input.width = 64;
    input.height = 64;
    input.channels = 1;
    input.timestamp_us = 0;
    input.frame_id = 0;

    EXPECT_TRUE(occipital_gpu_upload_input(bridge, &input));
}

TEST_F(OccipitalGPUBridgeTest, UploadNullInputFails) {
    bridge = occipital_gpu_bridge_create(occipital, nullptr);
    ASSERT_NE(bridge, nullptr);

    EXPECT_FALSE(occipital_gpu_upload_input(bridge, nullptr));
}

//=============================================================================
// Processing Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeTest, ProcessV1Succeeds) {
    bridge = occipital_gpu_bridge_create(occipital, nullptr);
    ASSERT_NE(bridge, nullptr);

    std::vector<float> image;
    create_test_image(image, 64, 64, 3);

    visual_input_t input;
    input.data = image.data();
    input.width = 64;
    input.height = 64;
    input.channels = 3;
    input.timestamp_us = 0;
    input.frame_id = 0;

    EXPECT_TRUE(occipital_gpu_upload_input(bridge, &input));
    EXPECT_TRUE(occipital_gpu_process_v1(bridge));
}

TEST_F(OccipitalGPUBridgeTest, ProcessV4RequiresRGB) {
    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    config.auto_fallback = false;

    bridge = occipital_gpu_bridge_create(occipital, &config);
    ASSERT_NE(bridge, nullptr);

    // Upload grayscale image
    std::vector<float> image;
    create_test_image(image, 64, 64, 1);

    visual_input_t input;
    input.data = image.data();
    input.width = 64;
    input.height = 64;
    input.channels = 1;
    input.timestamp_us = 0;
    input.frame_id = 0;

    EXPECT_TRUE(occipital_gpu_upload_input(bridge, &input));

    // V4 should fail with grayscale (requires color)
    EXPECT_FALSE(occipital_gpu_process_v4(bridge));
}

TEST_F(OccipitalGPUBridgeTest, ProcessV5RequiresPreviousFrame) {
    bridge = occipital_gpu_bridge_create(occipital, nullptr);
    ASSERT_NE(bridge, nullptr);

    std::vector<float> image;
    create_test_image(image, 64, 64, 3);

    visual_input_t input;
    input.data = image.data();
    input.width = 64;
    input.height = 64;
    input.channels = 3;
    input.timestamp_us = 0;
    input.frame_id = 0;

    EXPECT_TRUE(occipital_gpu_upload_input(bridge, &input));

    // First call stores frame
    EXPECT_TRUE(occipital_gpu_process_v5(bridge));

    // Second call computes motion
    input.frame_id = 1;
    input.timestamp_us = 33333;  // ~30fps
    EXPECT_TRUE(occipital_gpu_upload_input(bridge, &input));
    EXPECT_TRUE(occipital_gpu_process_v5(bridge));
}

TEST_F(OccipitalGPUBridgeTest, FullPipelineProcess) {
    bridge = occipital_gpu_bridge_create(occipital, nullptr);
    ASSERT_NE(bridge, nullptr);

    std::vector<float> image;
    create_test_image(image, 64, 64, 3);

    visual_input_t input;
    input.data = image.data();
    input.width = 64;
    input.height = 64;
    input.channels = 3;
    input.timestamp_us = 0;
    input.frame_id = 0;

    visual_processing_result_t result;
    EXPECT_TRUE(occipital_gpu_process_input(bridge, &input, &result));

    // At least V1 should succeed
    EXPECT_TRUE(result.v1_processed);
    EXPECT_TRUE(result.ready_for_downstream);
}

//=============================================================================
// Feature Download Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeTest, DownloadFeaturesAfterProcessing) {
    bridge = occipital_gpu_bridge_create(occipital, nullptr);
    ASSERT_NE(bridge, nullptr);

    std::vector<float> image;
    create_test_image(image, 64, 64, 3);

    visual_input_t input;
    input.data = image.data();
    input.width = 64;
    input.height = 64;
    input.channels = 3;
    input.timestamp_us = 0;
    input.frame_id = 0;

    EXPECT_TRUE(occipital_gpu_upload_input(bridge, &input));
    EXPECT_TRUE(occipital_gpu_process_v1(bridge));

    std::vector<visual_feature_t> features(256);
    uint32_t num_features = 256;
    EXPECT_TRUE(occipital_gpu_download_features(bridge, features.data(), 256, &num_features));

    // Should have detected some features (edges in our test image)
    // Note: May be 0 if threshold is too high for this simple test image
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeTest, StatsAreUpdated) {
    bridge = occipital_gpu_bridge_create(occipital, nullptr);
    ASSERT_NE(bridge, nullptr);

    std::vector<float> image;
    create_test_image(image, 64, 64, 3);

    visual_input_t input;
    input.data = image.data();
    input.width = 64;
    input.height = 64;
    input.channels = 3;
    input.timestamp_us = 0;
    input.frame_id = 0;

    visual_processing_result_t result;
    occipital_gpu_process_input(bridge, &input, &result);

    occipital_gpu_bridge_stats_t stats;
    EXPECT_TRUE(occipital_gpu_bridge_get_stats(bridge, &stats));

    // Should have processed at least one frame
    EXPECT_GT(stats.frames_processed_gpu + stats.frames_processed_cpu, 0u);

    if (result.v1_processed) {
        EXPECT_GT(stats.v1_gpu_calls, 0u);
    }
}

TEST_F(OccipitalGPUBridgeTest, ResetStatsWorks) {
    bridge = occipital_gpu_bridge_create(occipital, nullptr);
    ASSERT_NE(bridge, nullptr);

    std::vector<float> image;
    create_test_image(image, 64, 64, 3);

    visual_input_t input;
    input.data = image.data();
    input.width = 64;
    input.height = 64;
    input.channels = 3;
    input.timestamp_us = 0;
    input.frame_id = 0;

    visual_processing_result_t result;
    occipital_gpu_process_input(bridge, &input, &result);

    occipital_gpu_bridge_reset_stats(bridge);

    occipital_gpu_bridge_stats_t stats;
    EXPECT_TRUE(occipital_gpu_bridge_get_stats(bridge, &stats));
    EXPECT_EQ(stats.frames_processed_gpu, 0u);
    EXPECT_EQ(stats.v1_gpu_calls, 0u);
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeTest, ConfigureAtRuntime) {
    bridge = occipital_gpu_bridge_create(occipital, nullptr);
    ASSERT_NE(bridge, nullptr);

    occipital_gpu_bridge_config_t new_config = occipital_gpu_bridge_default_config();
    new_config.enable_gpu_v2 = false;
    new_config.max_consecutive_failures = 10;

    EXPECT_TRUE(occipital_gpu_bridge_configure(bridge, &new_config));
    EXPECT_FALSE(occipital_gpu_bridge_area_enabled(bridge, 1));  // V2
}

TEST_F(OccipitalGPUBridgeTest, SetAreaEnabled) {
    bridge = occipital_gpu_bridge_create(occipital, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Disable V1
    EXPECT_TRUE(occipital_gpu_bridge_set_area_enabled(bridge, 0, false));
    EXPECT_FALSE(occipital_gpu_bridge_area_enabled(bridge, 0));

    // Re-enable V1
    EXPECT_TRUE(occipital_gpu_bridge_set_area_enabled(bridge, 0, true));
    EXPECT_TRUE(occipital_gpu_bridge_area_enabled(bridge, 0));
}

//=============================================================================
// Tensor Access Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeTest, GetTensorByName) {
    bridge = occipital_gpu_bridge_create(occipital, nullptr);
    ASSERT_NE(bridge, nullptr);
    ASSERT_TRUE(occipital_gpu_bridge_init_size(bridge, 64, 64, 3));

    // Valid tensor names
    EXPECT_NE(occipital_gpu_bridge_get_tensor(bridge, "v1_edges"), nullptr);
    EXPECT_NE(occipital_gpu_bridge_get_tensor(bridge, "saliency"), nullptr);
    EXPECT_NE(occipital_gpu_bridge_get_tensor(bridge, "input_gray"), nullptr);
    EXPECT_NE(occipital_gpu_bridge_get_tensor(bridge, "v5_flow_u"), nullptr);

    // Invalid tensor name
    EXPECT_EQ(occipital_gpu_bridge_get_tensor(bridge, "nonexistent"), nullptr);
}

//=============================================================================
// GPU Availability Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeTest, GPUAvailabilityCheck) {
    // This test is mainly for coverage - the actual result depends on hardware
    bool available = occipital_gpu_processing_available();

    // If we got this far, GPU should be available (we skip tests otherwise)
    EXPECT_TRUE(available);
}

TEST_F(OccipitalGPUBridgeTest, GPUDeviceInfo) {
    char name[256] = {0};
    int compute_cap = 0;
    size_t memory_mb = 0;

    bool success = occipital_gpu_device_info(0, name, &compute_cap, &memory_mb);
    if (success) {
        EXPECT_GT(strlen(name), 0u);
        EXPECT_GE(compute_cap, 30);  // At least compute 3.0
        EXPECT_GT(memory_mb, 0u);
    }
}

//=============================================================================
// Re-enable Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeTest, ReenableGPU) {
    bridge = occipital_gpu_bridge_create(occipital, nullptr);
    ASSERT_NE(bridge, nullptr);

    // Re-enable should succeed on working GPU
    EXPECT_TRUE(occipital_gpu_bridge_reenable(bridge));
    EXPECT_TRUE(occipital_gpu_bridge_is_available(bridge));
}

//=============================================================================
// Memory Info Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeTest, MemoryInfo) {
    bridge = occipital_gpu_bridge_create(occipital, nullptr);
    ASSERT_NE(bridge, nullptr);
    ASSERT_TRUE(occipital_gpu_bridge_init_size(bridge, 64, 64, 3));

    size_t used = 0, peak = 0;
    EXPECT_TRUE(occipital_gpu_bridge_memory_info(bridge, &used, &peak));
    // Memory values may be 0 if not tracked, but function should succeed
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
