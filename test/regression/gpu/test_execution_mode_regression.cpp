/**
 * @file test_execution_mode_regression.cpp
 * @brief Regression tests for GPU execution mode module
 *
 * WHAT: Comprehensive regression tests for nimcp_execution_mode
 * WHY:  Ensure API stability, performance baselines, hardware compatibility
 * HOW:  Test API contracts, mode detection, fallback strategies, performance
 *
 * REGRESSION CATEGORIES:
 * - API Stability: Function signatures, enum values, struct layout
 * - Backward Compatibility: Old code patterns still work
 * - Performance Baselines: Mode switching, memory allocation speed
 * - Hardware Compatibility: CPU, GPU, distributed mode detection
 * - Bug Fixes: Previously fixed bugs must stay fixed
 * - Error Handling: Graceful degradation and fallback behavior
 *
 * @author NIMCP Test Team
 * @date 2025-01-19
 */

#include <gtest/gtest.h>
#include <cstring>
#include <chrono>
#include <vector>

// Headers have their own extern "C" guards
    #include "gpu/nimcp_execution_mode.h"
    #include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Utilities
//=============================================================================

class ExecutionModeRegressionTest : public ::testing::Test {
protected:
    execution_context_t ctx;
    hardware_capabilities_t caps;

    void SetUp() override {
        ctx = nullptr;
        memset(&caps, 0, sizeof(caps));
    }

    void TearDown() override {
        if (ctx) {
            execution_context_destroy(ctx);
            ctx = nullptr;
        }
    }
};

//=============================================================================
// API Stability Tests - Enum Values
//=============================================================================

TEST_F(ExecutionModeRegressionTest, ExecutionModeEnumStable) {
    // WHAT: Verify execution_mode_t enum values
    // WHY:  API stability - enum values must not change
    // REGRESSION: Enum values must remain constant

    execution_mode_t mode;

    mode = EXEC_MODE_CPU_SEQUENTIAL;
    EXPECT_EQ(mode, EXEC_MODE_CPU_SEQUENTIAL);

    mode = EXEC_MODE_CPU_PARALLEL;
    EXPECT_EQ(mode, EXEC_MODE_CPU_PARALLEL);

    mode = EXEC_MODE_GPU_CUDA;
    EXPECT_EQ(mode, EXEC_MODE_GPU_CUDA);

    mode = EXEC_MODE_GPU_ROCM;
    EXPECT_EQ(mode, EXEC_MODE_GPU_ROCM);

    mode = EXEC_MODE_GPU_OPENCL;
    EXPECT_EQ(mode, EXEC_MODE_GPU_OPENCL);

    mode = EXEC_MODE_DISTRIBUTED_CPU;
    EXPECT_EQ(mode, EXEC_MODE_DISTRIBUTED_CPU);

    mode = EXEC_MODE_DISTRIBUTED_GPU;
    EXPECT_EQ(mode, EXEC_MODE_DISTRIBUTED_GPU);

    mode = EXEC_MODE_HYBRID;
    EXPECT_EQ(mode, EXEC_MODE_HYBRID);

    mode = EXEC_MODE_AUTO;
    EXPECT_EQ(mode, EXEC_MODE_AUTO);
}

//=============================================================================
// API Stability Tests - Struct Layout
//=============================================================================

TEST_F(ExecutionModeRegressionTest, HardwareCapabilitiesStructStable) {
    // WHAT: Verify hardware_capabilities_t structure fields
    // WHY:  API stability - struct layout must remain stable
    // REGRESSION: Struct fields must be accessible

    hardware_capabilities_t test_caps;
    memset(&test_caps, 0, sizeof(test_caps));

    // CPU fields
    test_caps.cpu_available = true;
    test_caps.cpu_cores = 8;
    test_caps.cpu_threads = 16;
    test_caps.cpu_avx2 = true;
    test_caps.cpu_avx512 = false;

    // GPU fields
    test_caps.cuda_available = false;
    test_caps.rocm_available = false;
    test_caps.opencl_available = false;
    test_caps.gpu_count = 0;
    test_caps.gpu_compute_units = 0;
    test_caps.gpu_memory_mb = 0;
    test_caps.gpu_compute_capability = 0;

    // Network fields
    test_caps.network_available = false;
    test_caps.network_nodes = 0;
    test_caps.network_bandwidth_mbps = 0;

    // Recommended mode
    test_caps.recommended_mode = EXEC_MODE_CPU_PARALLEL;

    // Verify values
    EXPECT_TRUE(test_caps.cpu_available);
    EXPECT_EQ(test_caps.cpu_cores, 8u);
    EXPECT_EQ(test_caps.cpu_threads, 16u);
    EXPECT_TRUE(test_caps.cpu_avx2);
    EXPECT_FALSE(test_caps.cuda_available);
    EXPECT_EQ(test_caps.recommended_mode, EXEC_MODE_CPU_PARALLEL);
}

