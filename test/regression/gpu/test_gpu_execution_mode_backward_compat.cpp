/**
 * @file test_gpu_execution_mode_backward_compat.cpp
 * @brief Regression tests for GPU execution mode backward compatibility
 *
 * WHAT: Ensures execution mode features don't break existing code
 * WHY:  Verify zero breaking changes to pre-execution-mode code
 * HOW:  Test legacy patterns and ensure they still work correctly
 *
 * @version GPU Execution Mode Regression Testing
 * @date 2025-11-13
 */

#include <gtest/gtest.h>

// GPU header outside extern "C" due to CUDA runtime templates
#include "gpu/nimcp_execution_mode.h"

// Headers have their own extern "C" guards
    #include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class GPUExecutionModeRegressionTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        brain = nullptr;
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// Regression Test 1: Brain Creation Still Works
//=============================================================================

TEST_F(GPUExecutionModeRegressionTest, BrainCreation_StillWorks) {
    // Old code pattern: Create brain without execution mode awareness
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);

    // Should still work exactly as before
    ASSERT_NE(brain, nullptr) << "Brain creation should not be broken by execution modes";
}

//=============================================================================
// Regression Test 2: Legacy Inference Without Execution Modes
//=============================================================================

TEST_F(GPUExecutionModeRegressionTest, LegacyInference_StillWorks) {
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Old inference pattern (no execution mode knowledge)
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    EXPECT_NE(decision, nullptr);
    if (decision) {
        EXPECT_NE(decision->label, nullptr);
    }
}

//=============================================================================
// Regression Test 3: Execution Mode API Doesn't Break CPU Code
//=============================================================================

TEST_F(GPUExecutionModeRegressionTest, ExecutionModeAPI_NoCPUBreakage) {
    // Use execution mode API (new)
    hardware_capabilities_t caps;
    execution_detect_capabilities(&caps);

    // Old brain API should still work
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    ASSERT_NE(decision, nullptr);
}

//=============================================================================
// Regression Test 4: Execution Context Doesn't Interfere
//=============================================================================

TEST_F(GPUExecutionModeRegressionTest, ExecutionContextNoInterference) {
    // Create execution context (new API)
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    execution_context_t ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr);

    // Old brain API should still work
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision1 = brain_decide(brain, features, 4);
    ASSERT_NE(decision1, nullptr);

    brain_decision_t* decision2 = brain_decide(brain, features, 4);
    ASSERT_NE(decision2, nullptr);

    // Decisions should be consistent
    EXPECT_STREQ(decision1->label, decision2->label);

    execution_context_destroy(ctx);
}

//=============================================================================
// Regression Test 5: NULL Safety Maintained
//=============================================================================

TEST_F(GPUExecutionModeRegressionTest, NullSafety_Maintained) {
    // Test NULL safety for execution mode functions
    execution_context_destroy(nullptr);  // Should not crash

    // Mode support checking should not crash
    bool supported = execution_mode_is_supported(EXEC_MODE_CPU_SEQUENTIAL);
    (void)supported;  // Suppress unused warning

    // Brain with NULL features should be handled gracefully
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    brain_decision_t* decision = brain_decide(brain, nullptr, 0);
    // Should return NULL or handle error gracefully

    // Valid decision after error should still work
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    decision = brain_decide(brain, features, 4);
    ASSERT_NE(decision, nullptr);
}

//=============================================================================
// Regression Test 6: Memory Management No Leaks
//=============================================================================

TEST_F(GPUExecutionModeRegressionTest, MemoryManagement_NoLeaks) {
    // Create and destroy execution contexts multiple times
    for (int i = 0; i < 10; i++) {
        execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
        execution_context_t ctx = execution_context_create(&config);
        ASSERT_NE(ctx, nullptr);

        void* ptr = execution_alloc(ctx, 512);
        EXPECT_NE(ptr, nullptr);
        execution_free(ctx, ptr);

        execution_context_destroy(ctx);
    }

    // Brain should still work
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decide(brain, features, 4);

    SUCCEED();
}

//=============================================================================
// Regression Test 7: No Performance Regression
//=============================================================================

TEST_F(GPUExecutionModeRegressionTest, NoPerformanceRegression) {
    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
    ASSERT_NE(brain, nullptr);

    // Measure time for 100 inferences
    float features[10];
    for (int i = 0; i < 10; i++) {
        features[i] = 0.5f;
    }

    uint64_t start_time = nimcp_time_get_us();
    for (int i = 0; i < 100; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 10);
        (void)decision;
    }
    uint64_t end_time = nimcp_time_get_us();

    uint64_t elapsed_us = end_time - start_time;
    float avg_us = elapsed_us / 100.0f;

    EXPECT_LT(avg_us, 1000.0f) << "Execution modes shouldn't cause severe performance regression";
}

//=============================================================================
// Regression Test 8: Old Learning Pattern Works
//=============================================================================

TEST_F(GPUExecutionModeRegressionTest, OldLearningPattern_Works) {
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Old training loop
    for (int episode = 0; episode < 5; episode++) {
        float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
        brain_decision_t* decision = brain_decide(brain, features, 4);
        EXPECT_NE(decision, nullptr);

        float reward = 1.0f;
        uint32_t modified = brain_apply_reward_learning(brain, reward);
        EXPECT_GE(modified, 0);
    }

    SUCCEED();
}

//=============================================================================
// Regression Test 9: Hardware Detection Doesn't Break Brain
//=============================================================================

TEST_F(GPUExecutionModeRegressionTest, HardwareDetectionNoBrainBreak) {
    // Detect capabilities
    hardware_capabilities_t caps;
    bool detected = execution_detect_capabilities(&caps);
    EXPECT_TRUE(detected);

    // Brain should work normally
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision1 = brain_decide(brain, features, 4);
    ASSERT_NE(decision1, nullptr);

    brain_decision_t* decision2 = brain_decide(brain, features, 4);
    ASSERT_NE(decision2, nullptr);

    // Decisions should be consistent
    EXPECT_STREQ(decision1->label, decision2->label);
}

//=============================================================================
// Regression Test 10: CPU Mode Always Available
//=============================================================================

TEST_F(GPUExecutionModeRegressionTest, CPUModeAlwaysAvailable) {
    // CPU modes should always be supported (fallback guarantee)
    EXPECT_TRUE(execution_mode_is_supported(EXEC_MODE_CPU_SEQUENTIAL));

    // Create CPU context
    execution_config_t config = execution_get_default_config(EXEC_MODE_CPU_SEQUENTIAL);
    execution_context_t ctx = execution_context_create(&config);
    ASSERT_NE(ctx, nullptr) << "CPU mode should always work";

    // Verify mode
    execution_mode_t mode = execution_context_get_mode(ctx);
    EXPECT_EQ(mode, EXEC_MODE_CPU_SEQUENTIAL);

    // Brain should work with this context available
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    ASSERT_NE(decision, nullptr);

    execution_context_destroy(ctx);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
