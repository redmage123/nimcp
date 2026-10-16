/**
 * @file test_gpu_cpu_fallback_integration.cpp
 * @brief Integration tests for GPU detection and automatic CPU fallback
 *
 * WHAT: Tests GPU detection, execution mode selection, and automatic fallback
 * WHY:  Verify the system works correctly on various hardware configurations
 * HOW:  Test detection APIs, mode switching, and graceful degradation
 *
 * Test Categories:
 * - GPU capability detection
 * - Automatic CPU fallback when GPU unavailable
 * - Execution mode switching
 * - Hardware capability queries
 * - Memory management across modes
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "gpu/nimcp_execution_mode.h"
#include "gpu/execution/nimcp_gpu_detect.h"
#include "gpu/nimcp_gpu_neuron.h"
#include "gpu/nimcp_spike_event.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture - GPU Detection
//=============================================================================

class GPUDetectionTest : public ::testing::Test {
protected:
    gpu_detect_result_t caps;

    void SetUp() override {
        memset(&caps, 0, sizeof(caps));
    }

    void TearDown() override {
        // Nothing to clean up
    }
};

//=============================================================================
// Test Fixture - Execution Mode
//=============================================================================

class ExecutionModeTest : public ::testing::Test {
protected:
    execution_context_t ctx = nullptr;
    hardware_capabilities_t hw_caps;

    void SetUp() override {
        memset(&hw_caps, 0, sizeof(hw_caps));
        execution_detect_capabilities(&hw_caps);
    }

    void TearDown() override {
        if (ctx) {
            execution_context_destroy(ctx);
            ctx = nullptr;
        }
    }
};

//=============================================================================
// Test Fixture - Execution Context with Auto-Fallback
//=============================================================================

class ExecutionFallbackTest : public ::testing::Test {
protected:
    execution_context_t ctx = nullptr;

    void SetUp() override {
        // Nothing special
    }

    void TearDown() override {
        if (ctx) {
            execution_context_destroy(ctx);
            ctx = nullptr;
        }
    }
};

//=============================================================================
// GPU Detection Tests
//=============================================================================

TEST_F(GPUDetectionTest, DetectCapabilities) {
    bool result = gpu_detect_capabilities(&caps);
    EXPECT_TRUE(result);  // Should succeed even if no GPU

    // At minimum, we should get device count (possibly 0)
    EXPECT_GE(caps.device_count, 0u);
}

TEST_F(GPUDetectionTest, DetectCapabilitiesNullPointer) {
    bool result = gpu_detect_capabilities(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(GPUDetectionTest, CheckGpuIsAvailable) {
    // This just checks the function works - result depends on hardware
    bool gpu_avail = gpu_is_available();

    // Verify consistency with detailed detection
    gpu_detect_capabilities(&caps);
    if (caps.device_count > 0) {
        EXPECT_TRUE(gpu_avail);
    } else {
        EXPECT_FALSE(gpu_avail);
    }
}

TEST_F(GPUDetectionTest, BackendAvailability) {
    gpu_detect_capabilities(&caps);

    // Check individual backends
    bool cuda = gpu_backend_is_available(GPU_BACKEND_CUDA);
    bool opencl = gpu_backend_is_available(GPU_BACKEND_OPENCL);
    bool rocm = gpu_backend_is_available(GPU_BACKEND_ROCM);

    // Verify consistency with caps structure
    EXPECT_EQ(cuda, caps.cuda_available);
    EXPECT_EQ(opencl, caps.opencl_available);
    EXPECT_EQ(rocm, caps.rocm_available);
}

TEST_F(GPUDetectionTest, GetDeviceCount) {
    gpu_detect_capabilities(&caps);
    uint32_t count = gpu_get_device_count();
    EXPECT_EQ(count, caps.device_count);
}

TEST_F(GPUDetectionTest, GetDeviceInfo) {
    gpu_detect_capabilities(&caps);

    if (caps.device_count > 0) {
        gpu_device_info_t info;
        memset(&info, 0, sizeof(info));

        bool result = gpu_get_device_info(0, &info);
        EXPECT_TRUE(result);

        // Device should have a name
        EXPECT_GT(strlen(info.name), 0u);

        // Device should have memory
        EXPECT_GT(info.total_memory_bytes, 0u);
    }
}

TEST_F(GPUDetectionTest, GetDeviceInfoInvalidIndex) {
    gpu_device_info_t info;
    memset(&info, 0, sizeof(info));

    bool result = gpu_get_device_info(9999, &info);
    EXPECT_FALSE(result);
}

TEST_F(GPUDetectionTest, GetBestDevice) {
    gpu_detect_capabilities(&caps);

    int best_idx = gpu_get_best_device();

    if (caps.device_count > 0) {
        EXPECT_GE(best_idx, 0);
        EXPECT_LT(best_idx, static_cast<int>(caps.device_count));
    } else {
        EXPECT_EQ(best_idx, -1);
    }
}

TEST_F(GPUDetectionTest, GetRecommendedBackend) {
    gpu_detect_capabilities(&caps);

    gpu_backend_t backend = gpu_get_recommended_backend();

    if (caps.device_count > 0) {
        EXPECT_NE(backend, GPU_BACKEND_NONE);
    } else {
        EXPECT_EQ(backend, GPU_BACKEND_NONE);
    }
}

TEST_F(GPUDetectionTest, BackendName) {
    const char* cuda_name = gpu_backend_name(GPU_BACKEND_CUDA);
    EXPECT_STREQ(cuda_name, "CUDA");

    const char* opencl_name = gpu_backend_name(GPU_BACKEND_OPENCL);
    EXPECT_STREQ(opencl_name, "OpenCL");

    const char* rocm_name = gpu_backend_name(GPU_BACKEND_ROCM);
    EXPECT_STREQ(rocm_name, "ROCm");

    const char* none_name = gpu_backend_name(GPU_BACKEND_NONE);
    EXPECT_STREQ(none_name, "None");
}

TEST_F(GPUDetectionTest, VendorName) {
    const char* nvidia_name = gpu_vendor_name(GPU_VENDOR_NVIDIA);
    EXPECT_STREQ(nvidia_name, "NVIDIA");

    const char* amd_name = gpu_vendor_name(GPU_VENDOR_AMD);
    EXPECT_STREQ(amd_name, "AMD");

    const char* intel_name = gpu_vendor_name(GPU_VENDOR_INTEL);
    EXPECT_STREQ(intel_name, "Intel");
}

TEST_F(GPUDetectionTest, CapabilitiesString) {
    gpu_detect_capabilities(&caps);

    char buffer[512];
    size_t len = gpu_capabilities_string(buffer, sizeof(buffer));

    EXPECT_GT(len, 0u);
    EXPECT_LT(len, sizeof(buffer));
}

TEST_F(GPUDetectionTest, RefreshCapabilities) {
    // Initial detection
    gpu_detect_capabilities(&caps);
    uint32_t initial_count = caps.device_count;

    // Refresh (should return same results on stable system)
    bool result = gpu_refresh_capabilities();
    EXPECT_TRUE(result);

    // Re-detect and verify
    gpu_detect_capabilities(&caps);
    EXPECT_EQ(caps.device_count, initial_count);
}

//=============================================================================
// Execution Mode Detection Tests
//=============================================================================

TEST_F(ExecutionModeTest, DetectHardwareCapabilities) {
    bool result = execution_detect_capabilities(&hw_caps);
    EXPECT_TRUE(result);

    // CPU should always be available
    EXPECT_TRUE(hw_caps.cpu_available);
    EXPECT_GT(hw_caps.cpu_cores, 0u);
}

TEST_F(ExecutionModeTest, CPUModeAlwaysSupported) {
    EXPECT_TRUE(execution_mode_is_supported(EXEC_MODE_CPU_SEQUENTIAL));
    EXPECT_TRUE(execution_mode_is_supported(EXEC_MODE_CPU_PARALLEL));
}

TEST_F(ExecutionModeTest, GPUModeSupport) {
    bool cuda_supported = execution_mode_is_supported(EXEC_MODE_GPU_CUDA);
    bool rocm_supported = execution_mode_is_supported(EXEC_MODE_GPU_ROCM);
    bool opencl_supported = execution_mode_is_supported(EXEC_MODE_GPU_OPENCL);

    // Verify consistency with hardware detection
    EXPECT_EQ(cuda_supported, hw_caps.cuda_available);
    EXPECT_EQ(rocm_supported, hw_caps.rocm_available);
    EXPECT_EQ(opencl_supported, hw_caps.opencl_available);
}

TEST_F(ExecutionModeTest, GetRecommendedMode) {
    // Small network - should recommend CPU
    execution_mode_t mode_small = execution_get_recommended_mode(100, 10);
    EXPECT_TRUE(
        mode_small == EXEC_MODE_CPU_SEQUENTIAL ||
        mode_small == EXEC_MODE_CPU_PARALLEL
    );

    // Large network - might recommend GPU if available
    execution_mode_t mode_large = execution_get_recommended_mode(100000, 1000);

    if (hw_caps.cuda_available || hw_caps.rocm_available || hw_caps.opencl_available) {
        // GPU should be recommended for large networks
        EXPECT_TRUE(
            mode_large == EXEC_MODE_GPU_CUDA ||
            mode_large == EXEC_MODE_GPU_ROCM ||
            mode_large == EXEC_MODE_GPU_OPENCL ||
            mode_large == EXEC_MODE_HYBRID
        );
    } else {
        // Fall back to CPU
        EXPECT_TRUE(
            mode_large == EXEC_MODE_CPU_SEQUENTIAL ||
            mode_large == EXEC_MODE_CPU_PARALLEL
        );
    }
}

TEST_F(ExecutionModeTest, DefaultConfig) {
    execution_config_t cpu_config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);
    EXPECT_EQ(cpu_config.mode, EXEC_MODE_CPU_PARALLEL);
    EXPECT_GT(cpu_config.cpu_threads, 0u);
    EXPECT_TRUE(cpu_config.auto_fallback);

    execution_config_t gpu_config = execution_get_default_config(EXEC_MODE_GPU_CUDA);
    EXPECT_EQ(gpu_config.mode, EXEC_MODE_GPU_CUDA);
    EXPECT_GT(gpu_config.gpu_threads_per_block, 0u);
}

TEST_F(ExecutionModeTest, OptimalConfig) {
    // Get optimal config for small network
    execution_config_t config_small = execution_get_optimal_config(100);
    EXPECT_TRUE(
        config_small.mode == EXEC_MODE_CPU_SEQUENTIAL ||
        config_small.mode == EXEC_MODE_CPU_PARALLEL
    );

    // Get optimal config for medium network
    execution_config_t config_medium = execution_get_optimal_config(10000);
    // Should have reasonable defaults
    EXPECT_GT(config_medium.batch_size, 0u);
}

//=============================================================================
// Execution Context Tests
//=============================================================================

TEST_F(ExecutionModeTest, CreateCPUContext) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);

    ctx = execution_context_create(&config);
    EXPECT_NE(ctx, nullptr);

    execution_mode_t active_mode = execution_context_get_mode(ctx);
    EXPECT_EQ(active_mode, EXEC_MODE_CPU_PARALLEL);
}

TEST_F(ExecutionModeTest, CreateAutoContext) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_AUTO);

    ctx = execution_context_create(&config);
    EXPECT_NE(ctx, nullptr);

    // Should have resolved to a concrete mode
    execution_mode_t active_mode = execution_context_get_mode(ctx);
    EXPECT_NE(active_mode, EXEC_MODE_AUTO);
}

TEST_F(ExecutionModeTest, DestroyNullContext) {
    // Should be safe to destroy NULL
    execution_context_destroy(nullptr);
}

TEST_F(ExecutionModeTest, Synchronize) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    bool result = execution_synchronize(ctx);
    EXPECT_TRUE(result);
}

TEST_F(ExecutionModeTest, GetStats) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    uint64_t total_ops = 0;
    double total_time = 0.0;

    bool result = execution_get_stats(ctx, &total_ops, &total_time);
    EXPECT_TRUE(result);
    // Stats should be 0 for new context
    EXPECT_EQ(total_ops, 0u);
}

//=============================================================================
// Memory Management Tests
//=============================================================================

TEST_F(ExecutionModeTest, AllocAndFreeCPU) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Allocate memory
    size_t size = 1024;
    void* ptr = execution_alloc(ctx, size);
    EXPECT_NE(ptr, nullptr);

    // Write to it (verify it's usable)
    memset(ptr, 0xAB, size);

    // Free it
    execution_free(ctx, ptr);
}

TEST_F(ExecutionModeTest, FreeNullPointer) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Should be safe to free NULL
    execution_free(ctx, nullptr);
}

TEST_F(ExecutionModeTest, MemcpyCPU) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    size_t size = 256;
    void* src = execution_alloc(ctx, size);
    void* dst = execution_alloc(ctx, size);
    ASSERT_NE(src, nullptr);
    ASSERT_NE(dst, nullptr);

    // Fill source with pattern
    memset(src, 0x42, size);

    // Copy (in CPU mode, direction doesn't matter)
    bool result = execution_memcpy(ctx, dst, src, size, false);
    EXPECT_TRUE(result);

    // Verify copy
    EXPECT_EQ(memcmp(src, dst, size), 0);

    execution_free(ctx, src);
    execution_free(ctx, dst);
}

//=============================================================================
// Fallback Tests
//=============================================================================

TEST_F(ExecutionFallbackTest, FallbackTooCPUWhenGPUUnavailable) {
    // Try to create GPU context on system without GPU (or with fallback enabled)
    execution_config_t config = execution_get_default_config(EXEC_MODE_GPU_CUDA);
    config.auto_fallback = true;
    config.fallback_mode = EXEC_MODE_CPU_PARALLEL;

    ctx = execution_context_create(&config);

    // Should succeed (either GPU or fallback to CPU)
    EXPECT_NE(ctx, nullptr);

    execution_mode_t active_mode = execution_context_get_mode(ctx);

    // Test should pass regardless of whether GPU is actually available
    // The key invariant is that we get a valid mode (GPU or fallback)
    // CUDA may be "detected" but fail at runtime, so we accept either mode
    EXPECT_TRUE(
        active_mode == EXEC_MODE_GPU_CUDA ||
        active_mode == EXEC_MODE_CPU_PARALLEL ||
        active_mode == EXEC_MODE_CPU_SEQUENTIAL
    );
}

TEST_F(ExecutionFallbackTest, ModeSwitch) {
    // Start with CPU
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(execution_context_get_mode(ctx), EXEC_MODE_CPU_PARALLEL);

    // Try to switch to sequential
    bool result = execution_context_set_mode(ctx, EXEC_MODE_CPU_SEQUENTIAL);
    EXPECT_TRUE(result);
    EXPECT_EQ(execution_context_get_mode(ctx), EXEC_MODE_CPU_SEQUENTIAL);

    // Switch back
    result = execution_context_set_mode(ctx, EXEC_MODE_CPU_PARALLEL);
    EXPECT_TRUE(result);
    EXPECT_EQ(execution_context_get_mode(ctx), EXEC_MODE_CPU_PARALLEL);
}

TEST_F(ExecutionFallbackTest, ModeSwitchToUnsupportedFails) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Check if CUDA is supported
    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    if (!caps.cuda_available) {
        // Try to switch to unsupported CUDA mode
        bool result = execution_context_set_mode(ctx, EXEC_MODE_GPU_CUDA);
        EXPECT_FALSE(result);

        // Should remain in original mode
        EXPECT_EQ(execution_context_get_mode(ctx), EXEC_MODE_CPU_PARALLEL);
    }
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(ExecutionModeTest, MultipleContexts) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);

    std::vector<execution_context_t> contexts;

    // Create multiple contexts
    for (int i = 0; i < 5; ++i) {
        execution_context_t new_ctx = execution_context_create(&config);
        ASSERT_NE(new_ctx, nullptr);
        contexts.push_back(new_ctx);
    }

    // Destroy them
    for (auto& c : contexts) {
        execution_context_destroy(c);
    }
}

TEST_F(ExecutionModeTest, RepeatedAllocationsFree) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Allocate and free many times
    for (int i = 0; i < 100; ++i) {
        size_t size = 1024 * (i + 1);
        void* ptr = execution_alloc(ctx, size);
        EXPECT_NE(ptr, nullptr);
        memset(ptr, 0, size);  // Touch memory
        execution_free(ctx, ptr);
    }
}

//=============================================================================
// Integration Scenario Tests
//=============================================================================

TEST_F(ExecutionModeTest, TypicalWorkflow) {
    // 1. Detect hardware
    hardware_capabilities_t caps;
    EXPECT_TRUE(execution_detect_capabilities(&caps));

    // 2. Get optimal config for network size
    execution_config_t config = execution_get_optimal_config(10000);

    // 3. Create context
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // 4. Allocate buffers
    size_t neuron_data_size = 10000 * sizeof(float);
    void* neurons = execution_alloc(ctx, neuron_data_size);
    ASSERT_NE(neurons, nullptr);

    size_t synapse_data_size = 100000 * sizeof(float);
    void* synapses = execution_alloc(ctx, synapse_data_size);
    ASSERT_NE(synapses, nullptr);

    // 5. Initialize data
    memset(neurons, 0, neuron_data_size);
    memset(synapses, 0, synapse_data_size);

    // 6. Simulate some work (just memory access for this test)
    float* neuron_data = static_cast<float*>(neurons);
    for (size_t i = 0; i < 10000; ++i) {
        neuron_data[i] = static_cast<float>(i) * 0.001f;
    }

    // 7. Synchronize
    EXPECT_TRUE(execution_synchronize(ctx));

    // 8. Get stats
    uint64_t ops;
    double time_ms;
    EXPECT_TRUE(execution_get_stats(ctx, &ops, &time_ms));

    // 9. Cleanup
    execution_free(ctx, neurons);
    execution_free(ctx, synapses);
}

TEST_F(ExecutionFallbackTest, GracefulDegradationScenario) {
    // Simulate a system where we want GPU but fall back to CPU

    // First check what's available
    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    // Configure for GPU with fallback
    execution_config_t config;
    memset(&config, 0, sizeof(config));

    if (caps.cuda_available) {
        config.mode = EXEC_MODE_GPU_CUDA;
    } else if (caps.rocm_available) {
        config.mode = EXEC_MODE_GPU_ROCM;
    } else if (caps.opencl_available) {
        config.mode = EXEC_MODE_GPU_OPENCL;
    } else {
        config.mode = EXEC_MODE_GPU_CUDA;  // Will need to fallback
    }

    config.auto_fallback = true;
    config.fallback_mode = EXEC_MODE_CPU_PARALLEL;
    config.cpu_threads = caps.cpu_threads;
    config.batch_size = 32;

    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    execution_mode_t actual_mode = execution_context_get_mode(ctx);

    // Should be running in *some* valid mode
    EXPECT_TRUE(
        actual_mode == EXEC_MODE_GPU_CUDA ||
        actual_mode == EXEC_MODE_GPU_ROCM ||
        actual_mode == EXEC_MODE_GPU_OPENCL ||
        actual_mode == EXEC_MODE_CPU_PARALLEL ||
        actual_mode == EXEC_MODE_CPU_SEQUENTIAL
    );

    // Basic operations should work regardless of mode
    void* mem = execution_alloc(ctx, 4096);
    EXPECT_NE(mem, nullptr);
    execution_free(ctx, mem);
}

//=============================================================================
// Mode Switching Tests (Enhanced for resource cleanup/init)
//=============================================================================

class ModeSwitchingTest : public ::testing::Test {
protected:
    execution_context_t ctx = nullptr;
    hardware_capabilities_t hw_caps;

    void SetUp() override {
        memset(&hw_caps, 0, sizeof(hw_caps));
        execution_detect_capabilities(&hw_caps);
    }

    void TearDown() override {
        if (ctx) {
            execution_context_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(ModeSwitchingTest, SwitchBetweenCPUModes) {
    // Start with parallel mode
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(execution_context_get_mode(ctx), EXEC_MODE_CPU_PARALLEL);

    // Switch to sequential
    bool result = execution_context_set_mode(ctx, EXEC_MODE_CPU_SEQUENTIAL);
    EXPECT_TRUE(result);
    EXPECT_EQ(execution_context_get_mode(ctx), EXEC_MODE_CPU_SEQUENTIAL);

    // Switch back to parallel
    result = execution_context_set_mode(ctx, EXEC_MODE_CPU_PARALLEL);
    EXPECT_TRUE(result);
    EXPECT_EQ(execution_context_get_mode(ctx), EXEC_MODE_CPU_PARALLEL);
}

TEST_F(ModeSwitchingTest, SwitchToSameMode_NoOp) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Switching to same mode should succeed quickly
    EXPECT_TRUE(execution_context_set_mode(ctx, EXEC_MODE_CPU_PARALLEL));
    EXPECT_EQ(execution_context_get_mode(ctx), EXEC_MODE_CPU_PARALLEL);
}

TEST_F(ModeSwitchingTest, SwitchToGPU_FallsBackWhenUnavailable) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    config.auto_fallback = true;
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Try to switch to CUDA
    bool result = execution_context_set_mode(ctx, EXEC_MODE_GPU_CUDA);

    // CUDA may be "detected" but fail at runtime initialization
    // The key invariant: we either get GPU mode or remain in a valid CPU mode
    execution_mode_t current_mode = execution_context_get_mode(ctx);

    if (result) {
        // Switch succeeded - should be in GPU mode
        EXPECT_EQ(current_mode, EXEC_MODE_GPU_CUDA);
    } else {
        // Switch failed - should remain in a valid CPU mode
        EXPECT_TRUE(
            current_mode == EXEC_MODE_CPU_SEQUENTIAL ||
            current_mode == EXEC_MODE_CPU_PARALLEL
        );
    }
}

TEST_F(ModeSwitchingTest, MultipleSwitches_ResourceCleanup) {
    // Test that resources are properly cleaned up on each switch
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    config.auto_fallback = true;
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Multiple switches
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(execution_context_set_mode(ctx, EXEC_MODE_CPU_PARALLEL));
        EXPECT_EQ(execution_context_get_mode(ctx), EXEC_MODE_CPU_PARALLEL);

        EXPECT_TRUE(execution_context_set_mode(ctx, EXEC_MODE_CPU_SEQUENTIAL));
        EXPECT_EQ(execution_context_get_mode(ctx), EXEC_MODE_CPU_SEQUENTIAL);
    }

    // Allocate and free memory to verify context is still valid
    void* mem = execution_alloc(ctx, 1024);
    EXPECT_NE(mem, nullptr);
    execution_free(ctx, mem);
}

TEST_F(ModeSwitchingTest, SwitchWithPendingOperations) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Allocate some memory (simulate pending operations)
    void* mem1 = execution_alloc(ctx, 4096);
    void* mem2 = execution_alloc(ctx, 8192);
    EXPECT_NE(mem1, nullptr);
    EXPECT_NE(mem2, nullptr);

    // Write some data
    memset(mem1, 0xAA, 4096);
    memset(mem2, 0xBB, 8192);

    // Switch mode (should synchronize first)
    EXPECT_TRUE(execution_context_set_mode(ctx, EXEC_MODE_CPU_SEQUENTIAL));

    // Free memory (should still work after switch)
    execution_free(ctx, mem1);
    execution_free(ctx, mem2);
}

TEST_F(ModeSwitchingTest, SwitchToHybridMode) {
    if (!hw_caps.cuda_available && hw_caps.cpu_threads < 2) {
        GTEST_SKIP() << "Hybrid mode requires GPU or multiple CPU threads";
    }

    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    config.auto_fallback = true;
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Try hybrid mode
    bool result = execution_context_set_mode(ctx, EXEC_MODE_HYBRID);

    // Hybrid should work on most systems (at least CPU parallel part)
    if (result) {
        EXPECT_EQ(execution_context_get_mode(ctx), EXEC_MODE_HYBRID);

        // Should be able to allocate memory
        void* mem = execution_alloc(ctx, 2048);
        EXPECT_NE(mem, nullptr);
        execution_free(ctx, mem);
    }
}

//=============================================================================
// GPU Neural Network Integration Tests
//=============================================================================

class GPUNeuralNetworkIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Nothing special
    }

    void TearDown() override {
        // Nothing special
    }
};

TEST_F(GPUNeuralNetworkIntegrationTest, CreateWithGPUMode_FallsBack) {
    gpu_network_config_t config = {};
    config.num_neurons = 100;
    config.num_synapses = 1000;
    config.threads_per_block = 256;
    config.max_blocks = 1024;
    config.spike_queue_capacity = 1000;
    config.use_unified_memory = false;
    config.pin_host_memory = false;
    config.exec_mode = EXEC_MODE_GPU_CUDA;
    config.enable_stdp = true;
    config.global_learning_rate = 0.01f;

    gpu_neural_network_t network = gpu_neural_network_create(&config);

    // Should always succeed (GPU or CPU fallback)
    ASSERT_NE(network, nullptr);

    // Add neurons
    gpu_neuron_state_t state = {};
    state.membrane_potential = -65.0f;
    state.threshold = -55.0f;
    state.bias = 0.1f;
    state.refractory_period = 2000;

    for (int i = 0; i < 100; i++) {
        uint32_t id = gpu_neural_network_add_neuron(network, &state);
        EXPECT_EQ(id, (uint32_t)i);
    }

    // Add synapses
    for (int i = 0; i < 99; i++) {
        EXPECT_TRUE(gpu_neural_network_add_synapse(network, i, i + 1, 0.5f, 1.0f));
    }

    // Run updates (should work regardless of GPU availability)
    for (uint64_t t = 0; t < 10; t++) {
        uint32_t spikes = gpu_neural_network_update(network, t * 1000, 1000);
        EXPECT_GE(spikes, 0u);
    }

    // Synchronize
    EXPECT_TRUE(gpu_neural_network_synchronize(network));

    // Get stats
    uint64_t total_spikes = 0;
    float avg_rate = 0.0f;
    uint64_t gpu_mem = 0;
    EXPECT_TRUE(gpu_neural_network_get_stats(network, &total_spikes, &avg_rate, &gpu_mem));

    gpu_neural_network_destroy(network);
}

TEST_F(GPUNeuralNetworkIntegrationTest, EndToEndSimulation) {
    // Determine best mode
    hardware_capabilities_t hw_caps;
    execution_detect_capabilities(&hw_caps);

    gpu_network_config_t config = {};
    config.num_neurons = 50;
    config.num_synapses = 500;
    config.threads_per_block = 256;
    config.max_blocks = 256;
    config.spike_queue_capacity = 500;
    config.exec_mode = hw_caps.cuda_available ? EXEC_MODE_GPU_CUDA : EXEC_MODE_CPU_SEQUENTIAL;
    config.enable_stdp = true;
    config.global_learning_rate = 0.01f;

    gpu_neural_network_t network = gpu_neural_network_create(&config);
    ASSERT_NE(network, nullptr);

    // Create network with varied initial states
    for (int i = 0; i < 50; i++) {
        gpu_neuron_state_t state = {};
        state.membrane_potential = -65.0f + (float)(i % 10);  // Varied potentials
        state.threshold = -55.0f;
        state.bias = 0.05f + (float)(i % 5) * 0.01f;
        state.learning_rate = 0.01f;
        state.refractory_period = 2000;

        gpu_neural_network_add_neuron(network, &state);
    }

    // Create random-ish connectivity
    for (int i = 0; i < 50; i++) {
        for (int j = 0; j < 50; j++) {
            if (i != j && (i * j) % 7 == 0) {
                gpu_neural_network_add_synapse(network, i, j, 0.3f, 1.0f);
            }
        }
    }

    // Run simulation
    for (uint64_t t = 0; t < 100; t++) {
        gpu_neural_network_update(network, t * 1000, 1000);

        // Apply STDP periodically
        if (t % 10 == 0) {
            gpu_neural_network_apply_stdp(network, t * 1000);
        }
    }

    // Final sync and stats
    EXPECT_TRUE(gpu_neural_network_synchronize(network));

    uint64_t total_spikes = 0;
    float avg_rate = 0.0f;
    EXPECT_TRUE(gpu_neural_network_get_stats(network, &total_spikes, &avg_rate, nullptr));

    gpu_neural_network_destroy(network);
}

//=============================================================================
// Spike Queue GPU Sync Integration Tests
//=============================================================================

class SpikeQueueGPUSyncTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Nothing
    }

    void TearDown() override {
        // Nothing
    }
};

TEST_F(SpikeQueueGPUSyncTest, SyncDisabledForCPUQueue) {
    spike_queue_t* queue = spike_queue_create(100, false);  // GPU disabled
    ASSERT_NE(queue, nullptr);

    // Sync should return false when GPU not enabled
    EXPECT_FALSE(spike_queue_sync_gpu(queue, true));
    EXPECT_FALSE(spike_queue_sync_gpu(queue, false));

    spike_queue_destroy(queue);
}

TEST_F(SpikeQueueGPUSyncTest, QueueOperationsWork) {
    spike_queue_t* queue = spike_queue_create(100, false);
    ASSERT_NE(queue, nullptr);

    EXPECT_TRUE(spike_queue_is_empty(queue));

    // Add some events
    for (int i = 0; i < 10; i++) {
        spike_event_t event = {};
        event.timestamp = (uint64_t)(i * 1000);
        event.source_id = (uint32_t)i;
        event.target_id = (uint32_t)(i + 1);
        event.amplitude = 1.0f;

        EXPECT_TRUE(spike_queue_push(queue, &event));
    }

    EXPECT_EQ(spike_queue_size(queue), 10u);
    EXPECT_FALSE(spike_queue_is_empty(queue));

    // Pop events
    for (int i = 0; i < 10; i++) {
        spike_event_t event;
        EXPECT_TRUE(spike_queue_pop(queue, &event));
    }

    EXPECT_TRUE(spike_queue_is_empty(queue));

    spike_queue_destroy(queue);
}

//=============================================================================
// Resource Management Integration Tests
//=============================================================================

class ResourceManagementTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResourceManagementTest, RepeatedContextCreationDestruction) {
    // Test for resource leaks
    for (int i = 0; i < 20; i++) {
        execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);
        execution_context_t ctx = execution_context_create(&config);
        ASSERT_NE(ctx, nullptr);

        // Do some work
        void* mem = execution_alloc(ctx, 4096);
        EXPECT_NE(mem, nullptr);
        memset(mem, 0, 4096);
        execution_free(ctx, mem);

        execution_context_destroy(ctx);
    }
    SUCCEED();
}

TEST_F(ResourceManagementTest, RepeatedNetworkCreationDestruction) {
    for (int i = 0; i < 10; i++) {
        gpu_network_config_t config = {};
        config.num_neurons = 100;
        config.num_synapses = 1000;
        config.exec_mode = EXEC_MODE_CPU_SEQUENTIAL;
        config.enable_stdp = false;

        gpu_neural_network_t network = gpu_neural_network_create(&config);
        ASSERT_NE(network, nullptr);

        gpu_neuron_state_t state = {};
        state.membrane_potential = -65.0f;
        state.threshold = -55.0f;

        for (int j = 0; j < 100; j++) {
            gpu_neural_network_add_neuron(network, &state);
        }

        for (uint64_t t = 0; t < 5; t++) {
            gpu_neural_network_update(network, t * 1000, 1000);
        }

        gpu_neural_network_destroy(network);
    }
    SUCCEED();
}

TEST_F(ResourceManagementTest, MixedOperationsStressTest) {
    // Create multiple contexts and networks simultaneously
    std::vector<execution_context_t> contexts;
    std::vector<gpu_neural_network_t> networks;

    // Create resources
    for (int i = 0; i < 3; i++) {
        execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);
        execution_context_t ctx = execution_context_create(&config);
        ASSERT_NE(ctx, nullptr);
        contexts.push_back(ctx);

        gpu_network_config_t net_config = {};
        net_config.num_neurons = 50;
        net_config.num_synapses = 500;
        net_config.exec_mode = EXEC_MODE_CPU_SEQUENTIAL;

        gpu_neural_network_t network = gpu_neural_network_create(&net_config);
        ASSERT_NE(network, nullptr);
        networks.push_back(network);
    }

    // Do work with all resources
    for (size_t i = 0; i < contexts.size(); i++) {
        void* mem = execution_alloc(contexts[i], 1024);
        EXPECT_NE(mem, nullptr);
        execution_free(contexts[i], mem);

        gpu_neuron_state_t state = {};
        state.membrane_potential = -65.0f;
        state.threshold = -55.0f;
        for (int j = 0; j < 50; j++) {
            gpu_neural_network_add_neuron(networks[i], &state);
        }
        gpu_neural_network_update(networks[i], 1000, 1000);
    }

    // Cleanup
    for (auto& ctx : contexts) {
        execution_context_destroy(ctx);
    }
    for (auto& network : networks) {
        gpu_neural_network_destroy(network);
    }

    SUCCEED();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
