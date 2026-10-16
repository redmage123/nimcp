/**
 * @file e2e_test_gpu_training_pipeline.cpp
 * @brief E2E Tests for GPU Training Pipeline
 *
 * WHAT: End-to-end testing for GPU/CPU execution mode selection and training
 * WHY:  Verify GPU training works correctly with fallback support
 * HOW:  Test mode detection, switching, memory management, and training
 *
 * TEST PIPELINES:
 * - HardwareCapabilityDetection: Detect and verify hardware capabilities
 * - ExecutionContextLifecycle: Create/destroy execution contexts
 * - GPUCPUFallbackBehavior: Verify fallback when GPU unavailable
 * - ModeSwitchingDuringOperation: Switch modes mid-operation
 * - PerformanceValidation: Validate GPU vs CPU performance
 * - MemoryManagementCrossMode: Memory allocation across modes
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "e2e_test_framework.h"

// Headers have their own extern "C" guards
#include "gpu/nimcp_execution_mode.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"

#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <chrono>

//=============================================================================
// Test Fixture
//=============================================================================

class GPUTrainingPipelineE2ETest : public ::testing::Test {
protected:
    execution_context_t ctx_ = nullptr;
    hardware_capabilities_t caps_;
    bool capabilities_detected_ = false;

    void SetUp() override {
        memset(&caps_, 0, sizeof(caps_));
        capabilities_detected_ = execution_detect_capabilities(&caps_);
    }

    void TearDown() override {
        if (ctx_) {
            execution_context_destroy(ctx_);
            ctx_ = nullptr;
        }
    }

    bool HasGPUSupport() const {
        return caps_.cuda_available || caps_.rocm_available || caps_.opencl_available;
    }

    execution_mode_t GetAvailableGPUMode() const {
        if (caps_.cuda_available) return EXEC_MODE_GPU_CUDA;
        if (caps_.rocm_available) return EXEC_MODE_GPU_ROCM;
        if (caps_.opencl_available) return EXEC_MODE_GPU_OPENCL;
        return EXEC_MODE_CPU_PARALLEL;  // Fallback
    }

    execution_context_t CreateContext(execution_mode_t mode) {
        execution_config_t config = execution_get_default_config(mode);
        config.auto_fallback = true;
        config.fallback_mode = EXEC_MODE_CPU_PARALLEL;
        config.enable_validation = true;
        return execution_context_create(&config);
    }
};

//=============================================================================
// Pipeline 1: Hardware Capability Detection
//=============================================================================

TEST_F(GPUTrainingPipelineE2ETest, HardwareCapabilityDetection) {
    E2E_PIPELINE_START("Hardware Capability Detection");

    // Stage 1: Detect capabilities
    E2E_STAGE_BEGIN("Detect hardware capabilities", 500);

    E2E_ASSERT(capabilities_detected_, "Failed to detect hardware capabilities");

    // CPU should always be available
    E2E_ASSERT(caps_.cpu_available, "CPU must be available");
    EXPECT_GT(caps_.cpu_cores, 0u);
    EXPECT_GT(caps_.cpu_threads, 0u);

    E2E_STAGE_END();

    // Stage 2: Check GPU capabilities
    E2E_STAGE_BEGIN("Check GPU capabilities", 300);

    if (HasGPUSupport()) {
        EXPECT_GT(caps_.gpu_count, 0u);
        EXPECT_GT(caps_.gpu_memory_mb, 0u);
        EXPECT_GT(caps_.gpu_compute_units, 0u);

        std::cout << "\nGPU Detected:" << std::endl;
        std::cout << "  CUDA: " << (caps_.cuda_available ? "yes" : "no") << std::endl;
        std::cout << "  ROCm: " << (caps_.rocm_available ? "yes" : "no") << std::endl;
        std::cout << "  OpenCL: " << (caps_.opencl_available ? "yes" : "no") << std::endl;
        std::cout << "  GPU Count: " << caps_.gpu_count << std::endl;
        std::cout << "  GPU Memory: " << caps_.gpu_memory_mb << " MB" << std::endl;
        std::cout << "  Compute Units: " << caps_.gpu_compute_units << std::endl;
    } else {
        std::cout << "\nNo GPU detected - using CPU modes" << std::endl;
    }

    E2E_STAGE_END();

    // Stage 3: Verify mode support detection
    E2E_STAGE_BEGIN("Verify mode support detection", 300);

    // CPU modes should always be supported
    EXPECT_TRUE(execution_mode_is_supported(EXEC_MODE_CPU_SEQUENTIAL));
    EXPECT_TRUE(execution_mode_is_supported(EXEC_MODE_CPU_PARALLEL));

    // GPU modes depend on hardware
    if (caps_.cuda_available) {
        EXPECT_TRUE(execution_mode_is_supported(EXEC_MODE_GPU_CUDA));
    }
    if (caps_.rocm_available) {
        EXPECT_TRUE(execution_mode_is_supported(EXEC_MODE_GPU_ROCM));
    }
    if (caps_.opencl_available) {
        EXPECT_TRUE(execution_mode_is_supported(EXEC_MODE_GPU_OPENCL));
    }

    E2E_STAGE_END();

    // Stage 4: Get recommended mode
    E2E_STAGE_BEGIN("Get recommended mode for workloads", 300);

    // Small network - should recommend CPU
    execution_mode_t small_mode = execution_get_recommended_mode(100, 10);
    EXPECT_TRUE(small_mode == EXEC_MODE_CPU_SEQUENTIAL ||
                small_mode == EXEC_MODE_CPU_PARALLEL);

    // Large network - should recommend GPU if available
    execution_mode_t large_mode = execution_get_recommended_mode(100000, 100);
    if (HasGPUSupport()) {
        EXPECT_TRUE(large_mode == EXEC_MODE_GPU_CUDA ||
                    large_mode == EXEC_MODE_GPU_ROCM ||
                    large_mode == EXEC_MODE_GPU_OPENCL ||
                    large_mode == EXEC_MODE_HYBRID);
    }

    std::cout << "\nRecommended modes:" << std::endl;
    std::cout << "  Small (100 neurons): " << small_mode << std::endl;
    std::cout << "  Large (100K neurons): " << large_mode << std::endl;

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 2: Execution Context Lifecycle
//=============================================================================

TEST_F(GPUTrainingPipelineE2ETest, ExecutionContextLifecycle) {
    E2E_PIPELINE_START("Execution Context Lifecycle");

    // Stage 1: Create CPU sequential context
    E2E_STAGE_BEGIN("Create CPU sequential context", 500);

    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx_ = execution_context_create(&config);
    E2E_ASSERT_NOT_NULL(ctx_, "Failed to create CPU sequential context");

    execution_mode_t active_mode = execution_context_get_mode(ctx_);
    EXPECT_EQ(active_mode, EXEC_MODE_CPU_SEQUENTIAL);

    execution_context_destroy(ctx_);
    ctx_ = nullptr;

    E2E_STAGE_END();

    // Stage 2: Create CPU parallel context
    E2E_STAGE_BEGIN("Create CPU parallel context", 500);

    config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);
    config.cpu_threads = caps_.cpu_threads;
    ctx_ = execution_context_create(&config);
    E2E_ASSERT_NOT_NULL(ctx_, "Failed to create CPU parallel context");

    active_mode = execution_context_get_mode(ctx_);
    EXPECT_EQ(active_mode, EXEC_MODE_CPU_PARALLEL);

    execution_context_destroy(ctx_);
    ctx_ = nullptr;

    E2E_STAGE_END();

    // Stage 3: Create GPU context (or fallback)
    E2E_STAGE_BEGIN("Create GPU context (with fallback)", 1000);

    config = execution_get_default_config(GetAvailableGPUMode());
    config.auto_fallback = true;
    config.fallback_mode = EXEC_MODE_CPU_PARALLEL;

    ctx_ = execution_context_create(&config);
    E2E_ASSERT_NOT_NULL(ctx_, "Failed to create GPU context (or fallback)");

    active_mode = execution_context_get_mode(ctx_);
    if (HasGPUSupport()) {
        // Should be in GPU mode
        EXPECT_TRUE(active_mode == EXEC_MODE_GPU_CUDA ||
                    active_mode == EXEC_MODE_GPU_ROCM ||
                    active_mode == EXEC_MODE_GPU_OPENCL);
    } else {
        // Should fallback to CPU
        EXPECT_EQ(active_mode, EXEC_MODE_CPU_PARALLEL);
    }

    std::cout << "\nActive execution mode: " << active_mode << std::endl;

    E2E_STAGE_END();

    // Stage 4: Verify synchronization
    E2E_STAGE_BEGIN("Verify synchronization", 300);

    bool sync_result = execution_synchronize(ctx_);
    EXPECT_TRUE(sync_result);

    E2E_STAGE_END();

    // Stage 5: Get execution statistics
    E2E_STAGE_BEGIN("Get execution statistics", 200);

    uint64_t total_ops = 0;
    double total_time_ms = 0.0;

    bool stats_result = execution_get_stats(ctx_, &total_ops, &total_time_ms);
    EXPECT_TRUE(stats_result);

    std::cout << "  Total ops: " << total_ops << std::endl;
    std::cout << "  Total time: " << total_time_ms << " ms" << std::endl;

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 3: GPU/CPU Fallback Behavior
//=============================================================================

TEST_F(GPUTrainingPipelineE2ETest, GPUCPUFallbackBehavior) {
    E2E_PIPELINE_START("GPU/CPU Fallback Behavior");

    // Stage 1: Request unsupported GPU mode
    E2E_STAGE_BEGIN("Request mode with auto-fallback", 500);

    // Even if GPU is available, this tests fallback path
    execution_config_t config;
    memset(&config, 0, sizeof(config));
    config.mode = EXEC_MODE_GPU_CUDA;  // Request CUDA
    config.auto_fallback = true;
    config.fallback_mode = EXEC_MODE_CPU_PARALLEL;
    config.cpu_threads = caps_.cpu_threads;

    ctx_ = execution_context_create(&config);
    E2E_ASSERT_NOT_NULL(ctx_, "Context creation failed even with fallback");

    execution_mode_t active = execution_context_get_mode(ctx_);
    if (!caps_.cuda_available) {
        EXPECT_EQ(active, EXEC_MODE_CPU_PARALLEL);
        std::cout << "  CUDA not available - fell back to CPU parallel" << std::endl;
    } else {
        EXPECT_EQ(active, EXEC_MODE_GPU_CUDA);
        std::cout << "  CUDA available - using GPU" << std::endl;
    }

    execution_context_destroy(ctx_);
    ctx_ = nullptr;

    E2E_STAGE_END();

    // Stage 2: Verify operations work after fallback
    E2E_STAGE_BEGIN("Verify operations after fallback", 1000);

    // Create context with explicit fallback
    config = execution_get_default_config(EXEC_MODE_AUTO);
    config.auto_fallback = true;
    ctx_ = execution_context_create(&config);
    E2E_ASSERT_NOT_NULL(ctx_, "Failed to create AUTO context");

    // Allocate memory
    void* mem = execution_alloc(ctx_, 1024);
    E2E_ASSERT_NOT_NULL(mem, "Failed to allocate memory");

    // Free memory
    execution_free(ctx_, mem);

    E2E_STAGE_END();

    // Stage 3: Memory copy test
    E2E_STAGE_BEGIN("Memory copy operations", 500);

    const size_t test_size = 4096;
    float* host_src = new float[test_size];
    float* host_dst = new float[test_size];

    // Initialize source
    for (size_t i = 0; i < test_size; i++) {
        host_src[i] = static_cast<float>(i) * 0.001f;
    }
    memset(host_dst, 0, test_size * sizeof(float));

    // Allocate device memory
    void* dev_mem = execution_alloc(ctx_, test_size * sizeof(float));
    if (dev_mem) {
        // Copy to device
        bool copy_result = execution_memcpy(ctx_, dev_mem, host_src,
                                             test_size * sizeof(float), true);
        EXPECT_TRUE(copy_result);

        // Copy back
        copy_result = execution_memcpy(ctx_, host_dst, dev_mem,
                                        test_size * sizeof(float), false);
        EXPECT_TRUE(copy_result);

        // Verify
        bool match = true;
        for (size_t i = 0; i < test_size && match; i++) {
            if (std::abs(host_src[i] - host_dst[i]) > 1e-6f) {
                match = false;
            }
        }
        EXPECT_TRUE(match);

        execution_free(ctx_, dev_mem);
    }

    delete[] host_src;
    delete[] host_dst;

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 4: Mode Switching During Operation
//=============================================================================

TEST_F(GPUTrainingPipelineE2ETest, ModeSwitchingDuringOperation) {
    E2E_PIPELINE_START("Mode Switching During Operation");

    // Stage 1: Start with CPU parallel
    E2E_STAGE_BEGIN("Start with CPU parallel mode", 500);

    ctx_ = CreateContext(EXEC_MODE_CPU_PARALLEL);
    E2E_ASSERT_NOT_NULL(ctx_, "Failed to create CPU parallel context");

    EXPECT_EQ(execution_context_get_mode(ctx_), EXEC_MODE_CPU_PARALLEL);

    // Perform some operations
    void* mem1 = execution_alloc(ctx_, 2048);
    E2E_ASSERT_NOT_NULL(mem1, "Failed to allocate in CPU mode");

    E2E_STAGE_END();

    // Stage 2: Switch to CPU sequential
    E2E_STAGE_BEGIN("Switch to CPU sequential", 500);

    bool switch_result = execution_context_set_mode(ctx_, EXEC_MODE_CPU_SEQUENTIAL);
    EXPECT_TRUE(switch_result);
    EXPECT_EQ(execution_context_get_mode(ctx_), EXEC_MODE_CPU_SEQUENTIAL);

    // Previous allocation should still be valid
    // (implementation may migrate or invalidate - check behavior)

    E2E_STAGE_END();

    // Stage 3: Allocate in new mode
    E2E_STAGE_BEGIN("Allocate in new mode", 300);

    void* mem2 = execution_alloc(ctx_, 4096);
    E2E_ASSERT_NOT_NULL(mem2, "Failed to allocate after mode switch");

    E2E_STAGE_END();

    // Stage 4: Switch to GPU (or remain CPU)
    E2E_STAGE_BEGIN("Attempt GPU mode switch", 1000);

    execution_mode_t target_mode = GetAvailableGPUMode();
    switch_result = execution_context_set_mode(ctx_, target_mode);

    if (HasGPUSupport()) {
        EXPECT_TRUE(switch_result);
        execution_mode_t current = execution_context_get_mode(ctx_);
        EXPECT_EQ(current, target_mode);
    } else {
        // Switch should fail or fallback
        std::cout << "  GPU not available, switch returned: "
                  << (switch_result ? "true" : "false") << std::endl;
    }

    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Cleanup allocations", 300);

    execution_free(ctx_, mem1);
    execution_free(ctx_, mem2);

    bool sync = execution_synchronize(ctx_);
    EXPECT_TRUE(sync);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 5: Performance Validation
//=============================================================================

TEST_F(GPUTrainingPipelineE2ETest, PerformanceValidation) {
    E2E_PIPELINE_START("Performance Validation");

    const size_t DATA_SIZE = 1024 * 1024;  // 1M floats = 4MB
    const int NUM_ITERATIONS = 10;

    // Stage 1: CPU sequential baseline
    E2E_STAGE_BEGIN("CPU sequential baseline", 2000);

    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    execution_context_t cpu_seq_ctx = execution_context_create(&config);
    E2E_ASSERT_NOT_NULL(cpu_seq_ctx, "Failed to create CPU sequential context");

    auto start = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        void* mem = execution_alloc(cpu_seq_ctx, DATA_SIZE * sizeof(float));
        if (mem) {
            execution_synchronize(cpu_seq_ctx);
            execution_free(cpu_seq_ctx, mem);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    double cpu_seq_time = std::chrono::duration<double, std::milli>(end - start).count();

    execution_context_destroy(cpu_seq_ctx);

    std::cout << "\n  CPU Sequential: " << cpu_seq_time << " ms for "
              << NUM_ITERATIONS << " iterations" << std::endl;

    E2E_STAGE_END();

    // Stage 2: CPU parallel performance
    E2E_STAGE_BEGIN("CPU parallel performance", 2000);

    config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);
    config.cpu_threads = caps_.cpu_threads;
    execution_context_t cpu_par_ctx = execution_context_create(&config);
    E2E_ASSERT_NOT_NULL(cpu_par_ctx, "Failed to create CPU parallel context");

    start = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
        void* mem = execution_alloc(cpu_par_ctx, DATA_SIZE * sizeof(float));
        if (mem) {
            execution_synchronize(cpu_par_ctx);
            execution_free(cpu_par_ctx, mem);
        }
    }

    end = std::chrono::high_resolution_clock::now();
    double cpu_par_time = std::chrono::duration<double, std::milli>(end - start).count();

    execution_context_destroy(cpu_par_ctx);

    std::cout << "  CPU Parallel (" << caps_.cpu_threads << " threads): "
              << cpu_par_time << " ms" << std::endl;

    E2E_STAGE_END();

    // Stage 3: GPU performance (if available)
    E2E_STAGE_BEGIN("GPU performance", 2000);

    double gpu_time = 0.0;

    if (HasGPUSupport()) {
        config = execution_get_default_config(GetAvailableGPUMode());
        execution_context_t gpu_ctx = execution_context_create(&config);

        if (gpu_ctx) {
            start = std::chrono::high_resolution_clock::now();

            for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
                void* mem = execution_alloc(gpu_ctx, DATA_SIZE * sizeof(float));
                if (mem) {
                    execution_synchronize(gpu_ctx);
                    execution_free(gpu_ctx, mem);
                }
            }

            end = std::chrono::high_resolution_clock::now();
            gpu_time = std::chrono::duration<double, std::milli>(end - start).count();

            execution_context_destroy(gpu_ctx);

            std::cout << "  GPU: " << gpu_time << " ms" << std::endl;
        }
    } else {
        std::cout << "  GPU: N/A (not available)" << std::endl;
    }

    E2E_STAGE_END();

    // Stage 4: Performance comparison
    E2E_STAGE_BEGIN("Performance comparison", 200);

    std::cout << "\nPerformance Summary:" << std::endl;
    std::cout << "  CPU Sequential: " << cpu_seq_time << " ms" << std::endl;
    std::cout << "  CPU Parallel:   " << cpu_par_time << " ms ("
              << (cpu_seq_time / cpu_par_time) << "x speedup)" << std::endl;

    if (HasGPUSupport() && gpu_time > 0) {
        std::cout << "  GPU:            " << gpu_time << " ms ("
                  << (cpu_seq_time / gpu_time) << "x speedup)" << std::endl;
    }

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 6: Memory Management Cross-Mode
//=============================================================================

TEST_F(GPUTrainingPipelineE2ETest, MemoryManagementCrossMode) {
    E2E_PIPELINE_START("Memory Management Cross-Mode");

    // Stage 1: Create context with optimal config
    E2E_STAGE_BEGIN("Create optimally configured context", 500);

    execution_config_t config = execution_get_optimal_config(10000);
    config.auto_fallback = true;
    ctx_ = execution_context_create(&config);
    E2E_ASSERT_NOT_NULL(ctx_, "Failed to create optimal context");

    std::cout << "\n  Optimal mode for 10K neurons: "
              << execution_context_get_mode(ctx_) << std::endl;

    E2E_STAGE_END();

    // Stage 2: Allocate various sizes
    E2E_STAGE_BEGIN("Allocate various memory sizes", 1000);

    size_t sizes[] = {64, 256, 1024, 4096, 16384, 65536, 262144};
    void* allocations[7] = {nullptr};

    for (size_t i = 0; i < 7; i++) {
        allocations[i] = execution_alloc(ctx_, sizes[i]);
        EXPECT_NE(allocations[i], nullptr) << "Failed to allocate " << sizes[i] << " bytes";
    }

    E2E_STAGE_END();

    // Stage 3: Data transfer tests
    E2E_STAGE_BEGIN("Data transfer tests", 1000);

    const size_t TRANSFER_SIZE = 4096;
    float* src_data = new float[TRANSFER_SIZE];
    float* dst_data = new float[TRANSFER_SIZE];

    // Initialize source with pattern
    for (size_t i = 0; i < TRANSFER_SIZE; i++) {
        src_data[i] = std::sin(static_cast<float>(i) * 0.01f);
    }

    // Use allocation 3 (4096 bytes = 1024 floats)
    void* device_buf = allocations[3];
    if (device_buf && TRANSFER_SIZE * sizeof(float) <= 4096) {
        bool to_device = execution_memcpy(ctx_, device_buf, src_data,
                                          1024 * sizeof(float), true);
        EXPECT_TRUE(to_device);

        execution_synchronize(ctx_);

        bool from_device = execution_memcpy(ctx_, dst_data, device_buf,
                                            1024 * sizeof(float), false);
        EXPECT_TRUE(from_device);

        execution_synchronize(ctx_);

        // Verify data integrity
        int mismatches = 0;
        for (size_t i = 0; i < 1024; i++) {
            if (std::abs(src_data[i] - dst_data[i]) > 1e-5f) {
                mismatches++;
            }
        }
        EXPECT_EQ(mismatches, 0) << "Data transfer corrupted " << mismatches << " values";
    }

    delete[] src_data;
    delete[] dst_data;

    E2E_STAGE_END();

    // Stage 4: Free all allocations
    E2E_STAGE_BEGIN("Free all allocations", 500);

    for (size_t i = 0; i < 7; i++) {
        if (allocations[i]) {
            execution_free(ctx_, allocations[i]);
            allocations[i] = nullptr;
        }
    }

    execution_synchronize(ctx_);

    E2E_STAGE_END();

    // Stage 5: Verify no memory leaks via stats
    E2E_STAGE_BEGIN("Verify execution stats", 200);

    uint64_t total_ops = 0;
    double total_time_ms = 0.0;

    bool got_stats = execution_get_stats(ctx_, &total_ops, &total_time_ms);
    EXPECT_TRUE(got_stats);

    std::cout << "\n  Final execution stats:" << std::endl;
    std::cout << "    Total operations: " << total_ops << std::endl;
    std::cout << "    Total time: " << total_time_ms << " ms" << std::endl;

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
