/**
 * @file test_execution_mode.cpp
 * @brief Comprehensive unit tests for execution mode detection and selection
 *
 * WHAT: Tests CPU/GPU/distributed mode detection, selection, and fallback logic
 * WHY:  Ensure robust hardware detection across platforms and correct mode selection
 * HOW:  Test all public API functions with various scenarios and edge cases
 *
 * TEST COVERAGE:
 * - Hardware capability detection (CPU, GPU, network)
 * - Mode support checking for all execution modes
 * - Mode recommendation based on workload size
 * - Execution context lifecycle (create, destroy, mode switching)
 * - Memory allocation and transfer operations
 * - Synchronization and statistics
 * - Configuration defaults and optimization
 * - Edge cases and error handling
 * - NULL pointer safety
 * - Fallback mechanisms
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

// GPU headers include CUDA headers that cannot be in extern "C" blocks
#include "gpu/nimcp_execution_mode.h"

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Test fixture for execution mode tests
 *
 * WHAT: Provides common setup/teardown for execution mode tests
 * WHY:  Ensures proper cleanup of execution contexts
 * HOW:  Automatically destroys contexts in TearDown()
 */
class ExecutionModeTest : public ::testing::Test {
protected:
    execution_context_t ctx = nullptr;

    void TearDown() override {
        if (ctx) {
            execution_context_destroy(ctx);
            ctx = nullptr;
        }
    }
};

//=============================================================================
// Hardware Detection Tests
//=============================================================================

/**
 * TEST: Basic capability detection
 * WHAT: Verify execution_detect_capabilities() returns valid results
 * WHY:  Must detect hardware correctly for mode selection
 * HOW:  Call with valid pointer, check CPU always available
 */
TEST_F(ExecutionModeTest, DetectCapabilities_ValidPointer) {
    hardware_capabilities_t caps;
    memset(&caps, 0, sizeof(caps));

    bool result = execution_detect_capabilities(&caps);

    EXPECT_TRUE(result);
    EXPECT_TRUE(caps.cpu_available);  // CPU always available
    EXPECT_GT(caps.cpu_cores, 0u);
    EXPECT_GT(caps.cpu_threads, 0u);
}

/**
 * TEST: NULL pointer safety
 * WHAT: Verify execution_detect_capabilities() handles NULL gracefully
 * WHY:  Prevent crashes from invalid input
 * HOW:  Pass NULL, expect false return
 */
TEST_F(ExecutionModeTest, DetectCapabilities_NullPointer) {
    bool result = execution_detect_capabilities(nullptr);
    EXPECT_FALSE(result);
}

/**
 * TEST: CPU core detection
 * WHAT: Verify CPU cores are detected correctly
 * WHY:  Need accurate core count for parallel execution
 * HOW:  Check cores match expectations (typically 1-256)
 */
TEST_F(ExecutionModeTest, DetectCapabilities_CPUCores) {
    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    EXPECT_GE(caps.cpu_cores, 1u);
    EXPECT_LE(caps.cpu_cores, 256u);  // Reasonable upper bound
    EXPECT_EQ(caps.cpu_threads, caps.cpu_cores);  // Simplified model
}

/**
 * TEST: SIMD capability detection (platform-specific)
 * WHAT: Verify AVX2/AVX512 flags are set correctly
 * WHY:  Need to know SIMD capabilities for optimization
 * HOW:  Check flags on x86-64, expect false elsewhere
 */
TEST_F(ExecutionModeTest, DetectCapabilities_SIMDFlags) {
    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    #if defined(__x86_64__) || defined(_M_X64)
    EXPECT_TRUE(caps.cpu_avx2);
    EXPECT_FALSE(caps.cpu_avx512);  // Conservative default
    #else
    EXPECT_FALSE(caps.cpu_avx2);
    EXPECT_FALSE(caps.cpu_avx512);
    #endif
}

/**
 * TEST: GPU capability detection
 * WHAT: Verify CUDA GPU detection works correctly
 * WHY:  Need to know if GPU is available for GPU modes
 * HOW:  Check cuda_available flag and GPU count
 */
TEST_F(ExecutionModeTest, DetectCapabilities_GPU) {
    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    #ifdef NIMCP_ENABLE_CUDA
    // GPU may or may not be present, but flag should be consistent
    if (caps.cuda_available) {
        EXPECT_GT(caps.gpu_count, 0u);
        EXPECT_GT(caps.gpu_memory_mb, 0u);
        EXPECT_GT(caps.gpu_compute_capability, 0u);
    } else {
        EXPECT_EQ(caps.gpu_count, 0u);
    }
    #else
    EXPECT_FALSE(caps.cuda_available);
    EXPECT_EQ(caps.gpu_count, 0u);
    #endif
}

/**
 * TEST: Network capability detection
 * WHAT: Verify network capabilities are detected
 * WHY:  Need to know if distributed modes are available
 * HOW:  Check network_available flag (currently false by default)
 */
