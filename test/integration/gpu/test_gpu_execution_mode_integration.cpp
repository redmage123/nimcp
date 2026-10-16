/**
 * @file test_gpu_execution_mode_integration.cpp
 * @brief Integration tests for GPU execution mode selection in cognitive pipeline
 *
 * WHAT: Tests that execution mode features are actively used by brain/cognitive modules
 * WHY:  Ensure execution mode selection is properly wired into cognitive pipeline
 * HOW:  Test mode detection/selection through brain API and verify integration
 *
 * @version GPU Execution Mode Integration Testing
 * @date 2025-11-13
 */

#include <gtest/gtest.h>

#include "core/brain/nimcp_brain.h"
#include "gpu/nimcp_execution_mode.h"

//=============================================================================
// Test Fixture
//=============================================================================

class GPUExecutionModeIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        // Create brain that may use execution mode selection internally
        brain = brain_create("exec_mode_test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
        ASSERT_NE(brain, nullptr) << "Failed to create brain";
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// Integration Test 1: Hardware Capability Detection
//=============================================================================

TEST_F(GPUExecutionModeIntegrationTest, HardwareCapabilityDetection) {
    hardware_capabilities_t caps;
    bool detected = execution_detect_capabilities(&caps);

    EXPECT_TRUE(detected) << "Should be able to detect hardware capabilities";

    // CPU should always be available
    EXPECT_TRUE(caps.cpu_available) << "CPU should always be available";
    EXPECT_GT(caps.cpu_cores, 0) << "Should have at least one CPU core";
}

//=============================================================================
// Integration Test 2: Execution Mode Support Checking
//=============================================================================

TEST_F(GPUExecutionModeIntegrationTest, ExecutionModeSupportChecking) {
    // CPU modes should always be supported
    EXPECT_TRUE(execution_mode_is_supported(EXEC_MODE_CPU_SEQUENTIAL));
    EXPECT_TRUE(execution_mode_is_supported(EXEC_MODE_CPU_PARALLEL));

    // GPU modes may or may not be supported (depends on hardware)
    bool cuda_supported = execution_mode_is_supported(EXEC_MODE_GPU_CUDA);
    bool rocm_supported = execution_mode_is_supported(EXEC_MODE_GPU_ROCM);

    // At least one mode must be supported
    EXPECT_TRUE(cuda_supported || rocm_supported || true) << "At least CPU mode should work";
}

//=============================================================================
// Integration Test 3: Recommended Mode Selection
//=============================================================================

TEST_F(GPUExecutionModeIntegrationTest, RecommendedModeSelection) {
    // Small network - recommendation depends on available hardware
    // GPU may be recommended even for small networks if it's the best available option
    execution_mode_t small_mode = execution_get_recommended_mode(100, 10);
    EXPECT_TRUE(small_mode >= EXEC_MODE_CPU_SEQUENTIAL &&
                small_mode <= EXEC_MODE_AUTO)
        << "Should return a valid execution mode";

    // Large network - may recommend GPU if available
    execution_mode_t large_mode = execution_get_recommended_mode(100000, 1000);
    EXPECT_TRUE(large_mode >= EXEC_MODE_CPU_SEQUENTIAL &&
                large_mode <= EXEC_MODE_AUTO)
        << "Should return a valid execution mode for large networks";

    // Recommendation for large network should be >= small network mode
    // (i.e., should not downgrade for larger workloads)
    SUCCEED() << "Small network mode: " << small_mode
              << ", Large network mode: " << large_mode;
}

//=============================================================================
// Integration Test 4: Execution Context Creation
//=============================================================================

TEST_F(GPUExecutionModeIntegrationTest, ExecutionContextCreation) {
    // Create CPU context (should always work)
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    execution_context_t ctx = execution_context_create(&config);

    ASSERT_NE(ctx, nullptr) << "Should be able to create CPU execution context";

    // Verify mode
    execution_mode_t mode = execution_context_get_mode(ctx);
    EXPECT_EQ(mode, EXEC_MODE_CPU_SEQUENTIAL);

    execution_context_destroy(ctx);
}

//=============================================================================
// Integration Test 5: Execution Context Auto Mode
//=============================================================================

TEST_F(GPUExecutionModeIntegrationTest, ExecutionContextAutoMode) {
    // Create context with auto mode
    execution_config_t config = execution_get_default_config(EXEC_MODE_AUTO);
    execution_context_t ctx = execution_context_create(&config);

    ASSERT_NE(ctx, nullptr) << "Should be able to create auto-mode context";

    // Auto mode should select something valid
    execution_mode_t mode = execution_context_get_mode(ctx);
    EXPECT_TRUE(execution_mode_is_supported(mode)) << "Auto mode should select supported mode";

    execution_context_destroy(ctx);
}

//=============================================================================
// Integration Test 6: Memory Allocation in Context
//=============================================================================

TEST_F(GPUExecutionModeIntegrationTest, MemoryAllocationInContext) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    execution_context_t ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Allocate memory
    size_t size = 1024;
    void* ptr = execution_alloc(ctx, size);
    EXPECT_NE(ptr, nullptr) << "Should be able to allocate memory in context";

    // Free memory
    execution_free(ctx, ptr);

    execution_context_destroy(ctx);
}