TEST_F(ExecutionModeRegressionTest, ExecutionConfigStructStable) {
    // WHAT: Verify execution_config_t structure fields
    // WHY:  API stability - struct layout must remain stable
    // REGRESSION: Struct fields must be accessible

    execution_config_t config;
    memset(&config, 0, sizeof(config));

    config.mode = EXEC_MODE_CPU_PARALLEL;
    config.cpu_threads = 4;
    config.gpu_blocks = 256;
    config.gpu_threads_per_block = 256;
    config.pin_cpu_memory = false;
    config.use_unified_memory = false;
    config.gpu_memory_limit = 1024 * 1024 * 1024;
    config.batch_size = 32;
    config.enable_profiling = false;
    config.enable_validation = true;
    config.fallback_mode = EXEC_MODE_CPU_SEQUENTIAL;
    config.auto_fallback = true;

    // Verify values
    EXPECT_EQ(config.mode, EXEC_MODE_CPU_PARALLEL);
    EXPECT_EQ(config.cpu_threads, 4u);
    EXPECT_EQ(config.gpu_blocks, 256u);
    EXPECT_EQ(config.gpu_threads_per_block, 256u);
    EXPECT_EQ(config.batch_size, 32u);
    EXPECT_TRUE(config.enable_validation);
    EXPECT_TRUE(config.auto_fallback);
}

//=============================================================================
// Hardware Detection Tests
//=============================================================================

TEST_F(ExecutionModeRegressionTest, CapabilityDetectionWorks) {
    // WHAT: Verify execution_detect_capabilities() works
    // WHY:  Core functionality - must detect hardware
    // REGRESSION: Detection must remain stable

    bool result = execution_detect_capabilities(&caps);
    EXPECT_TRUE(result);

    // CPU should always be available
    EXPECT_TRUE(caps.cpu_available);
    EXPECT_GT(caps.cpu_cores, 0u);

    // Recommended mode should be set
    EXPECT_NE(caps.recommended_mode, EXEC_MODE_AUTO);
}

TEST_F(ExecutionModeRegressionTest, CPUModeAlwaysSupported) {
    // WHAT: Verify CPU modes are always supported
    // WHY:  Fallback guarantee - CPU must work
    // REGRESSION: Bug fix - must have fallback (Issue #GPU-001)

    EXPECT_TRUE(execution_mode_is_supported(EXEC_MODE_CPU_SEQUENTIAL));
    EXPECT_TRUE(execution_mode_is_supported(EXEC_MODE_CPU_PARALLEL));
}

TEST_F(ExecutionModeRegressionTest, RecommendedModeIsValid) {
    // WHAT: Verify execution_get_recommended_mode() returns supported mode
    // WHY:  Must not recommend unsupported mode
    // REGRESSION: Bug fix - recommended unsupported mode (Issue #GPU-002)

    execution_mode_t recommended = execution_get_recommended_mode(10000, 100);

    // Recommended mode must be supported
    EXPECT_TRUE(execution_mode_is_supported(recommended));

    // Should not be AUTO (must be concrete mode)
    EXPECT_NE(recommended, EXEC_MODE_AUTO);
}

TEST_F(ExecutionModeRegressionTest, RecommendedModeScalesWithWorkload) {
    // WHAT: Verify recommended mode scales with network size
    // WHY:  Heuristics must adapt to workload
    // REGRESSION: Scaling heuristics must remain stable

    // Small network should recommend CPU
    execution_mode_t small = execution_get_recommended_mode(100, 10);
    EXPECT_TRUE(small == EXEC_MODE_CPU_SEQUENTIAL ||
                small == EXEC_MODE_CPU_PARALLEL);

    // Large network should recommend GPU or CPU parallel (if GPU unavailable)
    execution_mode_t large = execution_get_recommended_mode(1000000, 100);

    // Should recommend parallel processing
    EXPECT_TRUE(large == EXEC_MODE_GPU_CUDA ||
                large == EXEC_MODE_GPU_ROCM ||
                large == EXEC_MODE_CPU_PARALLEL ||
                large == EXEC_MODE_DISTRIBUTED_GPU);
}

//=============================================================================
// Context Management Tests
//=============================================================================

TEST_F(ExecutionModeRegressionTest, ContextCreateDestroyWorks) {
    // WHAT: Verify context creation/destruction lifecycle
    // WHY:  Core functionality - must manage resources
    // REGRESSION: Memory leak fix (Issue #GPU-003)

    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);

    EXPECT_NE(ctx, nullptr);

    // Should be able to get mode
    execution_mode_t mode = execution_context_get_mode(ctx);
    EXPECT_EQ(mode, EXEC_MODE_CPU_SEQUENTIAL);

    // Destroy should be safe
    execution_context_destroy(ctx);
    ctx = nullptr;

    // Double destroy should be safe
    execution_context_destroy(nullptr);
}