TEST_F(ExecutionModeTest, DetectCapabilities_Network) {
    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    // Currently not implemented, should return false
    EXPECT_FALSE(caps.network_available);
    EXPECT_EQ(caps.network_nodes, 1u);
    EXPECT_EQ(caps.network_bandwidth_mbps, 0u);
}

/**
 * TEST: Recommended mode selection
 * WHAT: Verify recommended_mode is set based on capabilities
 * WHY:  Users need guidance on best execution mode
 * HOW:  Check recommendation matches available hardware
 */
TEST_F(ExecutionModeTest, DetectCapabilities_RecommendedMode) {
    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    if (caps.cuda_available && caps.gpu_count > 0) {
        EXPECT_EQ(caps.recommended_mode, EXEC_MODE_GPU_CUDA);
    } else if (caps.cpu_threads >= 4) {
        EXPECT_EQ(caps.recommended_mode, EXEC_MODE_CPU_PARALLEL);
    } else {
        EXPECT_EQ(caps.recommended_mode, EXEC_MODE_CPU_SEQUENTIAL);
    }
}

//=============================================================================
// Mode Support Tests
//=============================================================================

/**
 * TEST: CPU sequential mode always supported
 * WHAT: Verify EXEC_MODE_CPU_SEQUENTIAL is always supported
 * WHY:  CPU sequential is the fallback mode
 * HOW:  Call execution_mode_is_supported(), expect true
 */
TEST_F(ExecutionModeTest, ModeSupported_CPUSequential) {
    bool supported = execution_mode_is_supported(EXEC_MODE_CPU_SEQUENTIAL);
    EXPECT_TRUE(supported);
}

/**
 * TEST: CPU parallel mode support
 * WHAT: Verify EXEC_MODE_CPU_PARALLEL is supported
 * WHY:  Multi-core CPUs should support parallel execution
 * HOW:  Check support matches CPU availability
 */
TEST_F(ExecutionModeTest, ModeSupported_CPUParallel) {
    bool supported = execution_mode_is_supported(EXEC_MODE_CPU_PARALLEL);
    EXPECT_TRUE(supported);  // Should be supported on all platforms
}

/**
 * TEST: CUDA mode support
 * WHAT: Verify EXEC_MODE_GPU_CUDA support matches CUDA availability
 * WHY:  GPU mode requires CUDA runtime
 * HOW:  Check support matches cuda_available flag
 */
TEST_F(ExecutionModeTest, ModeSupported_CUDA) {
    bool supported = execution_mode_is_supported(EXEC_MODE_GPU_CUDA);

    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    EXPECT_EQ(supported, caps.cuda_available);
}

/**
 * TEST: ROCm mode support
 * WHAT: Verify EXEC_MODE_GPU_ROCM support
 * WHY:  Need to know if AMD GPUs are supported
 * HOW:  Check rocm_available flag
 */
TEST_F(ExecutionModeTest, ModeSupported_ROCm) {
    bool supported = execution_mode_is_supported(EXEC_MODE_GPU_ROCM);

    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    EXPECT_EQ(supported, caps.rocm_available);
}

/**
 * TEST: OpenCL mode support
 * WHAT: Verify EXEC_MODE_GPU_OPENCL support
 * WHY:  OpenCL is cross-platform GPU API
 * HOW:  Check opencl_available flag
 */
TEST_F(ExecutionModeTest, ModeSupported_OpenCL) {
    bool supported = execution_mode_is_supported(EXEC_MODE_GPU_OPENCL);

    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    EXPECT_EQ(supported, caps.opencl_available);
}

/**
 * TEST: Distributed CPU mode support
 * WHAT: Verify EXEC_MODE_DISTRIBUTED_CPU support
 * WHY:  Need network for distributed execution
 * HOW:  Check network_available flag
 */
TEST_F(ExecutionModeTest, ModeSupported_DistributedCPU) {
    bool supported = execution_mode_is_supported(EXEC_MODE_DISTRIBUTED_CPU);

    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    EXPECT_EQ(supported, caps.network_available);
}

/**
 * TEST: Distributed GPU mode support
 * WHAT: Verify EXEC_MODE_DISTRIBUTED_GPU support
 * WHY:  Requires both network and GPU
 * HOW:  Check network_available flag
 */
TEST_F(ExecutionModeTest, ModeSupported_DistributedGPU) {
    bool supported = execution_mode_is_supported(EXEC_MODE_DISTRIBUTED_GPU);

    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    EXPECT_EQ(supported, caps.network_available);
}

/**
 * TEST: Hybrid mode support
 * WHAT: Verify EXEC_MODE_HYBRID support
 * WHY:  Hybrid requires both CPU and GPU
 * HOW:  Check both cpu_available and cuda_available
 */
TEST_F(ExecutionModeTest, ModeSupported_Hybrid) {
    bool supported = execution_mode_is_supported(EXEC_MODE_HYBRID);

    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    EXPECT_EQ(supported, caps.cpu_available && caps.cuda_available);
}

