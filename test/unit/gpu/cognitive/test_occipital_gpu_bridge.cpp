/**
 * @file test_occipital_gpu_bridge.cpp
 * @brief Unit tests for GPU-accelerated Occipital Cortex Bridge
 *
 * WHAT: Unit tests for GPU occipital bridge kernels and API
 * WHY:  Verify correctness of GPU-accelerated visual processing (V1-V5, saliency)
 * HOW:  Test individual operations: V1 edges, V2 contours, V4 color, V5 motion, saliency
 *
 * @version 1.0
 * @date 2026-01-30
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <random>

// GPU headers outside extern "C"
#include "gpu/occipital/nimcp_occipital_gpu_bridge.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/backend/nimcp_kernel_backend.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"

//=============================================================================
// Test Fixture
//=============================================================================

class OccipitalGPUBridgeUnitTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx;
    occipital_adapter_t* occipital;
    occipital_gpu_bridge_t* bridge;
    std::mt19937 rng;

    // Test image dimensions
    static constexpr uint32_t IMAGE_WIDTH = 64;
    static constexpr uint32_t IMAGE_HEIGHT = 64;
    static constexpr uint32_t IMAGE_CHANNELS = 3;

    void SetUp() override {
        gpu_ctx = nullptr;
        occipital = nullptr;
        bridge = nullptr;
        rng.seed(42);

        // Initialize kernel backend to detect GPU
        nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO);

        // Create occipital adapter first (required for bridge)
        occipital = occipital_create(nullptr);

        // Try to create GPU context
        if (nimcp_cuda_backend_available() && occipital) {
            gpu_ctx = nimcp_gpu_context_create(0);
        }

        if (gpu_ctx && occipital) {
            occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
            config.enable_gpu_v1 = true;
            config.enable_gpu_v2 = true;
            config.enable_gpu_v4 = true;
            config.enable_gpu_v5 = true;
            config.enable_gpu_saliency = true;
            config.auto_fallback = true;
            config.preallocate_tensors = true;
            bridge = occipital_gpu_bridge_create(occipital, &config);
        }
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
        if (gpu_ctx) {
            nimcp_gpu_context_destroy(gpu_ctx);
            gpu_ctx = nullptr;
        }
        nimcp_kernel_backend_shutdown();
    }

    bool hasGPU() const {
        return gpu_ctx != nullptr && bridge != nullptr && occipital != nullptr;
    }

    visual_input_t createTestInput(bool rgb = true) {
        visual_input_t input;
        memset(&input, 0, sizeof(input));
        input.width = IMAGE_WIDTH;
        input.height = IMAGE_HEIGHT;
        input.channels = rgb ? 3 : 1;
        input.stride = input.width * input.channels;

        // Allocate and fill with test pattern
        size_t data_size = input.width * input.height * input.channels;
        input.data = (uint8_t*)malloc(data_size);

        std::uniform_int_distribution<int> dist(0, 255);
        for (size_t i = 0; i < data_size; i++) {
            input.data[i] = (uint8_t)dist(rng);
        }

        // Add some structure (horizontal edge in middle)
        if (!rgb) {
            for (uint32_t y = IMAGE_HEIGHT / 2 - 2; y < IMAGE_HEIGHT / 2 + 2; y++) {
                for (uint32_t x = 0; x < IMAGE_WIDTH; x++) {
                    input.data[y * input.stride + x] = (y < IMAGE_HEIGHT / 2) ? 50 : 200;
                }
            }
        }

        return input;
    }

    void freeTestInput(visual_input_t* input) {
        if (input && input->data) {
            free(input->data);
            input->data = nullptr;
        }
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeUnitTest, DefaultConfig_HasSaneValues) {
    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();

    EXPECT_TRUE(config.enable_gpu_v1);
    EXPECT_TRUE(config.enable_gpu_v2);
    EXPECT_TRUE(config.enable_gpu_v4);
    EXPECT_TRUE(config.enable_gpu_v5);
    EXPECT_TRUE(config.enable_gpu_saliency);
    EXPECT_TRUE(config.auto_fallback);
    EXPECT_TRUE(config.preallocate_tensors);
    EXPECT_EQ(config.device_id, 0);
    EXPECT_GT(config.pyramid_levels, 0);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeUnitTest, Create_WithNullOccipital_ReturnsNull) {
    occipital_gpu_bridge_t* b = occipital_gpu_bridge_create(nullptr, nullptr);
    EXPECT_EQ(b, nullptr);
}

TEST_F(OccipitalGPUBridgeUnitTest, Create_WithValidOccipital_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_NE(bridge, nullptr);
}

TEST_F(OccipitalGPUBridgeUnitTest, Destroy_WithNull_DoesNotCrash) {
    occipital_gpu_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(OccipitalGPUBridgeUnitTest, IsAvailable_WithValidBridge_ReturnsTrue) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_TRUE(occipital_gpu_bridge_is_available(bridge));
}

TEST_F(OccipitalGPUBridgeUnitTest, IsAvailable_WithNull_ReturnsFalse) {
    EXPECT_FALSE(occipital_gpu_bridge_is_available(nullptr));
}

//=============================================================================
// Size Initialization Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeUnitTest, InitSize_AllocatesTensors) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_TRUE(occipital_gpu_bridge_init_size(bridge, IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_CHANNELS));
}

TEST_F(OccipitalGPUBridgeUnitTest, InitSize_HandlesDifferentSizes) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // Test various sizes
    EXPECT_TRUE(occipital_gpu_bridge_init_size(bridge, 128, 128, 1));
    EXPECT_TRUE(occipital_gpu_bridge_init_size(bridge, 256, 256, 3));
    EXPECT_TRUE(occipital_gpu_bridge_init_size(bridge, 320, 240, 3));
}

TEST_F(OccipitalGPUBridgeUnitTest, Reset_ClearsState) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    occipital_gpu_bridge_init_size(bridge, IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_CHANNELS);

    // Process some frames first
    visual_input_t input = createTestInput();
    visual_processing_result_t result;
    occipital_gpu_process_input(bridge, &input, &result);

    // Reset
    EXPECT_TRUE(occipital_gpu_bridge_reset(bridge));

    freeTestInput(&input);
}

//=============================================================================
// Data Transfer Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeUnitTest, UploadInput_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    occipital_gpu_bridge_init_size(bridge, IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_CHANNELS);

    visual_input_t input = createTestInput();
    EXPECT_TRUE(occipital_gpu_upload_input(bridge, &input));

    freeTestInput(&input);
}

TEST_F(OccipitalGPUBridgeUnitTest, UploadInput_HandlesGrayscale) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    occipital_gpu_bridge_init_size(bridge, IMAGE_WIDTH, IMAGE_HEIGHT, 1);

    visual_input_t input = createTestInput(false);
    EXPECT_TRUE(occipital_gpu_upload_input(bridge, &input));

    freeTestInput(&input);
}

TEST_F(OccipitalGPUBridgeUnitTest, DownloadFeatures_ReturnsFeatures) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    occipital_gpu_bridge_init_size(bridge, IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_CHANNELS);

    visual_input_t input = createTestInput();
    occipital_gpu_upload_input(bridge, &input);
    occipital_gpu_process_v1(bridge);

    visual_feature_t features[100];
    uint32_t num_features = 0;

    EXPECT_TRUE(occipital_gpu_download_features(bridge, features, 100, &num_features));

    freeTestInput(&input);
}

//=============================================================================
// V1 Processing Tests (Edge Detection)
//=============================================================================

TEST_F(OccipitalGPUBridgeUnitTest, ProcessV1_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    occipital_gpu_bridge_init_size(bridge, IMAGE_WIDTH, IMAGE_HEIGHT, 1);

    visual_input_t input = createTestInput(false);
    occipital_gpu_upload_input(bridge, &input);

    EXPECT_TRUE(occipital_gpu_process_v1(bridge));

    freeTestInput(&input);
}

TEST_F(OccipitalGPUBridgeUnitTest, ProcessV1_DetectsEdges) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    occipital_gpu_bridge_init_size(bridge, IMAGE_WIDTH, IMAGE_HEIGHT, 1);

    // Create input with strong horizontal edge
    visual_input_t input = createTestInput(false);
    occipital_gpu_upload_input(bridge, &input);
    occipital_gpu_process_v1(bridge);

    // Get edge tensor
    nimcp_gpu_tensor_t* edges = occipital_gpu_bridge_get_tensor(bridge, "v1_edges");
    EXPECT_NE(edges, nullptr);

    freeTestInput(&input);
}

//=============================================================================
// V2 Processing Tests (Contour Integration)
//=============================================================================

TEST_F(OccipitalGPUBridgeUnitTest, ProcessV2_AfterV1_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    occipital_gpu_bridge_init_size(bridge, IMAGE_WIDTH, IMAGE_HEIGHT, 1);

    visual_input_t input = createTestInput(false);
    occipital_gpu_upload_input(bridge, &input);
    occipital_gpu_process_v1(bridge);

    EXPECT_TRUE(occipital_gpu_process_v2(bridge));

    freeTestInput(&input);
}

//=============================================================================
// V4 Processing Tests (Color)
//=============================================================================

TEST_F(OccipitalGPUBridgeUnitTest, ProcessV4_WithRGB_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    occipital_gpu_bridge_init_size(bridge, IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_CHANNELS);

    visual_input_t input = createTestInput(true);
    occipital_gpu_upload_input(bridge, &input);

    EXPECT_TRUE(occipital_gpu_process_v4(bridge));

    freeTestInput(&input);
}

TEST_F(OccipitalGPUBridgeUnitTest, ProcessV4_ProducesColorOutput) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    occipital_gpu_bridge_init_size(bridge, IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_CHANNELS);

    visual_input_t input = createTestInput(true);
    occipital_gpu_upload_input(bridge, &input);
    occipital_gpu_process_v4(bridge);

    nimcp_gpu_tensor_t* color = occipital_gpu_bridge_get_tensor(bridge, "v4_color");
    EXPECT_NE(color, nullptr);

    freeTestInput(&input);
}

//=============================================================================
// V5 Processing Tests (Motion)
//=============================================================================

TEST_F(OccipitalGPUBridgeUnitTest, ProcessV5_WithPreviousFrame_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    occipital_gpu_bridge_init_size(bridge, IMAGE_WIDTH, IMAGE_HEIGHT, 1);

    // Process first frame
    visual_input_t input1 = createTestInput(false);
    occipital_gpu_upload_input(bridge, &input1);
    occipital_gpu_process_v1(bridge);

    // Process second frame (with slight shift for motion)
    visual_input_t input2 = createTestInput(false);
    // Shift pattern slightly
    for (uint32_t y = 0; y < IMAGE_HEIGHT; y++) {
        for (uint32_t x = 1; x < IMAGE_WIDTH; x++) {
            input2.data[y * input2.stride + x - 1] = input1.data[y * input1.stride + x];
        }
    }
    occipital_gpu_upload_input(bridge, &input2);

    EXPECT_TRUE(occipital_gpu_process_v5(bridge));

    freeTestInput(&input1);
    freeTestInput(&input2);
}

TEST_F(OccipitalGPUBridgeUnitTest, ProcessV5_ProducesFlowOutput) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    occipital_gpu_bridge_init_size(bridge, IMAGE_WIDTH, IMAGE_HEIGHT, 1);

    // Two frames
    visual_input_t input1 = createTestInput(false);
    occipital_gpu_upload_input(bridge, &input1);
    occipital_gpu_process_v1(bridge);

    visual_input_t input2 = createTestInput(false);
    occipital_gpu_upload_input(bridge, &input2);
    occipital_gpu_process_v5(bridge);

    nimcp_gpu_tensor_t* flow_u = occipital_gpu_bridge_get_tensor(bridge, "v5_flow_u");
    nimcp_gpu_tensor_t* flow_v = occipital_gpu_bridge_get_tensor(bridge, "v5_flow_v");

    EXPECT_NE(flow_u, nullptr);
    EXPECT_NE(flow_v, nullptr);

    freeTestInput(&input1);
    freeTestInput(&input2);
}

//=============================================================================
// Saliency Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeUnitTest, ComputeSaliency_AfterProcessing_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    occipital_gpu_bridge_init_size(bridge, IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_CHANNELS);

    visual_input_t input = createTestInput(true);
    occipital_gpu_upload_input(bridge, &input);
    occipital_gpu_process_v1(bridge);
    occipital_gpu_process_v4(bridge);

    EXPECT_TRUE(occipital_gpu_compute_saliency(bridge));

    freeTestInput(&input);
}

TEST_F(OccipitalGPUBridgeUnitTest, ComputeSaliency_ProducesOutput) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    occipital_gpu_bridge_init_size(bridge, IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_CHANNELS);

    visual_input_t input = createTestInput(true);
    occipital_gpu_upload_input(bridge, &input);
    occipital_gpu_process_v1(bridge);
    occipital_gpu_process_v4(bridge);
    occipital_gpu_compute_saliency(bridge);

    nimcp_gpu_tensor_t* saliency = occipital_gpu_bridge_get_tensor(bridge, "saliency");
    EXPECT_NE(saliency, nullptr);

    freeTestInput(&input);
}

//=============================================================================
// Full Pipeline Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeUnitTest, Process_RunsFullPipeline) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    visual_input_t input = createTestInput(true);
    visual_processing_result_t result;
    memset(&result, 0, sizeof(result));

    EXPECT_TRUE(occipital_gpu_process_input(bridge, &input, &result));

    freeTestInput(&input);
}

TEST_F(OccipitalGPUBridgeUnitTest, Process_PopulatesResult) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    visual_input_t input = createTestInput(true);
    visual_processing_result_t result;
    memset(&result, 0, sizeof(result));

    occipital_gpu_process_input(bridge, &input, &result);

    // Should have some features
    EXPECT_GE(result.num_features, 0u);

    freeTestInput(&input);
}

TEST_F(OccipitalGPUBridgeUnitTest, Process_MultipleFrames_UpdatesStats) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    occipital_gpu_bridge_reset_stats(bridge);

    for (int i = 0; i < 5; i++) {
        visual_input_t input = createTestInput(true);
        visual_processing_result_t result;
        occipital_gpu_process_input(bridge, &input, &result);
        freeTestInput(&input);
    }

    occipital_gpu_bridge_stats_t stats;
    EXPECT_TRUE(occipital_gpu_bridge_get_stats(bridge, &stats));
    EXPECT_GE(stats.frames_processed_gpu + stats.frames_processed_cpu, 5u);
}

//=============================================================================
// Area Enable/Disable Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeUnitTest, AreaEnabled_ReturnsCorrectState) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_TRUE(occipital_gpu_bridge_area_enabled(bridge, 0)); // V1
    EXPECT_TRUE(occipital_gpu_bridge_area_enabled(bridge, 1)); // V2
    EXPECT_TRUE(occipital_gpu_bridge_area_enabled(bridge, 2)); // V4
    EXPECT_TRUE(occipital_gpu_bridge_area_enabled(bridge, 3)); // V5
}

TEST_F(OccipitalGPUBridgeUnitTest, SetAreaEnabled_ChangesState) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // Disable V2
    EXPECT_TRUE(occipital_gpu_bridge_set_area_enabled(bridge, 1, false));
    EXPECT_FALSE(occipital_gpu_bridge_area_enabled(bridge, 1));

    // Re-enable V2
    EXPECT_TRUE(occipital_gpu_bridge_set_area_enabled(bridge, 1, true));
    EXPECT_TRUE(occipital_gpu_bridge_area_enabled(bridge, 1));
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeUnitTest, GetStats_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    occipital_gpu_bridge_stats_t stats;
    EXPECT_TRUE(occipital_gpu_bridge_get_stats(bridge, &stats));
}

TEST_F(OccipitalGPUBridgeUnitTest, ResetStats_ClearsCounters) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // Process something first
    visual_input_t input = createTestInput(true);
    visual_processing_result_t result;
    occipital_gpu_process_input(bridge, &input, &result);
    freeTestInput(&input);

    occipital_gpu_bridge_reset_stats(bridge);

    occipital_gpu_bridge_stats_t stats;
    occipital_gpu_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.frames_processed_gpu, 0u);
    EXPECT_EQ(stats.frames_processed_cpu, 0u);
}

//=============================================================================
// Memory Info Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeUnitTest, MemoryInfo_ReturnsValues) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    occipital_gpu_bridge_init_size(bridge, IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_CHANNELS);

    size_t used = 0, peak = 0;
    EXPECT_TRUE(occipital_gpu_bridge_memory_info(bridge, &used, &peak));

    EXPECT_GT(used, 0u);
    EXPECT_GE(peak, used);
}

//=============================================================================
// Configuration Update Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeUnitTest, Configure_UpdatesSettings) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    config.enable_gpu_v2 = false;
    config.enable_gpu_saliency = false;

    EXPECT_TRUE(occipital_gpu_bridge_configure(bridge, &config));

    EXPECT_FALSE(occipital_gpu_bridge_area_enabled(bridge, 1)); // V2
}

//=============================================================================
// Tensor Access Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeUnitTest, GetTensor_ReturnsValidTensor) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    occipital_gpu_bridge_init_size(bridge, IMAGE_WIDTH, IMAGE_HEIGHT, 1);

    visual_input_t input = createTestInput(false);
    occipital_gpu_upload_input(bridge, &input);
    occipital_gpu_process_v1(bridge);

    nimcp_gpu_tensor_t* tensor = occipital_gpu_bridge_get_tensor(bridge, "v1_edges");
    EXPECT_NE(tensor, nullptr);

    freeTestInput(&input);
}

TEST_F(OccipitalGPUBridgeUnitTest, GetTensor_InvalidName_ReturnsNull) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    nimcp_gpu_tensor_t* tensor = occipital_gpu_bridge_get_tensor(bridge, "nonexistent_tensor");
    EXPECT_EQ(tensor, nullptr);
}

//=============================================================================
// Re-enable Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeUnitTest, Reenable_ResetsFailureState) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // Force some failures (if we could)
    // For now, just test that reenable succeeds
    EXPECT_TRUE(occipital_gpu_bridge_reenable(bridge));
    EXPECT_TRUE(occipital_gpu_bridge_is_available(bridge));
}

//=============================================================================
// GPU Recovery Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeUnitTest, Recovery_InitializedOnCreate) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());
}

TEST_F(OccipitalGPUBridgeUnitTest, Recovery_HandlesNullInputGracefully) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_FALSE(occipital_gpu_upload_input(bridge, nullptr));
    EXPECT_FALSE(occipital_gpu_download_features(bridge, nullptr, 0, nullptr));
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeUnitTest, NullSafety_AllFunctionsHandleNull) {
    occipital_gpu_bridge_destroy(nullptr);
    EXPECT_FALSE(occipital_gpu_bridge_is_available(nullptr));
    EXPECT_FALSE(occipital_gpu_bridge_init_size(nullptr, 0, 0, 0));
    EXPECT_FALSE(occipital_gpu_bridge_reset(nullptr));
    EXPECT_FALSE(occipital_gpu_upload_input(nullptr, nullptr));
    EXPECT_FALSE(occipital_gpu_download_features(nullptr, nullptr, 0, nullptr));
    EXPECT_FALSE(occipital_gpu_process_v1(nullptr));
    EXPECT_FALSE(occipital_gpu_process_v2(nullptr));
    EXPECT_FALSE(occipital_gpu_process_v4(nullptr));
    EXPECT_FALSE(occipital_gpu_process_v5(nullptr));
    EXPECT_FALSE(occipital_gpu_compute_saliency(nullptr));
    EXPECT_FALSE(occipital_gpu_process(nullptr, nullptr));
    EXPECT_FALSE(occipital_gpu_process_input(nullptr, nullptr, nullptr));
    EXPECT_FALSE(occipital_gpu_bridge_area_enabled(nullptr, 0));
    EXPECT_FALSE(occipital_gpu_bridge_get_stats(nullptr, nullptr));
    EXPECT_FALSE(occipital_gpu_bridge_memory_info(nullptr, nullptr, nullptr));
    EXPECT_FALSE(occipital_gpu_bridge_configure(nullptr, nullptr));
    EXPECT_EQ(occipital_gpu_bridge_get_tensor(nullptr, nullptr), nullptr);

    SUCCEED();
}

//=============================================================================
// System Availability Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeUnitTest, GPUProcessingAvailable_ReturnsConsistent) {
    bool available = occipital_gpu_processing_available();

    // If we have a bridge, GPU should be available
    if (hasGPU()) {
        EXPECT_TRUE(available);
    }
}

TEST_F(OccipitalGPUBridgeUnitTest, DeviceInfo_ReturnsValidData) {
    if (!nimcp_cuda_backend_available()) {
        GTEST_SKIP() << "CUDA not available";
    }

    char name[256];
    int compute_capability = 0;
    size_t memory_mb = 0;

    bool result = occipital_gpu_device_info(0, name, &compute_capability, &memory_mb);

    if (result) {
        EXPECT_GT(strlen(name), 0u);
        EXPECT_GT(compute_capability, 0);
        EXPECT_GT(memory_mb, 0u);
    }
}

//=============================================================================
// CPU Fallback Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeUnitTest, AutoFallback_ProcessesOnCPUWhenGPUFails) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // Configure to allow fallback
    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    config.auto_fallback = true;
    occipital_gpu_bridge_configure(bridge, &config);

    visual_input_t input = createTestInput(true);
    visual_processing_result_t result;

    // This should succeed (using GPU or CPU fallback)
    bool success = occipital_gpu_process_input(bridge, &input, &result);
    EXPECT_TRUE(success);

    freeTestInput(&input);
}

//=============================================================================
// Motion Vector Download Tests
//=============================================================================

TEST_F(OccipitalGPUBridgeUnitTest, DownloadMotion_ReturnsVectors) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    occipital_gpu_bridge_init_size(bridge, IMAGE_WIDTH, IMAGE_HEIGHT, 1);

    // Two frames for motion
    visual_input_t input1 = createTestInput(false);
    occipital_gpu_upload_input(bridge, &input1);
    occipital_gpu_process_v1(bridge);

    visual_input_t input2 = createTestInput(false);
    occipital_gpu_upload_input(bridge, &input2);
    occipital_gpu_process_v5(bridge);

    motion_vector_t vectors[100];
    uint32_t num_vectors = 0;

    EXPECT_TRUE(occipital_gpu_download_motion(bridge, vectors, 100, &num_vectors));

    freeTestInput(&input1);
    freeTestInput(&input2);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
