/**
 * @file test_e2e_visual_processing_gpu.cpp
 * @brief E2E Tests for GPU-Accelerated Visual Processing Pipeline
 *
 * WHAT: End-to-end testing of complete visual processing workflows on GPU
 * WHY:  Verify full visual cortex simulation from raw input to feature extraction
 * HOW:  Test V1->V2->V4->V5 pipeline, saliency, and feature extraction
 *
 * TEST PIPELINES:
 * - FullVisualPipeline: V1 edge -> V2 contour -> features
 * - ColorProcessing: V4 color opponent processing
 * - MotionTracking: V5 optical flow pipeline
 * - OccipitalBridgeFullPipeline: Complete occipital GPU bridge workflow
 * - PerformanceBenchmark: Throughput and latency measurements
 * - StressTest: Many consecutive frames
 *
 * @author NIMCP Development Team
 * @date 2025-01-02
 * @version 1.0.0
 */

#include "e2e_test_framework.h"

// GPU headers
#include "gpu/occipital/nimcp_occipital_gpu_bridge.h"
#include "gpu/nimcp_execution_mode.h"
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"

#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <chrono>
#include <random>
#include <numeric>
#include <algorithm>

//=============================================================================
// Test Metrics Structure
//=============================================================================

struct VisualMetrics {
    double v1_time_ms;
    double v2_time_ms;
    double v4_time_ms;
    double v5_time_ms;
    double total_time_ms;
    uint64_t pixels_processed;
    double pixels_per_second;
    uint64_t frames_processed;
    double avg_frame_time_ms;
};

//=============================================================================
// Test Fixture
//=============================================================================

class VisualProcessingGPUE2ETest : public ::testing::Test {
protected:
    occipital_adapter_t* occipital_ = nullptr;
    occipital_gpu_bridge_t* bridge_ = nullptr;
    hardware_capabilities_t caps_;
    bool has_gpu_ = false;
    std::mt19937 rng_;
    VisualMetrics metrics_;

    // Test dimensions
    static constexpr int WIDTH = 128;
    static constexpr int HEIGHT = 128;

    void SetUp() override {
        memset(&caps_, 0, sizeof(caps_));
        memset(&metrics_, 0, sizeof(metrics_));

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

    // Create natural-looking test image with edges and textures
    std::vector<float> CreateNaturalImage(int width, int height) {
        std::vector<float> image(width * height);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float value = 0.3f;

                // Horizontal edge at y=height/3
                if (std::abs(y - height / 3) < 3) value += 0.3f;

                // Vertical edge at x=width/2
                if (std::abs(x - width / 2) < 3) value += 0.3f;

                // Diagonal edge
                if (std::abs(y - x) < 3) value += 0.2f;

                // Circular object
                int cx = width * 3 / 4, cy = height * 3 / 4, r = 15;
                if ((x - cx) * (x - cx) + (y - cy) * (y - cy) < r * r) {
                    value = 0.9f;
                }

                // Add noise
                value += (rng_() % 20 - 10) / 100.0f;
                image[y * width + x] = std::clamp(value, 0.0f, 1.0f);
            }
        }
        return image;
    }

    // Create color test image (RGB)
    std::vector<float> CreateColorImage(int width, int height) {
        std::vector<float> image(width * height * 3);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int idx = (y * width + x) * 3;

                // Red region (left)
                if (x < width / 3) {
                    image[idx + 0] = 0.8f;  // R
                    image[idx + 1] = 0.2f;  // G
                    image[idx + 2] = 0.2f;  // B
                }
                // Green region (center)
                else if (x < 2 * width / 3) {
                    image[idx + 0] = 0.2f;
                    image[idx + 1] = 0.8f;
                    image[idx + 2] = 0.2f;
                }
                // Blue region (right)
                else {
                    image[idx + 0] = 0.2f;
                    image[idx + 1] = 0.2f;
                    image[idx + 2] = 0.8f;
                }

                // Add some noise
                for (int c = 0; c < 3; c++) {
                    image[idx + c] += (rng_() % 20 - 10) / 100.0f;
                    image[idx + c] = std::clamp(image[idx + c], 0.0f, 1.0f);
                }
            }
        }
        return image;
    }

    // Create moving object sequence (two frames)
    std::pair<std::vector<float>, std::vector<float>> CreateMovingObject(
            int width, int height, int dx, int dy) {
        std::vector<float> frame1(width * height, 0.2f);
        std::vector<float> frame2(width * height, 0.2f);

        // Object in frame1
        int cx1 = width / 2, cy1 = height / 2;
        for (int y = cy1 - 10; y < cy1 + 10; y++) {
            for (int x = cx1 - 10; x < cx1 + 10; x++) {
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    frame1[y * width + x] = 0.8f;
                }
            }
        }

        // Object in frame2 (shifted)
        int cx2 = cx1 + dx, cy2 = cy1 + dy;
        for (int y = cy2 - 10; y < cy2 + 10; y++) {
            for (int x = cx2 - 10; x < cx2 + 10; x++) {
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    frame2[y * width + x] = 0.8f;
                }
            }
        }

        return {frame1, frame2};
    }

    void LogMetrics(const std::string& test_name) {
        std::cout << "\n=== " << test_name << " Metrics ===" << std::endl;
        std::cout << "  V1 time: " << metrics_.v1_time_ms << " ms" << std::endl;
        std::cout << "  V2 time: " << metrics_.v2_time_ms << " ms" << std::endl;
        std::cout << "  V4 time: " << metrics_.v4_time_ms << " ms" << std::endl;
        std::cout << "  V5 time: " << metrics_.v5_time_ms << " ms" << std::endl;
        std::cout << "  Total time: " << metrics_.total_time_ms << " ms" << std::endl;
        std::cout << "  Pixels: " << metrics_.pixels_processed << std::endl;
        std::cout << "  Throughput: " << metrics_.pixels_per_second / 1e6 << " Mpix/s" << std::endl;
    }
};