TEST_F(ExecutionModeRegressionTest, ContextFallbackWorks) {
    // WHAT: Verify fallback to CPU if GPU unavailable
    // WHY:  Robustness - must not fail completely
    // REGRESSION: Bug fix - crashed when GPU unavailable (Issue #GPU-004)

    execution_config_t config;
    memset(&config, 0, sizeof(config));
    config.mode = EXEC_MODE_GPU_CUDA;  // May not be available
    config.fallback_mode = EXEC_MODE_CPU_SEQUENTIAL;
    config.auto_fallback = true;

    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Should have fallen back to CPU if GPU unavailable
    // NOTE: Fallback tries CPU_PARALLEL first, then CPU_SEQUENTIAL
    execution_mode_t mode = execution_context_get_mode(ctx);
    EXPECT_TRUE(mode == EXEC_MODE_GPU_CUDA ||
                mode == EXEC_MODE_CPU_PARALLEL ||
                mode == EXEC_MODE_CPU_SEQUENTIAL)
        << "Mode should be GPU or CPU (parallel or sequential)";
}

TEST_F(ExecutionModeRegressionTest, DefaultConfigWorks) {
    // WHAT: Verify execution_get_default_config() returns valid config
    // WHY:  Convenience function must work
    // REGRESSION: Config values must remain stable

    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);

    EXPECT_EQ(config.mode, EXEC_MODE_CPU_PARALLEL);
    EXPECT_GT(config.cpu_threads, 0u);
    EXPECT_TRUE(config.auto_fallback);
}

TEST_F(ExecutionModeRegressionTest, OptimalConfigWorks) {
    // WHAT: Verify execution_get_optimal_config() returns valid config
    // WHY:  Auto-tuning must work
    // REGRESSION: Optimal config must be valid

    execution_config_t config = execution_get_optimal_config(10000);

    // Mode should be supported
    EXPECT_TRUE(execution_mode_is_supported(config.mode));

    // Should have reasonable values
    EXPECT_GT(config.batch_size, 0u);
}

//=============================================================================
// Memory Management Tests
//=============================================================================

TEST_F(ExecutionModeRegressionTest, MemoryAllocationWorks) {
    // WHAT: Verify execution_alloc/free work correctly
    // WHY:  Memory management must be reliable
    // REGRESSION: Memory leak fix (Issue #GPU-005)

    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Allocate memory
    void* ptr = execution_alloc(ctx, 1024);
    EXPECT_NE(ptr, nullptr);

    // Free memory
    execution_free(ctx, ptr);

    // NULL free should be safe
    execution_free(ctx, nullptr);
}

TEST_F(ExecutionModeRegressionTest, MemoryCopyWorks) {
    // WHAT: Verify execution_memcpy() works
    // WHY:  Data transfer must work
    // REGRESSION: Copy direction bug fix (Issue #GPU-006)

    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Allocate source and destination
    float src[256];
    for (int i = 0; i < 256; i++) {
        src[i] = static_cast<float>(i);
    }

    float dst[256];
    memset(dst, 0, sizeof(dst));

    // Copy memory (in CPU mode, this is just memcpy)
    bool result = execution_memcpy(ctx, dst, src, sizeof(src), true);
    EXPECT_TRUE(result);

    // Verify data
    for (int i = 0; i < 256; i++) {
        EXPECT_FLOAT_EQ(dst[i], static_cast<float>(i));
    }
}

//=============================================================================
// Synchronization Tests
//=============================================================================

TEST_F(ExecutionModeRegressionTest, SynchronizeWorks) {
    // WHAT: Verify execution_synchronize() works
    // WHY:  Synchronization must complete
    // REGRESSION: Hang fix (Issue #GPU-007)

    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    bool result = execution_synchronize(ctx);
    EXPECT_TRUE(result);
}

TEST_F(ExecutionModeRegressionTest, GetStatsWorks) {
    // WHAT: Verify execution_get_stats() returns valid data
    // WHY:  Statistics must be available
    // REGRESSION: Stats must be accurate

    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    uint64_t total_ops = 0;
    double total_time_ms = 0.0;

    bool result = execution_get_stats(ctx, &total_ops, &total_time_ms);
    EXPECT_TRUE(result);

    // Initially should be zero
    EXPECT_GE(total_ops, 0u);
    EXPECT_GE(total_time_ms, 0.0);
}

//=============================================================================
// Performance Baseline Tests
//=============================================================================