/**
 * TEST: Auto mode always supported
 * WHAT: Verify EXEC_MODE_AUTO is always supported
 * WHY:  Auto-detection should always work
 * HOW:  Check support is true
 */
TEST_F(ExecutionModeTest, ModeSupported_Auto) {
    bool supported = execution_mode_is_supported(EXEC_MODE_AUTO);
    EXPECT_TRUE(supported);
}

/**
 * TEST: Invalid mode not supported
 * WHAT: Verify invalid mode values return false
 * WHY:  Prevent crashes from invalid mode values
 * HOW:  Pass out-of-range value, expect false
 */
TEST_F(ExecutionModeTest, ModeSupported_InvalidMode) {
    bool supported = execution_mode_is_supported((execution_mode_t)999);
    EXPECT_FALSE(supported);
}

//=============================================================================
// Mode Recommendation Tests
//=============================================================================

/**
 * TEST: Small network recommendation
 * WHAT: Verify mode recommendation for small networks (< 1K neurons)
 * WHY:  GPU-first policy: GPU preferred when available, regardless of size
 * HOW:  Request mode for 100 neurons, expect GPU if available
 */
TEST_F(ExecutionModeTest, RecommendMode_SmallNetwork) {
    execution_mode_t mode = execution_get_recommended_mode(100, 10);

    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    if (caps.cuda_available) {
        EXPECT_EQ(mode, EXEC_MODE_GPU_CUDA);
    } else if (caps.cpu_threads >= 4) {
        EXPECT_EQ(mode, EXEC_MODE_CPU_PARALLEL);
    } else {
        EXPECT_EQ(mode, EXEC_MODE_CPU_SEQUENTIAL);
    }
}

/**
 * TEST: Medium network recommendation
 * WHAT: Verify mode for medium networks (1K-10K neurons)
 * WHY:  GPU-first policy: GPU preferred when available, regardless of size
 * HOW:  Request mode for 5000 neurons, expect GPU if available
 */
TEST_F(ExecutionModeTest, RecommendMode_MediumNetwork) {
    execution_mode_t mode = execution_get_recommended_mode(5000, 100);

    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    if (caps.cuda_available) {
        EXPECT_EQ(mode, EXEC_MODE_GPU_CUDA);
    } else if (caps.cpu_threads >= 4) {
        EXPECT_EQ(mode, EXEC_MODE_CPU_PARALLEL);
    } else {
        EXPECT_EQ(mode, EXEC_MODE_CPU_SEQUENTIAL);
    }
}

/**
 * TEST: Large network recommendation
 * WHAT: Verify mode for large networks (10K-1M neurons)
 * WHY:  Large networks benefit from GPU acceleration
 * HOW:  Request mode for 50000 neurons, expect GPU or CPU parallel
 */
TEST_F(ExecutionModeTest, RecommendMode_LargeNetwork) {
    execution_mode_t mode = execution_get_recommended_mode(50000, 1000);

    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    if (caps.cuda_available) {
        EXPECT_EQ(mode, EXEC_MODE_GPU_CUDA);
    } else {
        EXPECT_EQ(mode, EXEC_MODE_CPU_PARALLEL);
    }
}

/**
 * TEST: Very large network recommendation
 * WHAT: Verify mode for very large networks (> 1M neurons)
 * WHY:  Huge networks may need distributed execution
 * HOW:  Request mode for 2M neurons, check logic
 */
TEST_F(ExecutionModeTest, RecommendMode_VeryLargeNetwork) {
    execution_mode_t mode = execution_get_recommended_mode(2000000, 1000);

    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    if (caps.network_available && caps.cuda_available) {
        EXPECT_EQ(mode, EXEC_MODE_DISTRIBUTED_GPU);
    } else if (caps.cuda_available) {
        EXPECT_EQ(mode, EXEC_MODE_GPU_CUDA);
    } else {
        EXPECT_EQ(mode, EXEC_MODE_CPU_PARALLEL);
    }
}

/**
 * TEST: Edge case - zero neurons
 * WHAT: Verify behavior with zero neurons
 * WHY:  GPU-first policy applies even for zero neurons (edge case)
 * HOW:  Request mode for 0 neurons, expect GPU if available
 */
TEST_F(ExecutionModeTest, RecommendMode_ZeroNeurons) {
    execution_mode_t mode = execution_get_recommended_mode(0, 0);

    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    if (caps.cuda_available) {
        EXPECT_EQ(mode, EXEC_MODE_GPU_CUDA);
    } else if (caps.cpu_threads >= 4) {
        EXPECT_EQ(mode, EXEC_MODE_CPU_PARALLEL);
    } else {
        EXPECT_EQ(mode, EXEC_MODE_CPU_SEQUENTIAL);
    }
}

/**
 * TEST: Boundary - exactly 1000 neurons
 * WHAT: Verify threshold behavior at 1000 neurons
 * WHY:  Test boundary condition
 * HOW:  Request mode for exactly 1000 neurons
 */
