#include <gtest/gtest.h>

#include "gpu/nimcp_execution_mode.h"

//=============================================================================
// Execution Mode Real Tests
//=============================================================================

class ExecutionModeRealTest : public ::testing::Test {
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

TEST_F(ExecutionModeRealTest, DetectCapabilities) {
    hardware_capabilities_t caps;
    memset(&caps, 0, sizeof(caps));

    bool result = execution_detect_capabilities(&caps);
    EXPECT_TRUE(result);

    // CPU should always be available
    EXPECT_TRUE(caps.cpu_available);
    EXPECT_GT(caps.cpu_cores, 0);
}

TEST_F(ExecutionModeRealTest, DetectCapabilitiesNull) {
    bool result = execution_detect_capabilities(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(ExecutionModeRealTest, CheckCPUModeSupported) {
    bool supported = execution_mode_is_supported(EXEC_MODE_CPU_SEQUENTIAL);
    EXPECT_TRUE(supported);
}

TEST_F(ExecutionModeRealTest, CheckCPUParallelSupported) {
    bool supported = execution_mode_is_supported(EXEC_MODE_CPU_PARALLEL);
    EXPECT_TRUE(supported);
}

TEST_F(ExecutionModeRealTest, CheckAutoModeSupported) {
    bool supported = execution_mode_is_supported(EXEC_MODE_AUTO);
    EXPECT_TRUE(supported);
}

//=============================================================================
// Execution Mode Recommendation Tests
//=============================================================================

TEST_F(ExecutionModeRealTest, RecommendModeSmallNetwork) {
    execution_mode_t mode = execution_get_recommended_mode(100, 10);
    // Small networks should recommend CPU
    EXPECT_TRUE(mode == EXEC_MODE_CPU_SEQUENTIAL || mode == EXEC_MODE_CPU_PARALLEL);
}

TEST_F(ExecutionModeRealTest, RecommendModeMediumNetwork) {
    execution_mode_t mode = execution_get_recommended_mode(5000, 100);
    // Should recommend some execution mode
    EXPECT_GE(mode, EXEC_MODE_CPU_SEQUENTIAL);
    EXPECT_LE(mode, EXEC_MODE_AUTO);
}

TEST_F(ExecutionModeRealTest, RecommendModeLargeNetwork) {
    execution_mode_t mode = execution_get_recommended_mode(100000, 1000);
    // Should recommend some execution mode
    EXPECT_GE(mode, EXEC_MODE_CPU_SEQUENTIAL);
    EXPECT_LE(mode, EXEC_MODE_AUTO);
}

//=============================================================================
// Execution Context Tests
//=============================================================================

TEST_F(ExecutionModeRealTest, CreateContextCPU) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);

    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    execution_mode_t mode = execution_context_get_mode(ctx);
    EXPECT_EQ(mode, EXEC_MODE_CPU_SEQUENTIAL);
}

TEST_F(ExecutionModeRealTest, CreateContextCPUParallel) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);

    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    execution_mode_t mode = execution_context_get_mode(ctx);
    EXPECT_EQ(mode, EXEC_MODE_CPU_PARALLEL);
}

TEST_F(ExecutionModeRealTest, CreateContextNull) {
    ctx = execution_context_create(nullptr);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(ExecutionModeRealTest, DestroyContextNull) {
    // Should not crash
    execution_context_destroy(nullptr);
    SUCCEED();
}

TEST_F(ExecutionModeRealTest, GetModeNull) {
    execution_mode_t mode = execution_context_get_mode(nullptr);
    EXPECT_EQ(mode, EXEC_MODE_CPU_SEQUENTIAL); // Default fallback
}

//=============================================================================
// Memory Management Tests
//=============================================================================

TEST_F(ExecutionModeRealTest, AllocateMemory) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    void* ptr = execution_alloc(ctx, 1024);
    ASSERT_NE(ptr, nullptr);

    execution_free(ctx, ptr);
}

TEST_F(ExecutionModeRealTest, AllocateZeroSize) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    void* ptr = execution_alloc(ctx, 0);
    // Implementation may return NULL or valid pointer for zero size
    if (ptr) {
        execution_free(ctx, ptr);
    }
    SUCCEED();
}

TEST_F(ExecutionModeRealTest, FreeNull) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Should not crash
    execution_free(ctx, nullptr);
    SUCCEED();
}

TEST_F(ExecutionModeRealTest, MemcpyHostToDevice) {
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

//=============================================================================
// Synchronization Tests
//=============================================================================

TEST_F(ExecutionModeRealTest, Synchronize) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    bool result = execution_synchronize(ctx);
    EXPECT_TRUE(result);
}

TEST_F(ExecutionModeRealTest, SynchronizeNull) {
    bool result = execution_synchronize(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(ExecutionModeRealTest, GetStats) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    uint64_t total_ops = 0;
    double total_time_ms = 0.0;

    bool result = execution_get_stats(ctx, &total_ops, &total_time_ms);
    EXPECT_TRUE(result);
}

TEST_F(ExecutionModeRealTest, GetStatsNull) {
    uint64_t total_ops = 0;
    double total_time_ms = 0.0;

    bool result = execution_get_stats(nullptr, &total_ops, &total_time_ms);
    EXPECT_FALSE(result);
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(ExecutionModeRealTest, GetDefaultConfigCPU) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);

    EXPECT_EQ(config.mode, EXEC_MODE_CPU_SEQUENTIAL);
    EXPECT_GT(config.cpu_threads, 0);
}

TEST_F(ExecutionModeRealTest, GetDefaultConfigParallel) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);

    EXPECT_EQ(config.mode, EXEC_MODE_CPU_PARALLEL);
    EXPECT_GT(config.cpu_threads, 0);
}

TEST_F(ExecutionModeRealTest, GetOptimalConfigSmall) {
    execution_config_t config = execution_get_optimal_config(100);

    EXPECT_GE(config.mode, EXEC_MODE_CPU_SEQUENTIAL);
    EXPECT_LE(config.mode, EXEC_MODE_AUTO);
}

TEST_F(ExecutionModeRealTest, GetOptimalConfigLarge) {
    execution_config_t config = execution_get_optimal_config(100000);

    EXPECT_GE(config.mode, EXEC_MODE_CPU_SEQUENTIAL);
    EXPECT_LE(config.mode, EXEC_MODE_AUTO);
}

//=============================================================================
// Mode Switching Tests
//=============================================================================

TEST_F(ExecutionModeRealTest, SwitchMode) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Try to switch to parallel (may fail if not supported)
    bool result = execution_context_set_mode(ctx, EXEC_MODE_CPU_PARALLEL);
    // Just verify it doesn't crash
    SUCCEED();
}

TEST_F(ExecutionModeRealTest, SwitchModeNull) {
    bool result = execution_context_set_mode(nullptr, EXEC_MODE_CPU_PARALLEL);
    EXPECT_FALSE(result);
}