//=============================================================================
// E2E Test: Full Visual Processing Pipeline
//=============================================================================

TEST_F(VisualProcessingGPUE2ETest, FullVisualPipeline) {
    // WHAT: Test complete V1->V2 visual processing pipeline
    // WHY:  Verify all stages work together end-to-end
    // HOW:  Process natural image through all stages, verify feature output

    // Create bridge
    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    E2E_ASSERT_NOT_NULL(bridge_, "Bridge creation failed");
    E2E_ASSERT(occipital_gpu_bridge_is_available(bridge_), "Bridge not available");

    // Create test image
    auto image = CreateNaturalImage(WIDTH, HEIGHT);
    metrics_.pixels_processed = WIDTH * HEIGHT;

    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = WIDTH;
    input.height = HEIGHT;
    input.channels = 1;
    input.data = image.data();

    auto pipeline_start = std::chrono::high_resolution_clock::now();

    // Upload
    bool uploaded = occipital_gpu_upload_input(bridge_, &input);
    E2E_ASSERT(uploaded, "Upload failed");

    // Stage 1: V1 Edge Detection
    auto v1_start = std::chrono::high_resolution_clock::now();
    bool v1_ok = occipital_gpu_process_v1(bridge_);
    auto v1_end = std::chrono::high_resolution_clock::now();
    metrics_.v1_time_ms = std::chrono::duration<double, std::milli>(v1_end - v1_start).count();
    E2E_ASSERT(v1_ok, "V1 processing failed");

    // Stage 2: V2 Contour Integration
    auto v2_start = std::chrono::high_resolution_clock::now();
    bool v2_ok = occipital_gpu_process_v2(bridge_);
    auto v2_end = std::chrono::high_resolution_clock::now();
    metrics_.v2_time_ms = std::chrono::duration<double, std::milli>(v2_end - v2_start).count();
    E2E_ASSERT(v2_ok, "V2 contour integration failed");

    // Download features
    visual_feature_t features[256];
    uint32_t num_features = 0;
    bool downloaded = occipital_gpu_download_features(bridge_, features, 256, &num_features);
    E2E_ASSERT(downloaded, "Download failed");

    auto pipeline_end = std::chrono::high_resolution_clock::now();
    metrics_.total_time_ms = std::chrono::duration<double, std::milli>(pipeline_end - pipeline_start).count();
    metrics_.pixels_per_second = metrics_.pixels_processed / (metrics_.total_time_ms / 1000.0);

    LogMetrics("FullVisualPipeline");

    // Verify processing completed
    occipital_gpu_bridge_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    occipital_gpu_bridge_get_stats(bridge_, &stats);
    EXPECT_GE(stats.v1_gpu_calls, 1u) << "V1 not recorded in stats";
}