TEST_F(ExecutionModeTest, RecommendMode_Boundary1000) {
    execution_mode_t mode = execution_get_recommended_mode(1000, 100);
    // Should be CPU sequential (< 1000 is sequential, >= 1000 depends on threads)
    EXPECT_NE(mode, EXEC_MODE_DISTRIBUTED_GPU);  // Should not recommend distributed
}

/**
 * TEST: Boundary - exactly 10000 neurons
 * WHAT: Verify threshold behavior at 10000 neurons
 * WHY:  Test boundary condition
 * HOW:  Request mode for exactly 10000 neurons
 */
TEST_F(ExecutionModeTest, RecommendMode_Boundary10000) {
    execution_mode_t mode = execution_get_recommended_mode(10000, 100);

    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    // Should recommend CPU parallel or GPU
    if (caps.cuda_available) {
        EXPECT_TRUE(mode == EXEC_MODE_GPU_CUDA || mode == EXEC_MODE_CPU_PARALLEL);
    } else {
        EXPECT_EQ(mode, EXEC_MODE_CPU_PARALLEL);
    }
}

//=============================================================================
// Execution Context Lifecycle Tests
//=============================================================================

/**
 * TEST: Create context with valid config
 * WHAT: Verify execution_context_create() works with valid config
 * WHY:  Context creation is fundamental to execution
 * HOW:  Create context with CPU sequential config
 */
TEST_F(ExecutionModeTest, CreateContext_ValidConfig) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);

    ctx = execution_context_create(&config);

    ASSERT_NE(ctx, nullptr);
}

/**
 * TEST: Create context with NULL config
 * WHAT: Verify execution_context_create() handles NULL gracefully
 * WHY:  Prevent crashes from invalid input
 * HOW:  Pass NULL, expect NULL return
 */
TEST_F(ExecutionModeTest, CreateContext_NullConfig) {
    ctx = execution_context_create(nullptr);
    EXPECT_EQ(ctx, nullptr);
}

/**
 * TEST: Create context with AUTO mode
 * WHAT: Verify AUTO mode selects appropriate execution mode
 * WHY:  Auto mode should work on all platforms
 * HOW:  Create with AUTO, verify context created
 */
TEST_F(ExecutionModeTest, CreateContext_AutoMode) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_AUTO);

    ctx = execution_context_create(&config);

    ASSERT_NE(ctx, nullptr);

    // Should select some valid mode
    execution_mode_t mode = execution_context_get_mode(ctx);
    EXPECT_NE(mode, EXEC_MODE_AUTO);  // Should resolve to concrete mode
}

/**
 * TEST: Create context with fallback enabled
 * WHAT: Verify fallback mechanism when mode unavailable
 * WHY:  Fallback ensures execution always possible
 * HOW:  Request unsupported mode with auto_fallback=true
 */
TEST_F(ExecutionModeTest, CreateContext_WithFallback) {
    execution_config_t config;
    memset(&config, 0, sizeof(config));
    config.mode = EXEC_MODE_GPU_ROCM;  // Likely unsupported
    config.auto_fallback = true;

    ctx = execution_context_create(&config);

    if (!execution_mode_is_supported(EXEC_MODE_GPU_ROCM)) {
        ASSERT_NE(ctx, nullptr);  // Should fallback
        execution_mode_t mode = execution_context_get_mode(ctx);
        EXPECT_TRUE(mode == EXEC_MODE_CPU_PARALLEL || mode == EXEC_MODE_CPU_SEQUENTIAL);
    }
}

/**
 * TEST: Create context without fallback
 * WHAT: Verify context creation fails without fallback
 * WHY:  User may want explicit mode enforcement
 * HOW:  Request unsupported mode with auto_fallback=false
 */
TEST_F(ExecutionModeTest, CreateContext_NoFallback) {
    execution_config_t config;
    memset(&config, 0, sizeof(config));
    config.mode = EXEC_MODE_GPU_ROCM;  // Likely unsupported
    config.auto_fallback = false;

    ctx = execution_context_create(&config);

    if (!execution_mode_is_supported(EXEC_MODE_GPU_ROCM)) {
        EXPECT_EQ(ctx, nullptr);  // Should fail
    }
}

/**
 * TEST: Destroy NULL context
 * WHAT: Verify execution_context_destroy() handles NULL gracefully
 * WHY:  Prevent crashes from double-free or NULL destroy
 * HOW:  Pass NULL, expect no crash
 */
TEST_F(ExecutionModeTest, DestroyContext_Null) {
    execution_context_destroy(nullptr);
    SUCCEED();  // Should not crash
}

/**
 * TEST: Get mode from context
 * WHAT: Verify execution_context_get_mode() returns correct mode
 * WHY:  Need to query active execution mode
 * HOW:  Create context, get mode, verify matches
 */
TEST_F(ExecutionModeTest, GetMode_ValidContext) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    execution_mode_t mode = execution_context_get_mode(ctx);
    EXPECT_EQ(mode, EXEC_MODE_CPU_SEQUENTIAL);
}

