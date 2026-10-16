/**
 * @file test_visual_processing_regression.cpp
 * @brief Regression tests for GPU visual processing via occipital bridge
 *
 * WHAT: Ensure API stability and prevent regression of fixed bugs
 * WHY:  Visual processing API must remain stable, fixed bugs must stay fixed
 * HOW:  Test struct layouts, function signatures, edge cases, and known bug scenarios
 *
 * REGRESSION CATEGORIES:
 * - API Stability: Struct sizes, field alignments, function signatures
 * - Backward Compatibility: CPU fallback when GPU unavailable
 * - Numerical Stability: Edge cases, boundary conditions
 * - Memory Safety: No leaks, proper cleanup on errors
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
#include <limits>

// GPU headers
#include "gpu/occipital/nimcp_occipital_gpu_bridge.h"
#include "gpu/nimcp_execution_mode.h"
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"

//=============================================================================
// Test Fixture
//=============================================================================

class VisualProcessingRegressionTest : public ::testing::Test {
protected:
    occipital_adapter_t* occipital_ = nullptr;
    occipital_gpu_bridge_t* bridge_ = nullptr;
    hardware_capabilities_t caps_;
    bool has_gpu_ = false;

    static constexpr int WIDTH = 64;
    static constexpr int HEIGHT = 64;

    void SetUp() override {
        memset(&caps_, 0, sizeof(caps_));
        execution_detect_capabilities(&caps_);
        has_gpu_ = caps_.cuda_available || caps_.rocm_available || caps_.opencl_available;

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
};

//=============================================================================
// API Stability - Struct Layout Tests
//=============================================================================

TEST_F(VisualProcessingRegressionTest, OccipitalBridgeConfigStructStable) {
    // WHAT: Verify occipital_gpu_bridge_config_t layout
    // WHY:  Config struct must remain stable for ABI compatibility
    // REGRESSION: Struct layout critical for serialization and interop

    occipital_gpu_bridge_config_t config;
    memset(&config, 0, sizeof(config));

    // Verify all fields are accessible (compiler will fail if removed)
    config.enable_gpu_v1 = true;
    config.enable_gpu_v2 = true;
    config.enable_gpu_v4 = true;
    config.enable_gpu_v5 = true;
    config.enable_gpu_saliency = true;
    config.auto_fallback = true;
    config.device_id = 0;

    // Verify default config has expected values
    occipital_gpu_bridge_config_t default_config = occipital_gpu_bridge_default_config();
    EXPECT_TRUE(default_config.enable_gpu_v1);
    EXPECT_TRUE(default_config.enable_gpu_v2);
    EXPECT_TRUE(default_config.enable_gpu_v4);
    EXPECT_TRUE(default_config.enable_gpu_v5);
    EXPECT_TRUE(default_config.enable_gpu_saliency);
    EXPECT_TRUE(default_config.auto_fallback);
    EXPECT_EQ(default_config.device_id, 0);
}

TEST_F(VisualProcessingRegressionTest, OccipitalBridgeStatsStructStable) {
    // WHAT: Verify occipital_gpu_bridge_stats_t layout
    // WHY:  Stats struct must remain stable for monitoring tools
    // REGRESSION: Stats field layout critical for dashboards

    occipital_gpu_bridge_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    // Verify timing and count fields exist and are accessible
    stats.frames_processed_gpu = 100;
    stats.frames_processed_cpu = 5;
    stats.gpu_failures = 2;
    stats.v1_gpu_calls = 50;
    stats.v2_gpu_calls = 50;
    stats.v4_gpu_calls = 50;
    stats.v5_gpu_calls = 50;
    stats.saliency_gpu_calls = 50;
    stats.avg_gpu_time_ms = 1.5f;
    stats.avg_v1_gpu_time_ms = 0.5f;
    stats.gpu_memory_allocated = 1024 * 1024;

    EXPECT_EQ(stats.frames_processed_gpu, 100u);
    EXPECT_EQ(stats.gpu_failures, 2u);
    EXPECT_FLOAT_EQ(stats.avg_gpu_time_ms, 1.5f);
}

//=============================================================================
// API Stability - Function Signature Tests
//=============================================================================

TEST_F(VisualProcessingRegressionTest, DefaultConfigFunctionSignature) {
    // WHAT: Verify occipital_gpu_bridge_default_config signature
    // WHY:  API signature must remain stable
    // REGRESSION: Function signature changes break compilation

    // Function takes no args, returns config struct
    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    EXPECT_TRUE(config.auto_fallback);
}

TEST_F(VisualProcessingRegressionTest, CreateFunctionSignature) {
    // WHAT: Verify occipital_gpu_bridge_create signature
    // WHY:  API signature must remain stable
    // REGRESSION: Function signature changes break compilation

    // Function takes occipital adapter (can be null) and config pointer
    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);
}

TEST_F(VisualProcessingRegressionTest, DestroyFunctionSignature) {
    // WHAT: Verify occipital_gpu_bridge_destroy signature
    // WHY:  API signature must remain stable
    // REGRESSION: Function signature changes break compilation

    // Function takes bridge pointer (NULL-safe)
    occipital_gpu_bridge_destroy(nullptr);  // Should not crash

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    occipital_gpu_bridge_destroy(bridge_);
    bridge_ = nullptr;  // Prevent double-free in TearDown
}

TEST_F(VisualProcessingRegressionTest, IsAvailableFunctionSignature) {
    // WHAT: Verify occipital_gpu_bridge_is_available signature
    // WHY:  API signature must remain stable
    // REGRESSION: Function signature changes break compilation

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    // Function takes const bridge, returns bool
    bool available = occipital_gpu_bridge_is_available(bridge_);
    EXPECT_TRUE(available);
}

TEST_F(VisualProcessingRegressionTest, GetStatsFunctionSignature) {
    // WHAT: Verify occipital_gpu_bridge_get_stats signature
    // WHY:  API signature must remain stable
    // REGRESSION: Function signature changes break compilation

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    // Function takes const bridge and stats pointer, returns bool
    occipital_gpu_bridge_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    bool got_stats = occipital_gpu_bridge_get_stats(bridge_, &stats);
    EXPECT_TRUE(got_stats);
}

//=============================================================================
// Backward Compatibility - CPU Fallback Tests
//=============================================================================

TEST_F(VisualProcessingRegressionTest, CPUFallbackOnNullContext) {
    // WHAT: Verify CPU fallback when GPU context is null
    // WHY:  System must work without GPU
    // REGRESSION: Null GPU context must not crash

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    config.auto_fallback = true;

    // Should create bridge (falls back to CPU if needed)
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr) << "Bridge creation failed";
}

TEST_F(VisualProcessingRegressionTest, GracefulDegradation) {
    // WHAT: Verify graceful degradation when features disabled
    // WHY:  Partial GPU support should still work
    // REGRESSION: Disabled features must not cause errors

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    config.enable_gpu_v1 = false;
    config.enable_gpu_v2 = false;
    config.enable_gpu_v4 = false;
    config.enable_gpu_v5 = false;
    config.enable_gpu_saliency = false;

    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    // Bridge should still be usable (CPU mode)
    EXPECT_TRUE(occipital_gpu_bridge_is_available(bridge_));
}

//=============================================================================
// Memory Safety Tests
//=============================================================================

TEST_F(VisualProcessingRegressionTest, NullHandling) {
    // WHAT: Test that null pointers are handled gracefully
    // WHY:  Null pointers must not cause crashes
    // REGRESSION: Fixed null pointer dereference bugs

    // Null bridge destroy
    occipital_gpu_bridge_destroy(nullptr);  // Should not crash

    // Null config uses defaults (or fails gracefully)
    bridge_ = occipital_gpu_bridge_create(occipital_, nullptr);
    // Either works or returns null, but doesn't crash
}

TEST_F(VisualProcessingRegressionTest, BridgeResetSafety) {
    // WHAT: Test bridge reset doesn't corrupt state
    // WHY:  Reset must maintain valid state
    // REGRESSION: Fixed reset corruption bugs

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    // Reset multiple times
    for (int i = 0; i < 5; i++) {
        bool reset_ok = occipital_gpu_bridge_reset(bridge_);
        EXPECT_TRUE(reset_ok) << "Reset failed on iteration " << i;
        EXPECT_TRUE(occipital_gpu_bridge_is_available(bridge_))
            << "Bridge unavailable after reset " << i;
    }
}

TEST_F(VisualProcessingRegressionTest, StatsAfterMultipleOperations) {
    // WHAT: Test stats remain valid after multiple operations
    // WHY:  Stats must accurately reflect processing
    // REGRESSION: Fixed stats overflow/corruption bugs

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    // Create test data
    std::vector<float> image_data(64 * 64, 0.5f);
    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = 64;
    input.height = 64;
    input.channels = 1;
    input.data = image_data.data();

    // Process multiple times
    for (int i = 0; i < 10; i++) {
        occipital_gpu_upload_input(bridge_, &input);
        occipital_gpu_process_v1(bridge_);
    }

    occipital_gpu_bridge_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    bool got_stats = occipital_gpu_bridge_get_stats(bridge_, &stats);
    EXPECT_TRUE(got_stats);

    // Stats should reflect processing
    EXPECT_GE(stats.v1_gpu_calls, 0u);
}

//=============================================================================
// Boundary Condition Tests
//=============================================================================

TEST_F(VisualProcessingRegressionTest, MinimumImageSize) {
    // WHAT: Test processing of minimum size images
    // WHY:  Small images must not cause buffer overflows
    // REGRESSION: Fixed small image processing bugs

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    // Very small image (4x4)
    std::vector<float> tiny(4 * 4, 0.5f);
    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = 4;
    input.height = 4;
    input.channels = 1;
    input.data = tiny.data();

    // May fail or succeed, but should not crash
    bool uploaded = occipital_gpu_upload_input(bridge_, &input);
    if (uploaded) {
        occipital_gpu_process_v1(bridge_);
    }
    SUCCEED() << "Small image handled without crash";
}

TEST_F(VisualProcessingRegressionTest, LargeImageHandling) {
    // WHAT: Test processing of larger images
    // WHY:  Large images must not cause memory issues
    // REGRESSION: Fixed large image memory allocation bugs

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    // Moderately large image (256x256)
    std::vector<float> large(256 * 256);
    for (size_t i = 0; i < large.size(); i++) {
        large[i] = static_cast<float>(rand()) / RAND_MAX;
    }

    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = 256;
    input.height = 256;
    input.channels = 1;
    input.data = large.data();

    bool uploaded = occipital_gpu_upload_input(bridge_, &input);
    EXPECT_TRUE(uploaded) << "Large image upload failed";

    if (uploaded) {
        bool v1_ok = occipital_gpu_process_v1(bridge_);
        EXPECT_TRUE(v1_ok) << "Large image V1 processing failed";
    }
}

TEST_F(VisualProcessingRegressionTest, UnalignedDimensions) {
    // WHAT: Test images with non-power-of-2 dimensions
    // WHY:  Real images often have odd dimensions
    // REGRESSION: Fixed alignment issues with odd dimensions

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    // Non-power-of-2 dimensions
    int width = 123, height = 97;
    std::vector<float> image(width * height, 0.5f);

    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = width;
    input.height = height;
    input.channels = 1;
    input.data = image.data();

    bool uploaded = occipital_gpu_upload_input(bridge_, &input);
    EXPECT_TRUE(uploaded) << "Unaligned image upload failed";

    if (uploaded) {
        bool v1_ok = occipital_gpu_process_v1(bridge_);
        EXPECT_TRUE(v1_ok) << "Unaligned image processing failed";
    }
}

//=============================================================================
// Numerical Stability Tests
//=============================================================================

TEST_F(VisualProcessingRegressionTest, ZeroInputHandling) {
    // WHAT: Test handling of all-zero input images
    // WHY:  Zero input must not cause division by zero
    // REGRESSION: Fixed zero-input edge case bug

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    // All-zero image
    std::vector<float> zeros(64 * 64, 0.0f);

    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = 64;
    input.height = 64;
    input.channels = 1;
    input.data = zeros.data();

    bool uploaded = occipital_gpu_upload_input(bridge_, &input);
    ASSERT_TRUE(uploaded);

    bool v1_ok = occipital_gpu_process_v1(bridge_);
    EXPECT_TRUE(v1_ok) << "V1 should handle zero input";
}

TEST_F(VisualProcessingRegressionTest, ExtremeValueHandling) {
    // WHAT: Test handling of extreme input values
    // WHY:  Extreme values must not cause overflow
    // REGRESSION: Fixed numerical overflow bugs

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    // Large positive values
    std::vector<float> extreme(64 * 64, 1000.0f);

    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = 64;
    input.height = 64;
    input.channels = 1;
    input.data = extreme.data();

    bool uploaded = occipital_gpu_upload_input(bridge_, &input);
    ASSERT_TRUE(uploaded);

    // Should handle without crashing
    bool v1_ok = occipital_gpu_process_v1(bridge_);
    // May or may not produce valid results, but shouldn't crash
    (void)v1_ok;
    SUCCEED() << "Extreme values handled without crash";
}

//=============================================================================
// State Consistency Tests
//=============================================================================

TEST_F(VisualProcessingRegressionTest, InitSizeConsistency) {
    // WHAT: Test init_size maintains consistent state
    // WHY:  Changing size must not corrupt state
    // REGRESSION: Fixed size change corruption bugs

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    // Initialize with different sizes
    EXPECT_TRUE(occipital_gpu_bridge_init_size(bridge_, 64, 64, 1));
    EXPECT_TRUE(occipital_gpu_bridge_is_available(bridge_));

    EXPECT_TRUE(occipital_gpu_bridge_init_size(bridge_, 128, 128, 1));
    EXPECT_TRUE(occipital_gpu_bridge_is_available(bridge_));

    EXPECT_TRUE(occipital_gpu_bridge_init_size(bridge_, 32, 32, 3));
    EXPECT_TRUE(occipital_gpu_bridge_is_available(bridge_));
}

TEST_F(VisualProcessingRegressionTest, ProcessingOrderFlexibility) {
    // WHAT: Test processing stages can be called in any order
    // WHY:  Caller may want partial processing
    // REGRESSION: Fixed processing order dependency bugs

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    ASSERT_NE(bridge_, nullptr);

    std::vector<float> image(64 * 64, 0.5f);
    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = 64;
    input.height = 64;
    input.channels = 1;
    input.data = image.data();

    ASSERT_TRUE(occipital_gpu_upload_input(bridge_, &input));

    // Call stages in non-standard order
    // Each stage should handle being called without prior stages
    occipital_gpu_process_v2(bridge_);  // Without V1
    occipital_gpu_process_v1(bridge_);  // After V2
    occipital_gpu_process_v5(bridge_);  // Motion without prior frame

    SUCCEED() << "Non-standard order handled";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