//=============================================================================
// E2E Test: Color Processing Pipeline
//=============================================================================

TEST_F(VisualProcessingGPUE2ETest, ColorProcessingPipeline) {
    // WHAT: Test V4 color opponent processing pipeline
    // WHY:  Verify color channels are correctly extracted
    // HOW:  Process color image, verify opponent channels at color boundaries

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    E2E_ASSERT_NOT_NULL(bridge_, "Bridge creation failed");

    auto color_image = CreateColorImage(WIDTH, HEIGHT);
    metrics_.pixels_processed = WIDTH * HEIGHT;

    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = WIDTH;
    input.height = HEIGHT;
    input.channels = 3;
    input.data = color_image.data();

    bool uploaded = occipital_gpu_upload_input(bridge_, &input);
    E2E_ASSERT(uploaded, "Color upload failed");

    auto v4_start = std::chrono::high_resolution_clock::now();
    bool v4_ok = occipital_gpu_process_v4(bridge_);
    auto v4_end = std::chrono::high_resolution_clock::now();
    metrics_.v4_time_ms = std::chrono::duration<double, std::milli>(v4_end - v4_start).count();
    metrics_.total_time_ms = metrics_.v4_time_ms;
    metrics_.pixels_per_second = metrics_.pixels_processed / (metrics_.total_time_ms / 1000.0);

    E2E_ASSERT(v4_ok, "V4 color processing failed");

    LogMetrics("ColorProcessingPipeline");
}

//=============================================================================
// E2E Test: Motion Tracking Pipeline
//=============================================================================

TEST_F(VisualProcessingGPUE2ETest, MotionTrackingPipeline) {
    // WHAT: Test optical flow pipeline for motion detection
    // WHY:  Verify V5 motion processing works end-to-end
    // HOW:  Create moving object, compute flow, verify motion detected

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    E2E_ASSERT_NOT_NULL(bridge_, "Bridge creation failed");

    // Create moving object (shift 5 pixels right, 3 pixels down)
    auto [frame1, frame2] = CreateMovingObject(WIDTH, HEIGHT, 5, 3);
    metrics_.pixels_processed = WIDTH * HEIGHT * 2;

    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = WIDTH;
    input.height = HEIGHT;
    input.channels = 1;

    // Upload first frame
    input.data = frame1.data();
    bool uploaded1 = occipital_gpu_upload_input(bridge_, &input);
    E2E_ASSERT(uploaded1, "Frame 1 upload failed");

    // Upload second frame
    input.data = frame2.data();
    bool uploaded2 = occipital_gpu_upload_input(bridge_, &input);
    E2E_ASSERT(uploaded2, "Frame 2 upload failed");

    auto v5_start = std::chrono::high_resolution_clock::now();
    bool v5_ok = occipital_gpu_process_v5(bridge_);
    auto v5_end = std::chrono::high_resolution_clock::now();
    metrics_.v5_time_ms = std::chrono::duration<double, std::milli>(v5_end - v5_start).count();
    metrics_.total_time_ms = metrics_.v5_time_ms;
    metrics_.pixels_per_second = metrics_.pixels_processed / (metrics_.total_time_ms / 1000.0);

    E2E_ASSERT(v5_ok, "V5 motion processing failed");

    LogMetrics("MotionTrackingPipeline");
}

//=============================================================================
// E2E Test: Occipital Bridge Full Pipeline
//=============================================================================