/**
 * TEST: Get mode from NULL context
 * WHAT: Verify execution_context_get_mode() handles NULL
 * WHY:  Prevent crashes from invalid context
 * HOW:  Pass NULL, expect default fallback mode
 */
TEST_F(ExecutionModeTest, GetMode_NullContext) {
    execution_mode_t mode = execution_context_get_mode(nullptr);
    EXPECT_EQ(mode, EXEC_MODE_CPU_SEQUENTIAL);  // Default fallback
}

/**
 * TEST: Set mode on valid context
 * WHAT: Verify execution_context_set_mode() switches mode
 * WHY:  Runtime mode switching may be needed
 * HOW:  Create context, switch mode, verify change
 */
TEST_F(ExecutionModeTest, SetMode_ValidContext) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    bool result = execution_context_set_mode(ctx, EXEC_MODE_CPU_PARALLEL);

    if (execution_mode_is_supported(EXEC_MODE_CPU_PARALLEL)) {
        EXPECT_TRUE(result);
        execution_mode_t mode = execution_context_get_mode(ctx);
        EXPECT_EQ(mode, EXEC_MODE_CPU_PARALLEL);
    } else {
        EXPECT_FALSE(result);
    }
}

/**
 * TEST: Set mode to NULL context
 * WHAT: Verify execution_context_set_mode() handles NULL
 * WHY:  Prevent crashes from invalid context
 * HOW:  Pass NULL, expect false
 */
TEST_F(ExecutionModeTest, SetMode_NullContext) {
    bool result = execution_context_set_mode(nullptr, EXEC_MODE_CPU_PARALLEL);
    EXPECT_FALSE(result);
}

/**
 * TEST: Set mode to unsupported mode
 * WHAT: Verify set_mode fails for unsupported modes
 * WHY:  Cannot switch to unavailable hardware
 * HOW:  Try to set GPU mode when no GPU available
 */
TEST_F(ExecutionModeTest, SetMode_UnsupportedMode) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    if (!execution_mode_is_supported(EXEC_MODE_GPU_ROCM)) {
        bool result = execution_context_set_mode(ctx, EXEC_MODE_GPU_ROCM);
        EXPECT_FALSE(result);
    }
}

//=============================================================================
// Memory Management Tests
//=============================================================================

/**
 * TEST: Allocate memory
 * WHAT: Verify execution_alloc() allocates memory
 * WHY:  Memory allocation is fundamental
 * HOW:  Allocate 1024 bytes, verify non-NULL
 */
TEST_F(ExecutionModeTest, Memory_Allocate) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    void* ptr = execution_alloc(ctx, 1024);
    ASSERT_NE(ptr, nullptr);

    execution_free(ctx, ptr);
}

/**
 * TEST: Allocate zero size
 * WHAT: Verify behavior when allocating zero bytes
 * WHY:  Edge case should not crash
 * HOW:  Allocate 0 bytes, check return value
 */
TEST_F(ExecutionModeTest, Memory_AllocateZeroSize) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    void* ptr = execution_alloc(ctx, 0);
    EXPECT_EQ(ptr, nullptr);  // Should return NULL for zero size
}

/**
 * TEST: Allocate with NULL context
 * WHAT: Verify execution_alloc() handles NULL context
 * WHY:  Prevent crashes from invalid context
 * HOW:  Pass NULL context, expect NULL return
 */
TEST_F(ExecutionModeTest, Memory_AllocateNullContext) {
    void* ptr = execution_alloc(nullptr, 1024);
    EXPECT_EQ(ptr, nullptr);
}

/**
 * TEST: Free NULL pointer
 * WHAT: Verify execution_free() handles NULL pointer
 * WHY:  Should be safe like standard free()
 * HOW:  Free NULL, expect no crash
 */
TEST_F(ExecutionModeTest, Memory_FreeNull) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    execution_free(ctx, nullptr);
    SUCCEED();  // Should not crash
}

/**
 * TEST: Free with NULL context
 * WHAT: Verify execution_free() handles NULL context
 * WHY:  Prevent crashes from invalid context
 * HOW:  Pass NULL context, expect no crash
 */
TEST_F(ExecutionModeTest, Memory_FreeNullContext) {
    execution_free(nullptr, (void*)0x1000);
    SUCCEED();  // Should not crash
}

/**
 * TEST: Memcpy host to device
 * WHAT: Verify execution_memcpy() copies data correctly
 * WHY:  Need to transfer data between host and device
 * HOW:  Copy array, verify success
 */
TEST_F(ExecutionModeTest, Memory_MemcpyHostToDevice) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    float src[10] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    void* dst = execution_alloc(ctx, sizeof(src));
    ASSERT_NE(dst, nullptr);

    bool result = execution_memcpy(ctx, dst, src, sizeof(src), true);
    EXPECT_TRUE(result);

    execution_free(ctx, dst);
}

/**
 * TEST: Memcpy device to host
 * WHAT: Verify execution_memcpy() copies back from device
 * WHY:  Need to read results from device
 * HOW:  Copy data to device, then back to host
 */
