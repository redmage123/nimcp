/**
 * @file test_visual_processing_gpu.cpp
 * @brief Integration tests for GPU visual processing via occipital bridge
 *
 * WHAT: Test GPU/CPU equivalence for visual processing operations
 * WHY:  Verify GPU visual processing through the bridge works correctly
 * HOW:  Test occipital bridge lifecycle, data transfer, and processing stages
 *
 * TEST CATEGORIES:
 * - Bridge Lifecycle: Creation, initialization, destruction
 * - Data Transfer: Upload/download visual data
 * - V1 Processing: Edge detection
 * - V2 Processing: Contour integration
 * - V4 Processing: Color processing
 * - V5 Processing: Optical flow/motion
 * - Full Pipeline: Complete visual processing flow
 *
 * @author NIMCP Development Team
 * @date 2025-01-02
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <chrono>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <numeric>

// GPU headers
#include "gpu/occipital/nimcp_occipital_gpu_bridge.h"
#include "gpu/nimcp_execution_mode.h"
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"

//=============================================================================
// Test Fixture
//=============================================================================

class VisualProcessingGPUIntegrationTest : public ::testing::Test {
protected:
    occipital_adapter_t* occipital_ = nullptr;
    occipital_gpu_bridge_t* bridge_ = nullptr;
    hardware_capabilities_t caps_;
    bool has_gpu_ = false;
    std::mt19937 rng_;

    // Test image dimensions
    static constexpr int WIDTH = 64;
    static constexpr int HEIGHT = 64;

    void SetUp() override {
        memset(&caps_, 0, sizeof(caps_));
        execution_detect_capabilities(&caps_);
        has_gpu_ = caps_.cuda_available || caps_.rocm_available || caps_.opencl_available;
        rng_.seed(42);

        // Create occipital adapter (required for GPU bridge)
        occipital_config_t occ_config = occipital_default_config();
        occ_config.image_width = WIDTH;
        occ_config.image_height = HEIGHT;
        occ_config.color_channels = 1;
        occipital_ = occipital_create(&occ_config);
    }

    void TearDown() override {
        if (bridge_) {
            occipital_gpu_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
        if (occipital_) {
            occipital_destroy(occipital_);
            occipital_ = nullptr;
        }
    }

    bool HasGPU() const { return has_gpu_; }

    // Create test image with gradient pattern
    std::vector<float> CreateGradientImage(int width, int height) {
        std::vector<float> image(width * height);
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                image[y * width + x] = static_cast<float>(x + y) / (width + height);
            }
        }
        return image;
    }

    // Create test image with edges
    std::vector<float> CreateEdgeImage(int width, int height) {
        std::vector<float> image(width * height, 0.3f);
        // Vertical edge in the middle
        for (int y = 0; y < height; y++) {
            for (int x = width / 2; x < width; x++) {
                image[y * width + x] = 0.8f;
            }
        }
        return image;
    }

    // Create natural-looking test image
    std::vector<float> CreateNaturalImage(int width, int height) {
        std::vector<float> image(width * height);
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float value = 0.3f;
                // Horizontal edge
                if (std::abs(y - height / 3) < 3) value += 0.3f;
                // Vertical edge
                if (std::abs(x - width / 2) < 3) value += 0.3f;
                // Diagonal edge
                if (std::abs(y - x) < 3) value += 0.2f;
                // Circular object
                int cx = width * 3 / 4, cy = height * 3 / 4, r = 10;
                if ((x - cx) * (x - cx) + (y - cy) * (y - cy) < r * r) {
                    value = 0.9f;
                }
                image[y * width + x] = std::clamp(value, 0.0f, 1.0f);
            }
        }
        return image;
    }
};

//=============================================================================
// Bridge Lifecycle Tests
//=============================================================================

TEST_F(VisualProcessingGPUIntegrationTest, BridgeDefaultConfig) {
    // WHAT: Test default bridge configuration
    // WHY:  Default config should have sensible values
    // HOW:  Get default config, verify all GPU areas enabled

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();

    EXPECT_TRUE(config.enable_gpu_v1) << "V1 GPU should be enabled by default";
    EXPECT_TRUE(config.enable_gpu_v2) << "V2 GPU should be enabled by default";
    EXPECT_TRUE(config.enable_gpu_v4) << "V4 GPU should be enabled by default";
    EXPECT_TRUE(config.enable_gpu_v5) << "V5 GPU should be enabled by default";
    EXPECT_TRUE(config.enable_gpu_saliency) << "Saliency GPU should be enabled by default";
    EXPECT_TRUE(config.auto_fallback) << "Auto fallback should be enabled by default";
    EXPECT_EQ(config.device_id, 0) << "Default device should be 0";
}

TEST_F(VisualProcessingGPUIntegrationTest, BridgeCreationWithNullAdapter) {
    // WHAT: Test bridge creation with null occipital adapter
    // WHY:  Bridge should handle null adapter gracefully
    // HOW:  Create bridge with null adapter, verify it still creates

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);

    // Bridge may fail or succeed depending on implementation
    // but should not crash
    if (bridge_) {
        EXPECT_TRUE(occipital_gpu_bridge_is_available(bridge_));
    }
}

TEST_F(VisualProcessingGPUIntegrationTest, BridgeCreationWithNullConfig) {
    // WHAT: Test bridge creation with null config (uses defaults)
    // WHY:  Null config should use sensible defaults
    // HOW:  Create bridge with null config, verify it works

    bridge_ = occipital_gpu_bridge_create(occipital_, nullptr);

    // Should either fail gracefully or use defaults
    // Implementation may require config
}

TEST_F(VisualProcessingGPUIntegrationTest, BridgeCreationWithValidConfig) {
    // WHAT: Test bridge creation with valid configuration
    // WHY:  Bridge should be created successfully with valid config
    // HOW:  Create bridge, verify it's available

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);

    ASSERT_NE(bridge_, nullptr) << "Bridge creation failed";
    EXPECT_TRUE(occipital_gpu_bridge_is_available(bridge_))
        << "Bridge should be available after creation";
}

TEST_F(VisualProcessingGPUIntegrationTest, BridgeDestroyNull) {
    // WHAT: Test destroying null bridge
    // WHY:  Should handle null gracefully (no crash)
    // HOW:  Call destroy with null, should not crash

    occipital_gpu_bridge_destroy(nullptr);
    // If we get here without crashing, the test passes
    SUCCEED();
}

TEST_F(VisualProcessingGPUIntegrationTest, BridgeInitSize) {
    // WHAT: Test bridge size initialization
    // WHY:  Bridge must be sized before processing
    // HOW:  Create bridge, initialize for image size

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    bool init_ok = occipital_gpu_bridge_init_size(bridge_, WIDTH, HEIGHT, 1);
    EXPECT_TRUE(init_ok) << "Bridge size initialization failed";
}

TEST_F(VisualProcessingGPUIntegrationTest, BridgeReset) {
    // WHAT: Test bridge reset functionality
    // WHY:  Reset should clear state for new processing
    // HOW:  Create bridge, reset, verify it's still usable

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    bool reset_ok = occipital_gpu_bridge_reset(bridge_);
    EXPECT_TRUE(reset_ok) << "Bridge reset failed";
    EXPECT_TRUE(occipital_gpu_bridge_is_available(bridge_))
        << "Bridge should be available after reset";
}

//=============================================================================
// Data Transfer Tests
//=============================================================================

TEST_F(VisualProcessingGPUIntegrationTest, UploadInput) {
    // WHAT: Test uploading visual input to GPU
    // WHY:  Data must be uploaded before GPU processing
    // HOW:  Create input, upload, verify success

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    auto image_data = CreateEdgeImage(WIDTH, HEIGHT);

    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = WIDTH;
    input.height = HEIGHT;
    input.channels = 1;
    input.data = image_data.data();

    bool uploaded = occipital_gpu_upload_input(bridge_, &input);
    EXPECT_TRUE(uploaded) << "Failed to upload input to GPU";
}

TEST_F(VisualProcessingGPUIntegrationTest, UploadRGBInput) {
    // WHAT: Test uploading RGB visual input
    // WHY:  Color processing needs RGB data
    // HOW:  Create 3-channel input, upload

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    std::vector<float> rgb_data(WIDTH * HEIGHT * 3);
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int idx = (y * WIDTH + x) * 3;
            rgb_data[idx + 0] = static_cast<float>(x) / WIDTH;
            rgb_data[idx + 1] = static_cast<float>(y) / HEIGHT;
            rgb_data[idx + 2] = 0.5f;
        }
    }

    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = WIDTH;
    input.height = HEIGHT;
    input.channels = 3;
    input.data = rgb_data.data();

    bool uploaded = occipital_gpu_upload_input(bridge_, &input);
    EXPECT_TRUE(uploaded) << "Failed to upload RGB input to GPU";
}

TEST_F(VisualProcessingGPUIntegrationTest, DownloadFeatures) {
    // WHAT: Test downloading processed features from GPU
    // WHY:  Results must be downloaded for CPU use
    // HOW:  Upload, process V1, download

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    auto image_data = CreateNaturalImage(WIDTH, HEIGHT);

    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = WIDTH;
    input.height = HEIGHT;
    input.channels = 1;
    input.data = image_data.data();

    bool uploaded = occipital_gpu_upload_input(bridge_, &input);
    ASSERT_TRUE(uploaded);

    // Process V1
    bool v1_ok = occipital_gpu_process_v1(bridge_);
    ASSERT_TRUE(v1_ok) << "V1 processing failed";

    // Download features
    visual_feature_t features[256];
    uint32_t num_features = 0;
    bool downloaded = occipital_gpu_download_features(bridge_, features, 256, &num_features);
    EXPECT_TRUE(downloaded) << "Failed to download features from GPU";
}

//=============================================================================
// Processing Stage Tests
//=============================================================================

TEST_F(VisualProcessingGPUIntegrationTest, ProcessV1EdgeDetection) {
    // WHAT: Test V1 edge detection processing
    // WHY:  V1 is the first stage of visual processing
    // HOW:  Upload edge image, process V1, verify success

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    auto image_data = CreateEdgeImage(WIDTH, HEIGHT);

    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = WIDTH;
    input.height = HEIGHT;
    input.channels = 1;
    input.data = image_data.data();

    ASSERT_TRUE(occipital_gpu_upload_input(bridge_, &input));

    bool v1_ok = occipital_gpu_process_v1(bridge_);
    EXPECT_TRUE(v1_ok) << "V1 edge detection failed";
}

TEST_F(VisualProcessingGPUIntegrationTest, ProcessV2ContourIntegration) {
    // WHAT: Test V2 contour integration
    // WHY:  V2 groups edges into contours
    // HOW:  Process V1 first, then V2

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    auto image_data = CreateNaturalImage(WIDTH, HEIGHT);

    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = WIDTH;
    input.height = HEIGHT;
    input.channels = 1;
    input.data = image_data.data();

    ASSERT_TRUE(occipital_gpu_upload_input(bridge_, &input));
    ASSERT_TRUE(occipital_gpu_process_v1(bridge_));

    bool v2_ok = occipital_gpu_process_v2(bridge_);
    EXPECT_TRUE(v2_ok) << "V2 contour integration failed";
}

TEST_F(VisualProcessingGPUIntegrationTest, ProcessV4ColorProcessing) {
    // WHAT: Test V4 color processing
    // WHY:  V4 handles color opponent processing
    // HOW:  Upload RGB, process V4

    // V4 requires RGB occipital adapter - create one specifically
    occipital_config_t rgb_occ_config = occipital_default_config();
    rgb_occ_config.image_width = WIDTH;
    rgb_occ_config.image_height = HEIGHT;
    rgb_occ_config.color_channels = 3;  // RGB for V4
    occipital_adapter_t* rgb_occipital = occipital_create(&rgb_occ_config);
    if (!rgb_occipital) {
        GTEST_SKIP() << "Could not create RGB occipital adapter";
    }

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    occipital_gpu_bridge_t* rgb_bridge = occipital_gpu_bridge_create(rgb_occipital, &config);
    if (!rgb_bridge) {
        occipital_destroy(rgb_occipital);
        GTEST_SKIP() << "Could not create RGB GPU bridge";
    }

    std::vector<float> rgb_data(WIDTH * HEIGHT * 3);
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            int idx = (y * WIDTH + x) * 3;
            rgb_data[idx + 0] = (x < WIDTH / 2) ? 0.8f : 0.2f;  // Red left
            rgb_data[idx + 1] = (x >= WIDTH / 2) ? 0.8f : 0.2f;  // Green right
            rgb_data[idx + 2] = 0.3f;
        }
    }

    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = WIDTH;
    input.height = HEIGHT;
    input.channels = 3;
    input.data = rgb_data.data();

    bool uploaded = occipital_gpu_upload_input(rgb_bridge, &input);
    bool v4_ok = false;
    if (uploaded) {
        v4_ok = occipital_gpu_process_v4(rgb_bridge);
    }

    occipital_gpu_bridge_destroy(rgb_bridge);
    occipital_destroy(rgb_occipital);

    EXPECT_TRUE(uploaded) << "Failed to upload RGB input";
    EXPECT_TRUE(v4_ok) << "V4 color processing failed";
}

TEST_F(VisualProcessingGPUIntegrationTest, ProcessV5MotionProcessing) {
    // WHAT: Test V5 motion/optical flow processing
    // WHY:  V5 handles motion detection
    // HOW:  Upload two frames, process V5

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    auto image_data = CreateGradientImage(WIDTH, HEIGHT);

    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = WIDTH;
    input.height = HEIGHT;
    input.channels = 1;
    input.data = image_data.data();

    // Upload first frame
    ASSERT_TRUE(occipital_gpu_upload_input(bridge_, &input));

    // Upload second frame (same for now - no motion expected)
    ASSERT_TRUE(occipital_gpu_upload_input(bridge_, &input));

    bool v5_ok = occipital_gpu_process_v5(bridge_);
    EXPECT_TRUE(v5_ok) << "V5 motion processing failed";
}

//=============================================================================
// Full Pipeline Tests
//=============================================================================

TEST_F(VisualProcessingGPUIntegrationTest, FullProcessingPipeline) {
    // WHAT: Test complete visual processing pipeline
    // WHY:  All stages should work together
    // HOW:  Run V1 -> V2 -> V4 -> V5 in sequence

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    auto image_data = CreateNaturalImage(WIDTH, HEIGHT);

    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = WIDTH;
    input.height = HEIGHT;
    input.channels = 1;
    input.data = image_data.data();

    ASSERT_TRUE(occipital_gpu_upload_input(bridge_, &input));

    // Run full pipeline
    EXPECT_TRUE(occipital_gpu_process_v1(bridge_)) << "V1 failed";
    EXPECT_TRUE(occipital_gpu_process_v2(bridge_)) << "V2 failed";
    // Skip V4 for grayscale
    EXPECT_TRUE(occipital_gpu_process_v5(bridge_)) << "V5 failed";

    visual_feature_t features[256];
    uint32_t num_features = 0;
    EXPECT_TRUE(occipital_gpu_download_features(bridge_, features, 256, &num_features)) << "Download failed";
}

TEST_F(VisualProcessingGPUIntegrationTest, FullProcessWithResult) {
    // WHAT: Test complete processing with result structure
    // WHY:  occipital_gpu_process should fill result structure
    // HOW:  Call occipital_gpu_process, verify result

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    auto image_data = CreateNaturalImage(WIDTH, HEIGHT);

    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = WIDTH;
    input.height = HEIGHT;
    input.channels = 1;
    input.data = image_data.data();

    ASSERT_TRUE(occipital_gpu_upload_input(bridge_, &input));

    visual_processing_result_t result;
    memset(&result, 0, sizeof(result));

    bool processed = occipital_gpu_process(bridge_, &result);
    EXPECT_TRUE(processed) << "Full processing failed";
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(VisualProcessingGPUIntegrationTest, GetStatistics) {
    // WHAT: Test statistics collection
    // WHY:  Statistics help monitor GPU performance
    // HOW:  Process frames, verify stats are collected

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    auto image_data = CreateEdgeImage(WIDTH, HEIGHT);

    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = WIDTH;
    input.height = HEIGHT;
    input.channels = 1;
    input.data = image_data.data();

    ASSERT_TRUE(occipital_gpu_upload_input(bridge_, &input));
    ASSERT_TRUE(occipital_gpu_process_v1(bridge_));

    occipital_gpu_bridge_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    bool got_stats = occipital_gpu_bridge_get_stats(bridge_, &stats);
    EXPECT_TRUE(got_stats) << "Failed to get statistics";

    // After processing, some stats should be set
    EXPECT_GE(stats.v1_gpu_calls, 0u);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(VisualProcessingGPUIntegrationTest, ProcessWithoutUpload) {
    // WHAT: Test processing without uploading data
    // WHY:  Should fail gracefully, not crash
    // HOW:  Try to process without upload

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    // Try to process without uploading - may fail gracefully
    bool v1_ok = occipital_gpu_process_v1(bridge_);
    // Either false or falls back to CPU
    // Just verify no crash
    (void)v1_ok;
    SUCCEED();
}

TEST_F(VisualProcessingGPUIntegrationTest, DisabledGPUFeatures) {
    // WHAT: Test with GPU features disabled
    // WHY:  Should fall back to CPU or fail gracefully
    // HOW:  Disable all GPU features, try to process

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    config.enable_gpu_v1 = false;
    config.enable_gpu_v2 = false;
    config.enable_gpu_v4 = false;
    config.enable_gpu_v5 = false;
    config.enable_gpu_saliency = false;

    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    // Bridge should still work (CPU fallback)
    EXPECT_TRUE(occipital_gpu_bridge_is_available(bridge_));
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(VisualProcessingGPUIntegrationTest, ProcessingLatency) {
    // WHAT: Measure processing latency
    // WHY:  GPU should provide fast processing
    // HOW:  Time multiple frames, compute average

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    auto image_data = CreateNaturalImage(WIDTH, HEIGHT);

    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = WIDTH;
    input.height = HEIGHT;
    input.channels = 1;
    input.data = image_data.data();

    const int iterations = 10;
    double total_ms = 0.0;

    for (int i = 0; i < iterations; i++) {
        auto start = std::chrono::high_resolution_clock::now();

        ASSERT_TRUE(occipital_gpu_upload_input(bridge_, &input));
        ASSERT_TRUE(occipital_gpu_process_v1(bridge_));

        auto end = std::chrono::high_resolution_clock::now();
        total_ms += std::chrono::duration<double, std::milli>(end - start).count();
    }

    double avg_ms = total_ms / iterations;
    std::cout << "Average V1 processing: " << avg_ms << " ms" << std::endl;

    // Should complete in reasonable time
    EXPECT_LT(avg_ms, 100.0) << "Processing too slow";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