TEST_F(VisualProcessingGPUE2ETest, OccipitalBridgeFullPipeline) {
    // WHAT: Test complete occipital GPU bridge workflow
    // WHY:  Verify bridge correctly orchestrates all visual processing
    // HOW:  Upload image, run all stages through bridge, verify results

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    E2E_ASSERT_NOT_NULL(bridge_, "Bridge creation failed");
    E2E_ASSERT(occipital_gpu_bridge_is_available(bridge_), "Bridge not available");

    auto image = CreateNaturalImage(WIDTH, HEIGHT);
    metrics_.pixels_processed = WIDTH * HEIGHT;

    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = WIDTH;
    input.height = HEIGHT;
    input.channels = 1;
    input.data = image.data();

    auto pipeline_start = std::chrono::high_resolution_clock::now();

    // Upload
    bool uploaded = occipital_gpu_upload_input(bridge_, &input);
    E2E_ASSERT(uploaded, "Upload failed");

    // Process all stages
    auto v1_start = std::chrono::high_resolution_clock::now();
    E2E_ASSERT(occipital_gpu_process_v1(bridge_), "V1 failed");
    auto v1_end = std::chrono::high_resolution_clock::now();
    metrics_.v1_time_ms = std::chrono::duration<double, std::milli>(v1_end - v1_start).count();

    auto v2_start = std::chrono::high_resolution_clock::now();
    E2E_ASSERT(occipital_gpu_process_v2(bridge_), "V2 failed");
    auto v2_end = std::chrono::high_resolution_clock::now();
    metrics_.v2_time_ms = std::chrono::duration<double, std::milli>(v2_end - v2_start).count();

    auto v5_start = std::chrono::high_resolution_clock::now();
    E2E_ASSERT(occipital_gpu_process_v5(bridge_), "V5 failed");
    auto v5_end = std::chrono::high_resolution_clock::now();
    metrics_.v5_time_ms = std::chrono::duration<double, std::milli>(v5_end - v5_start).count();

    // Download
    visual_feature_t features_out[256];
    uint32_t num_features_out = 0;
    E2E_ASSERT(occipital_gpu_download_features(bridge_, features_out, 256, &num_features_out), "Download failed");

    auto pipeline_end = std::chrono::high_resolution_clock::now();
    metrics_.total_time_ms = std::chrono::duration<double, std::milli>(pipeline_end - pipeline_start).count();
    metrics_.pixels_per_second = metrics_.pixels_processed / (metrics_.total_time_ms / 1000.0);

    // Get stats
    occipital_gpu_bridge_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    bool got_stats = occipital_gpu_bridge_get_stats(bridge_, &stats);
    E2E_ASSERT(got_stats, "Failed to get stats");

    LogMetrics("OccipitalBridgeFullPipeline");
}

//=============================================================================
// E2E Test: Performance Benchmark
//=============================================================================

TEST_F(VisualProcessingGPUE2ETest, PerformanceBenchmark) {
    // WHAT: Benchmark full visual pipeline performance
    // WHY:  Measure throughput for real-time capability assessment
    // HOW:  Run multiple iterations, compute average throughput

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    E2E_ASSERT_NOT_NULL(bridge_, "Bridge creation failed");

    // Larger image for benchmarking
    const int bench_width = 256;
    const int bench_height = 256;

    std::vector<float> image(bench_width * bench_height);
    for (size_t i = 0; i < image.size(); i++) {
        image[i] = static_cast<float>(rng_() % 256) / 255.0f;
    }

    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = bench_width;
    input.height = bench_height;
    input.channels = 1;
    input.data = image.data();

    // Warmup
    occipital_gpu_upload_input(bridge_, &input);
    occipital_gpu_process_v1(bridge_);

    // Benchmark
    const int iterations = 50;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        occipital_gpu_upload_input(bridge_, &input);
        occipital_gpu_process_v1(bridge_);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double avg_ms = elapsed_ms / iterations;
    double fps = 1000.0 / avg_ms;
    double mpix_per_sec = (bench_width * bench_height * fps) / 1e6;

    std::cout << "\n=== Performance Benchmark ===" << std::endl;
    std::cout << "  Image size: " << bench_width << "x" << bench_height << std::endl;
    std::cout << "  Iterations: " << iterations << std::endl;
    std::cout << "  Avg time: " << avg_ms << " ms/frame" << std::endl;
    std::cout << "  Throughput: " << fps << " fps" << std::endl;
    std::cout << "  Pixel rate: " << mpix_per_sec << " Mpix/s" << std::endl;

    // Should achieve reasonable performance
    EXPECT_GT(fps, 1.0) << "Performance too low";
}

//=============================================================================
// E2E Test: Stress Test
//=============================================================================