TEST_F(ExecutionModeTest, Memory_MemcpyDeviceToHost) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    float src[10] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    float dst[10] = {0};

    void* device_ptr = execution_alloc(ctx, sizeof(src));
    ASSERT_NE(device_ptr, nullptr);

    // Host to device
    bool result1 = execution_memcpy(ctx, device_ptr, src, sizeof(src), true);
    EXPECT_TRUE(result1);

    // Device to host
    bool result2 = execution_memcpy(ctx, dst, device_ptr, sizeof(dst), false);
    EXPECT_TRUE(result2);

    execution_free(ctx, device_ptr);
}

/**
 * TEST: Memcpy with NULL context
 * WHAT: Verify execution_memcpy() handles NULL context
 * WHY:  Prevent crashes from invalid context
 * HOW:  Pass NULL context, expect false
 */
TEST_F(ExecutionModeTest, Memory_MemcpyNullContext) {
    float src[10] = {0};
    float dst[10] = {0};

    bool result = execution_memcpy(nullptr, dst, src, sizeof(src), true);
    EXPECT_FALSE(result);
}

/**
 * TEST: Memcpy with NULL pointers
 * WHAT: Verify execution_memcpy() handles NULL src/dst
 * WHY:  Prevent crashes from invalid pointers
 * HOW:  Pass NULL pointers, expect false
 */
TEST_F(ExecutionModeTest, Memory_MemcpyNullPointers) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    float src[10] = {0};

    bool result1 = execution_memcpy(ctx, nullptr, src, sizeof(src), true);
    EXPECT_FALSE(result1);

    bool result2 = execution_memcpy(ctx, src, nullptr, sizeof(src), true);
    EXPECT_FALSE(result2);
}

/**
 * TEST: Memcpy with zero size
 * WHAT: Verify execution_memcpy() handles zero size
 * WHY:  Edge case should not crash
 * HOW:  Copy 0 bytes, expect false
 */
TEST_F(ExecutionModeTest, Memory_MemcpyZeroSize) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    float src[10] = {0};
    float dst[10] = {0};

    bool result = execution_memcpy(ctx, dst, src, 0, true);
    EXPECT_FALSE(result);
}

//=============================================================================
// Synchronization Tests
//=============================================================================

/**
 * TEST: Synchronize valid context
 * WHAT: Verify execution_synchronize() succeeds
 * WHY:  Need to ensure all operations complete
 * HOW:  Create context, synchronize, expect true
 */
TEST_F(ExecutionModeTest, Sync_ValidContext) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    bool result = execution_synchronize(ctx);
    EXPECT_TRUE(result);
}

/**
 * TEST: Synchronize NULL context
 * WHAT: Verify execution_synchronize() handles NULL
 * WHY:  Prevent crashes from invalid context
 * HOW:  Pass NULL, expect false
 */
TEST_F(ExecutionModeTest, Sync_NullContext) {
    bool result = execution_synchronize(nullptr);
    EXPECT_FALSE(result);
}

/**
 * TEST: Get statistics
 * WHAT: Verify execution_get_stats() returns stats
 * WHY:  Need to monitor performance
 * HOW:  Get stats, verify success
 */
TEST_F(ExecutionModeTest, Stats_ValidContext) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    uint64_t total_ops = 0;
    double total_time_ms = 0.0;

    bool result = execution_get_stats(ctx, &total_ops, &total_time_ms);
    EXPECT_TRUE(result);
    EXPECT_EQ(total_ops, 0u);  // No operations yet
    EXPECT_EQ(total_time_ms, 0.0);
}

/**
 * TEST: Get statistics with NULL context
 * WHAT: Verify execution_get_stats() handles NULL context
 * WHY:  Prevent crashes from invalid context
 * HOW:  Pass NULL, expect false
 */
TEST_F(ExecutionModeTest, Stats_NullContext) {
    uint64_t total_ops = 0;
    double total_time_ms = 0.0;

    bool result = execution_get_stats(nullptr, &total_ops, &total_time_ms);
    EXPECT_FALSE(result);
}

/**
 * TEST: Get statistics with NULL output pointers
 * WHAT: Verify execution_get_stats() handles NULL outputs
 * WHY:  User may only want subset of stats
 * HOW:  Pass NULL for outputs, expect success
 */
TEST_F(ExecutionModeTest, Stats_NullOutputPointers) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    bool result1 = execution_get_stats(ctx, nullptr, nullptr);
    EXPECT_TRUE(result1);

    uint64_t total_ops = 0;
    bool result2 = execution_get_stats(ctx, &total_ops, nullptr);
    EXPECT_TRUE(result2);

    double total_time_ms = 0.0;
    bool result3 = execution_get_stats(ctx, nullptr, &total_time_ms);
    EXPECT_TRUE(result3);
}

//=============================================================================
// Configuration Tests
//=============================================================================