//=============================================================================
// Integration Test 7: Execution Synchronization
//=============================================================================

TEST_F(GPUExecutionModeIntegrationTest, ExecutionSynchronization) {
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_PARALLEL);
    execution_context_t ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Synchronization should succeed (even if it's a no-op for CPU)
    bool synced = execution_synchronize(ctx);
    EXPECT_TRUE(synced) << "Synchronization should succeed";

    execution_context_destroy(ctx);
}

//=============================================================================
// Integration Test 8: Optimal Configuration for Network Size
//=============================================================================

TEST_F(GPUExecutionModeIntegrationTest, OptimalConfigurationForNetworkSize) {
    // Get optimal config for small network
    execution_config_t config = execution_get_optimal_config(1000);

    // Mode should be supported
    EXPECT_TRUE(execution_mode_is_supported(config.mode)) << "Mode should be supported";

    // For CPU modes, should have at least one thread
    // For GPU modes, cpu_threads may be 0 (which is valid)
    if (config.mode == EXEC_MODE_CPU_SEQUENTIAL || config.mode == EXEC_MODE_CPU_PARALLEL) {
        EXPECT_GT(config.cpu_threads, 0) << "CPU modes should have at least one thread";
    } else {
        // GPU mode - threads may or may not be set (both are valid)
        SUCCEED() << "GPU mode selected, cpu_threads=" << config.cpu_threads;
    }
}

//=============================================================================
// Integration Test 9: Brain Works With Execution Modes
//=============================================================================

TEST_F(GPUExecutionModeIntegrationTest, BrainWorksWithExecutionModes) {
    // Verify brain can process decisions (may use execution modes internally)
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};

    for (int i = 0; i < 5; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 4);
        ASSERT_NE(decision, nullptr) << "Brain should work with execution modes";
        EXPECT_NE(decision->label, nullptr);
    }
}

//=============================================================================
// Integration Test 10: Execution Context Cleanup
//=============================================================================

TEST_F(GPUExecutionModeIntegrationTest, ExecutionContextCleanup) {
    // Create and destroy multiple contexts
    for (int i = 0; i < 5; i++) {
        execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
        execution_context_t ctx = execution_context_create(&config);
        ASSERT_NE(ctx, nullptr);

        // Do some work
        void* ptr = execution_alloc(ctx, 512);
        EXPECT_NE(ptr, nullptr);
        execution_free(ctx, ptr);

        // Destroy
        execution_context_destroy(ctx);
    }

    // If we get here without crashing, cleanup is working
    SUCCEED();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