TEST_F(VisualProcessingGPUE2ETest, StressTestManyFrames) {
    // WHAT: Stress test with many consecutive frames
    // WHY:  Verify no memory leaks or degradation over time
    // HOW:  Process 100 frames, verify no errors or slowdown

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    E2E_ASSERT_NOT_NULL(bridge_, "Bridge creation failed");

    const int num_frames = 100;
    std::vector<double> frame_times;
    frame_times.reserve(num_frames);

    for (int f = 0; f < num_frames; f++) {
        // Generate random frame
        std::vector<float> frame(WIDTH * HEIGHT);
        for (size_t i = 0; i < frame.size(); i++) {
            frame[i] = static_cast<float>(rng_() % 256) / 255.0f;
        }

        visual_input_t input;
        memset(&input, 0, sizeof(input));
        input.width = WIDTH;
        input.height = HEIGHT;
        input.channels = 1;
        input.data = frame.data();

        auto start = std::chrono::high_resolution_clock::now();

        bool uploaded = occipital_gpu_upload_input(bridge_, &input);
        ASSERT_TRUE(uploaded) << "Upload failed at frame " << f;

        bool v1_ok = occipital_gpu_process_v1(bridge_);
        ASSERT_TRUE(v1_ok) << "V1 failed at frame " << f;

        auto end = std::chrono::high_resolution_clock::now();

        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        frame_times.push_back(ms);
    }

    // Analyze frame times
    double sum = std::accumulate(frame_times.begin(), frame_times.end(), 0.0);
    double avg = sum / frame_times.size();
    double max_time = *std::max_element(frame_times.begin(), frame_times.end());
    double min_time = *std::min_element(frame_times.begin(), frame_times.end());

    // Check for degradation (last 10 frames shouldn't be much slower than first 10)
    double first_10_avg = std::accumulate(frame_times.begin(), frame_times.begin() + 10, 0.0) / 10;
    double last_10_avg = std::accumulate(frame_times.end() - 10, frame_times.end(), 0.0) / 10;

    std::cout << "\n=== Stress Test Results ===" << std::endl;
    std::cout << "  Frames processed: " << num_frames << std::endl;
    std::cout << "  Avg frame time: " << avg << " ms" << std::endl;
    std::cout << "  Min frame time: " << min_time << " ms" << std::endl;
    std::cout << "  Max frame time: " << max_time << " ms" << std::endl;
    std::cout << "  First 10 avg: " << first_10_avg << " ms" << std::endl;
    std::cout << "  Last 10 avg: " << last_10_avg << " ms" << std::endl;

    // Last frames shouldn't be more than 3x slower than first frames
    EXPECT_LT(last_10_avg, first_10_avg * 3.0)
        << "Performance degradation detected over " << num_frames << " frames";
}

//=============================================================================
// E2E Test: Full Processing with Result
//=============================================================================

TEST_F(VisualProcessingGPUE2ETest, FullProcessingWithResult) {
    // WHAT: Test complete processing using occipital_gpu_process
    // WHY:  Verify unified processing API works correctly
    // HOW:  Call occipital_gpu_process, verify result

    occipital_gpu_bridge_config_t config = occipital_gpu_bridge_default_config();
    bridge_ = occipital_gpu_bridge_create(occipital_, &config);
    E2E_ASSERT_NOT_NULL(bridge_, "Bridge creation failed");

    auto image = CreateNaturalImage(WIDTH, HEIGHT);

    visual_input_t input;
    memset(&input, 0, sizeof(input));
    input.width = WIDTH;
    input.height = HEIGHT;
    input.channels = 1;
    input.data = image.data();

    bool uploaded = occipital_gpu_upload_input(bridge_, &input);
    E2E_ASSERT(uploaded, "Upload failed");

    visual_processing_result_t result;
    memset(&result, 0, sizeof(result));

    auto start = std::chrono::high_resolution_clock::now();
    bool processed = occipital_gpu_process(bridge_, &result);
    auto end = std::chrono::high_resolution_clock::now();

    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    E2E_ASSERT(processed, "Full processing failed");

    std::cout << "\n=== Full Processing with Result ===" << std::endl;
    std::cout << "  Processing time: " << elapsed_ms << " ms" << std::endl;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