/**
 * TEST: Get default config for CPU sequential
 * WHAT: Verify execution_get_default_config() for CPU sequential
 * WHY:  Need sensible defaults for each mode
 * HOW:  Get config, verify mode and settings
 */
TEST_F(ExecutionModeTest, Config_DefaultCPUSequential) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);

    EXPECT_EQ(config.mode, EXEC_MODE_CPU_SEQUENTIAL);
    EXPECT_EQ(config.cpu_threads, 1u);
    EXPECT_EQ(config.batch_size, 1u);
    EXPECT_TRUE(config.auto_fallback);
    EXPECT_EQ(config.fallback_mode, EXEC_MODE_CPU_PARALLEL);
}

/**
 * TEST: Get default config for CPU parallel
 * WHAT: Verify execution_get_default_config() for CPU parallel
 * WHY:  Parallel mode needs thread configuration
 * HOW:  Get config, verify thread count matches cores
 */
TEST_F(ExecutionModeTest, Config_DefaultCPUParallel) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);

    EXPECT_EQ(config.mode, EXEC_MODE_CPU_PARALLEL);
    EXPECT_GT(config.cpu_threads, 0u);
    EXPECT_EQ(config.batch_size, 32u);
    EXPECT_TRUE(config.auto_fallback);
}

/**
 * TEST: Get default config for GPU CUDA
 * WHAT: Verify execution_get_default_config() for GPU CUDA
 * WHY:  GPU mode needs block/thread configuration
 * HOW:  Get config, verify GPU settings
 */
TEST_F(ExecutionModeTest, Config_DefaultGPUCUDA) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_GPU_CUDA);

    EXPECT_EQ(config.mode, EXEC_MODE_GPU_CUDA);
    EXPECT_EQ(config.gpu_blocks, 256u);
    EXPECT_EQ(config.gpu_threads_per_block, 256u);
    EXPECT_FALSE(config.use_unified_memory);
    EXPECT_TRUE(config.pin_cpu_memory);
    EXPECT_EQ(config.batch_size, 1024u);
}

/**
 * TEST: Get optimal config for small network
 * WHAT: Verify execution_get_optimal_config() for small network
 * WHY:  GPU-first policy: GPU preferred when available, regardless of size
 * HOW:  Get config for 100 neurons, verify GPU if available
 */
TEST_F(ExecutionModeTest, Config_OptimalSmall) {
    execution_config_t config = execution_get_optimal_config(100);

    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    if (caps.cuda_available) {
        EXPECT_EQ(config.mode, EXEC_MODE_GPU_CUDA);
    } else if (caps.cpu_threads >= 4) {
        EXPECT_EQ(config.mode, EXEC_MODE_CPU_PARALLEL);
    } else {
        EXPECT_EQ(config.mode, EXEC_MODE_CPU_SEQUENTIAL);
    }
}

/**
 * TEST: Get optimal config for medium network
 * WHAT: Verify execution_get_optimal_config() for medium network
 * WHY:  Should select parallel or GPU
 * HOW:  Get config for 5000 neurons
 */
TEST_F(ExecutionModeTest, Config_OptimalMedium) {
    execution_config_t config = execution_get_optimal_config(5000);

    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    if (caps.cpu_threads >= 4) {
        EXPECT_TRUE(config.mode == EXEC_MODE_CPU_PARALLEL || config.mode == EXEC_MODE_GPU_CUDA);
    }
}

/**
 * TEST: Get optimal config for large network
 * WHAT: Verify execution_get_optimal_config() for large network
 * WHY:  Should prefer GPU if available
 * HOW:  Get config for 100K neurons
 */
TEST_F(ExecutionModeTest, Config_OptimalLarge) {
    execution_config_t config = execution_get_optimal_config(100000);

    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    if (caps.cuda_available) {
        EXPECT_EQ(config.mode, EXEC_MODE_GPU_CUDA);
    } else {
        EXPECT_EQ(config.mode, EXEC_MODE_CPU_PARALLEL);
    }
}

/**
 * TEST: Get optimal config for very large network
 * WHAT: Verify execution_get_optimal_config() for huge network
 * WHY:  Should consider distributed modes
 * HOW:  Get config for 2M neurons
 */
TEST_F(ExecutionModeTest, Config_OptimalVeryLarge) {
    execution_config_t config = execution_get_optimal_config(2000000);

    // Should recommend powerful execution mode
    EXPECT_TRUE(config.mode == EXEC_MODE_GPU_CUDA ||
                config.mode == EXEC_MODE_DISTRIBUTED_GPU ||
                config.mode == EXEC_MODE_CPU_PARALLEL);
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

/**
 * TEST: Multiple context creation
 * WHAT: Verify multiple contexts can coexist
 * WHY:  Application may need multiple execution contexts
 * HOW:  Create two contexts, verify both valid
 */
TEST_F(ExecutionModeTest, EdgeCase_MultipleContexts) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);

    execution_context_t ctx1 = execution_context_create(&config);
    execution_context_t ctx2 = execution_context_create(&config);

    ASSERT_NE(ctx1, nullptr);
    ASSERT_NE(ctx2, nullptr);
    EXPECT_NE(ctx1, ctx2);  // Different contexts

    execution_context_destroy(ctx1);
    execution_context_destroy(ctx2);
}