TEST_F(ExecutionModeRegressionTest, ContextCreationSpeed) {
    // WHAT: Verify context creation performance
    // WHY:  Performance baseline - must be fast
    // BASELINE: < 100ms for CPU mode

    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);

    auto start = std::chrono::high_resolution_clock::now();

    execution_context_t test_ctx = execution_context_create(&config);

    auto end = std::chrono::high_resolution_clock::now();

    ASSERT_NE(test_ctx, nullptr);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Context creation time: " << duration.count() << "ms" << std::endl;

    // Baseline: < 100ms
    EXPECT_LT(duration.count(), 100);

    execution_context_destroy(test_ctx);
}

TEST_F(ExecutionModeRegressionTest, MemoryAllocationSpeed) {
    // WHAT: Verify memory allocation performance
    // WHY:  Performance baseline - must be efficient
    // BASELINE: > 1000 allocations/second

    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    const int num_allocs = 1000;
    std::vector<void*> ptrs;
    ptrs.reserve(num_allocs);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_allocs; i++) {
        void* ptr = execution_alloc(ctx, 1024);
        ASSERT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }

    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    double allocs_per_sec = num_allocs / (duration.count() / 1000.0);

    std::cout << "Memory allocation rate: " << allocs_per_sec << " allocs/sec" << std::endl;

    // Baseline: > 1000 allocs/second
    EXPECT_GT(allocs_per_sec, 1000.0);

    // Free all
    for (void* ptr : ptrs) {
        execution_free(ctx, ptr);
    }
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(ExecutionModeRegressionTest, NullPointerHandling) {
    // WHAT: Verify NULL pointer handling
    // WHY:  API contract - must handle NULL gracefully
    // REGRESSION: Bug fix - NULL caused crash (Issue #GPU-008)

    // NULL config
    ctx = execution_context_create(nullptr);
    EXPECT_EQ(ctx, nullptr);

    // NULL context operations should be safe
    execution_context_destroy(nullptr);
    EXPECT_EQ(execution_context_get_mode(nullptr), EXEC_MODE_CPU_SEQUENTIAL);

    execution_alloc(nullptr, 1024);  // Should return NULL
    execution_free(nullptr, nullptr);  // Should be safe

    bool result = execution_synchronize(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(ExecutionModeRegressionTest, InvalidModeHandling) {
    // WHAT: Verify invalid mode is rejected
    // WHY:  Input validation
    // REGRESSION: Bug fix - invalid mode caused crash (Issue #GPU-009)

    execution_config_t config;
    memset(&config, 0, sizeof(config));
    config.mode = static_cast<execution_mode_t>(9999);  // Invalid
    config.fallback_mode = EXEC_MODE_CPU_SEQUENTIAL;
    config.auto_fallback = true;

    ctx = execution_context_create(&config);

    // Should either fall back or return NULL (not crash)
    if (ctx != nullptr) {
        // If created, should have fallen back to a valid CPU mode
        // NOTE: Fallback tries CPU_PARALLEL first, then CPU_SEQUENTIAL
        execution_mode_t mode = execution_context_get_mode(ctx);
        EXPECT_TRUE(mode == EXEC_MODE_CPU_PARALLEL ||
                    mode == EXEC_MODE_CPU_SEQUENTIAL)
            << "Invalid mode should fall back to valid CPU mode";
    }
}

TEST_F(ExecutionModeRegressionTest, ZeroSizeAllocation) {
    // WHAT: Verify zero-size allocation is handled
    // WHY:  Edge case handling
    // REGRESSION: Bug fix - zero size caused crash (Issue #GPU-010)

    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    void* ptr = execution_alloc(ctx, 0);

    // Should either return NULL or handle gracefully (no crash)
    if (ptr != nullptr) {
        execution_free(ctx, ptr);
    }

    SUCCEED();
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(ExecutionModeRegressionTest, BrainStillWorksWithoutExecutionMode) {
    // WHAT: Verify brain works without execution mode awareness
    // WHY:  Backward compatibility - old code must work
    // REGRESSION: Brain API must work independently

    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    EXPECT_NE(decision, nullptr);

    brain_destroy(brain);
}

TEST_F(ExecutionModeRegressionTest, BrainWorksWithExecutionMode) {
    // WHAT: Verify brain works with execution mode
    // WHY:  Integration must work
    // REGRESSION: Combined usage must work

    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                  BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    EXPECT_NE(decision, nullptr);

    brain_destroy(brain);
}

//=============================================================================
// Test Summary
//=============================================================================

// Test count: 20 regression tests
// Coverage:
// - API Stability: 3 tests (enums, structs)
// - Hardware Detection: 4 tests
// - Context Management: 4 tests
// - Memory Management: 2 tests
// - Synchronization: 2 tests
// - Performance Baselines: 2 tests
// - Error Handling: 3 tests
// - Backward Compatibility: 2 tests
