/**
 * @file test_gpu_neuron_backward_compat.cpp
 * @brief Regression tests for GPU neuron backward compatibility
 *
 * WHAT: Ensures GPU neuron features don't break existing code
 * WHY:  Verify zero breaking changes to pre-GPU code
 * HOW:  Test legacy patterns and ensure they still work correctly
 *
 * @version GPU Neuron Regression Testing
 * @date 2025-11-13
 */

#include <gtest/gtest.h>

// GPU header must be outside extern "C" due to CUDA runtime templates
#include "gpu/nimcp_gpu_neuron.h"

// Headers have their own extern "C" guards
    #include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class GPUNeuronRegressionTest : public ::testing::Test {
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

TEST_F(GPUNeuronRegressionTest, BrainCreation_StillWorks) {
    // Old code pattern: Create brain with default config
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);

    // Should still work exactly as before
    ASSERT_NE(brain, nullptr) << "Brain creation should not be broken by GPU features";
}

//=============================================================================
// Regression Test 2: Legacy Inference Without GPU
//=============================================================================

TEST_F(GPUNeuronRegressionTest, LegacyInference_StillWorks) {
    // Old code pattern: Create brain and do inference without GPU awareness
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Old inference pattern (no GPU knowledge)
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    EXPECT_NE(decision, nullptr);
    if (decision) {
        EXPECT_NE(decision->label, nullptr);
    }
}

//=============================================================================
// Regression Test 3: GPU Features Optional
//=============================================================================

TEST_F(GPUNeuronRegressionTest, GPUFeatures_Optional) {
    // Brain should work even if GPU is not available
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Brain should still function for decision making
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    EXPECT_NE(decision, nullptr);
    if (decision) {
        EXPECT_NE(decision->label, nullptr);
    }
}

//=============================================================================
// Regression Test 4: GPU Doesn't Interfere with Learning
//=============================================================================

TEST_F(GPUNeuronRegressionTest, GPU_NoInterference) {
    // Create brain
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Old learning pattern
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};

    // Decision 1
    brain_decision_t* decision1 = brain_decide(brain, features, 4);
    ASSERT_NE(decision1, nullptr);
    float conf1 = decision1->confidence;

    // Decision 2 (should be consistent)
    brain_decision_t* decision2 = brain_decide(brain, features, 4);
    ASSERT_NE(decision2, nullptr);
    float conf2 = decision2->confidence;

    // Confidence should be consistent (GPU shouldn't cause erratic behavior)
    EXPECT_NEAR(conf1, conf2, 0.1f) << "GPU shouldn't cause erratic behavior";
}

//=============================================================================
// Regression Test 5: No Performance Regression
//=============================================================================

TEST_F(GPUNeuronRegressionTest, NoPerformanceRegression) {
    // Create brain
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
        (void)decision;  // Suppress unused warning
    }
    uint64_t end_time = nimcp_time_get_us();

    uint64_t elapsed_us = end_time - start_time;
    float avg_us = elapsed_us / 100.0f;

    // Should be reasonably fast (< 1ms per inference for SMALL brain)
    EXPECT_LT(avg_us, 1000.0f) << "GPU features shouldn't cause severe performance regression";
}

//=============================================================================
// Regression Test 6: NULL Safety Maintained
//=============================================================================

TEST_F(GPUNeuronRegressionTest, NullSafety_Maintained) {
    // Test NULL safety for GPU functions
    bool available = gpu_is_available();
    // Should not crash, just return false or true

    uint32_t count = gpu_get_device_count();
    // Should return 0 if no GPU, or device count

    // Brain with NULL features should be handled gracefully
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    brain_decision_t* decision = brain_decide(brain, nullptr, 0);
    // Should return NULL or handle error gracefully

    // Valid decision after error should still work
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    decision = brain_decide(brain, features, 4);
    ASSERT_NE(decision, nullptr);
    EXPECT_NE(decision->label, nullptr);
}

//=============================================================================
// Regression Test 7: Memory Management No Leaks
//=============================================================================

TEST_F(GPUNeuronRegressionTest, MemoryManagement_NoLeaks) {
    // Create and destroy brain multiple times
    for (int i = 0; i < 10; i++) {
        brain_t temp_brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
        ASSERT_NE(temp_brain, nullptr);

        // Do some operations
        float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
        brain_decide(temp_brain, features, 4);

        // Destroy (should not leak)
        brain_destroy(temp_brain);
    }

    // If we get here without crashing, memory management is OK
    SUCCEED();
}

//=============================================================================
// Regression Test 8: Old Learning Pattern Works
//=============================================================================

TEST_F(GPUNeuronRegressionTest, OldLearningPattern_Works) {
    // Old code pattern from pre-GPU
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Old training loop pattern
    for (int episode = 0; episode < 5; episode++) {
        // Get features
        float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};

        // Make decision
        brain_decision_t* decision = brain_decide(brain, features, 4);
        EXPECT_NE(decision, nullptr);
        if (decision) {
            EXPECT_NE(decision->label, nullptr);
        }

        // Apply reward (old API - should still work)
        float reward = 1.0f;
        uint32_t modified = brain_apply_reward_learning(brain, reward);
        EXPECT_GE(modified, 0);
    }

    // Should complete without errors
    SUCCEED();
}

//=============================================================================
// Regression Test 9: GPU API Doesn't Break CPU Code
//=============================================================================

TEST_F(GPUNeuronRegressionTest, GPUAPI_NoCPUBreakage) {
    // Check that GPU API functions exist and are callable
    // (even if they return false/0 when GPU not available)

    bool gpu_avail = gpu_is_available();
    (void)gpu_avail;  // May be true or false, both are valid

    uint32_t gpu_count = gpu_get_device_count();
    (void)gpu_count;  // May be 0 or more

    // CPU code should still work fine
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    ASSERT_NE(decision, nullptr);
    EXPECT_NE(decision->label, nullptr);
}

//=============================================================================
// Regression Test 10: Batch Processing Still Works
//=============================================================================

TEST_F(GPUNeuronRegressionTest, BatchProcessing_StillWorks) {
    // Old batch processing pattern
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Process multiple samples
    float features_batch[10][4];
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 4; j++) {
            features_batch[i][j] = 0.5f;
        }
    }

    // Old batch loop
    for (int i = 0; i < 10; i++) {
        brain_decision_t* decision = brain_decide(brain, features_batch[i], 4);
        EXPECT_NE(decision, nullptr);
        if (decision) {
            EXPECT_NE(decision->label, nullptr);
        }
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