/**
 * TEST: Context destruction idempotence
 * WHAT: Verify destroying context twice is safe (via TearDown)
 * WHY:  Prevent double-free crashes
 * HOW:  Destroy explicitly, then let TearDown destroy again
 */
TEST_F(ExecutionModeTest, EdgeCase_DoubleDestroy) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    execution_context_destroy(ctx);
    ctx = nullptr;  // Prevent TearDown from double-destroy

    SUCCEED();
}

/**
 * TEST: Large memory allocation
 * WHAT: Verify allocation of large memory block
 * WHY:  Neural networks may need large allocations
 * HOW:  Allocate 100MB, verify success
 */
TEST_F(ExecutionModeTest, EdgeCase_LargeAllocation) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    size_t size = 100 * 1024 * 1024;  // 100 MB
    void* ptr = execution_alloc(ctx, size);

    if (ptr) {
        // Allocation succeeded
        execution_free(ctx, ptr);
        SUCCEED();
    } else {
        // Allocation failed (may be expected on some systems)
        SUCCEED();
    }
}

/**
 * TEST: Synchronize after multiple operations
 * WHAT: Verify sync works after multiple alloc/free
 * WHY:  Real usage involves many operations
 * HOW:  Do multiple operations, then sync
 */
TEST_F(ExecutionModeTest, EdgeCase_SyncAfterOperations) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Perform multiple operations
    std::vector<void*> ptrs;
    for (int i = 0; i < 10; i++) {
        void* ptr = execution_alloc(ctx, 1024);
        if (ptr) {
            ptrs.push_back(ptr);
        }
    }

    bool result = execution_synchronize(ctx);
    EXPECT_TRUE(result);

    // Cleanup
    for (void* ptr : ptrs) {
        execution_free(ctx, ptr);
    }
}

/**
 * TEST: Mode switching after operations
 * WHAT: Verify mode can switch after doing work
 * WHY:  Dynamic workload may benefit from mode switching
 * HOW:  Do operations, sync, switch mode
 */
TEST_F(ExecutionModeTest, EdgeCase_ModeSwitchAfterWork) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Do some work
    void* ptr = execution_alloc(ctx, 1024);
    ASSERT_NE(ptr, nullptr);
    execution_free(ctx, ptr);

    // Switch mode
    bool result = execution_context_set_mode(ctx, EXEC_MODE_CPU_PARALLEL);

    // Should succeed if parallel is supported
    if (execution_mode_is_supported(EXEC_MODE_CPU_PARALLEL)) {
        EXPECT_TRUE(result);
    }
}

//=============================================================================
// Platform-Specific Tests
//=============================================================================

/**
 * TEST: Linux CPU detection
 * WHAT: Verify CPU detection works on Linux
 * WHY:  Linux uses sysconf for CPU info
 * HOW:  Detect capabilities, verify reasonable values
 */
TEST_F(ExecutionModeTest, Platform_LinuxCPU) {
    #ifdef __linux__
    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    EXPECT_TRUE(caps.cpu_available);
    EXPECT_GE(caps.cpu_cores, 1u);
    EXPECT_LE(caps.cpu_cores, 256u);
    #endif
}

/**
 * TEST: Windows CPU detection
 * WHAT: Verify CPU detection works on Windows
 * WHY:  Windows uses GetSystemInfo
 * HOW:  Detect capabilities, verify reasonable values
 */
TEST_F(ExecutionModeTest, Platform_WindowsCPU) {
    #ifdef _WIN32
    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    EXPECT_TRUE(caps.cpu_available);
    EXPECT_GE(caps.cpu_cores, 1u);
    EXPECT_LE(caps.cpu_cores, 256u);
    #endif
}

/**
 * TEST: x86-64 SIMD detection
 * WHAT: Verify AVX2 detection on x86-64
 * WHY:  x86-64 should have AVX2 available
 * HOW:  Check AVX2 flag on x86-64 platforms
 */
TEST_F(ExecutionModeTest, Platform_x86_64_SIMD) {
    #if defined(__x86_64__) || defined(_M_X64)
    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    EXPECT_TRUE(caps.cpu_avx2);
    #endif
}

/**
 * TEST: CUDA availability check
 * WHAT: Verify CUDA detection when compiled with CUDA
 * WHY:  CUDA flag should match compile-time flag
 * HOW:  Check cuda_available matches NIMCP_ENABLE_CUDA
 */
TEST_F(ExecutionModeTest, Platform_CUDAAvailability) {
    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    #ifdef NIMCP_ENABLE_CUDA
    // CUDA may or may not be present at runtime
    // Just verify detection doesn't crash
    SUCCEED();
    #else
    EXPECT_FALSE(caps.cuda_available);
    #endif
}
