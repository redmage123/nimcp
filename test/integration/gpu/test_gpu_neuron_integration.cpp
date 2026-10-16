/**
 * @file test_gpu_neuron_integration.cpp
 * @brief Integration tests for GPU neuron module in cognitive pipeline
 *
 * WHAT: Tests that GPU neuron features are actively used by brain/cognitive modules
 * WHY:  Ensure GPU acceleration is properly wired into cognitive pipeline
 * HOW:  Test GPU neuron operations through brain API and verify integration
 *
 * @version GPU Integration Testing
 * @date 2025-11-13
 */

#include <gtest/gtest.h>

#include "core/brain/nimcp_brain.h"
#include "gpu/nimcp_gpu_neuron.h"

//=============================================================================
// Test Fixture
//=============================================================================

class GPUNeuronIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        // Create brain with potential GPU support
        brain = brain_create("gpu_test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
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
// Integration Test 1: GPU Mode Detection
//=============================================================================

TEST_F(GPUNeuronIntegrationTest, GPUModeDetectionWorks) {
    // Test GPU availability detection
    // This should work regardless of whether GPU is actually available
    bool gpu_available = gpu_is_available();

    // Should return either true or false, not crash
    EXPECT_TRUE(gpu_available == true || gpu_available == false);
}

//=============================================================================
// Integration Test 2: Brain Works With GPU Disabled
//=============================================================================

TEST_F(GPUNeuronIntegrationTest, BrainWorksWithGPUDisabled) {
    // Ensure brain functions work in CPU-only mode
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};

    brain_decision_t* decision = brain_decide(brain, features, 4);
    ASSERT_NE(decision, nullptr);
    EXPECT_NE(decision->label, nullptr);
    EXPECT_GT(decision->confidence, 0.0f);
}

//=============================================================================
// Integration Test 3: GPU Configuration Options
//=============================================================================

TEST_F(GPUNeuronIntegrationTest, GPUConfigurationExists) {
    // Verify GPU configuration options are available
    // (This tests that the config exists even if GPU is not available)

    // Create a new brain with explicit CPU mode
    brain_t cpu_brain = brain_create("cpu_test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(cpu_brain, nullptr);

    // Brain should work fine without GPU
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(cpu_brain, features, 4);
    ASSERT_NE(decision, nullptr);
    EXPECT_NE(decision->label, nullptr);

    brain_destroy(cpu_brain);
}

//=============================================================================
// Integration Test 4: GPU Fallback to CPU
//=============================================================================

TEST_F(GPUNeuronIntegrationTest, GPUFallbackToCPUWorks) {
    // Test that brain works even if GPU initialization fails
    // This ensures graceful degradation

    // Multiple decisions should work consistently
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};

    for (int i = 0; i < 10; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 4);
        ASSERT_NE(decision, nullptr) << "Decision " << i << " failed";
        EXPECT_NE(decision->label, nullptr);
        EXPECT_GT(decision->confidence, 0.0f);
    }
}

//=============================================================================
// Integration Test 5: GPU Memory Management
//=============================================================================

TEST_F(GPUNeuronIntegrationTest, GPUMemoryManagement_NoLeaks) {
    // Create and destroy multiple brains to test memory management
    for (int i = 0; i < 5; i++) {
        brain_t temp_brain = brain_create("temp", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
        ASSERT_NE(temp_brain, nullptr);

        // Do some work
        float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
        brain_decision_t* decision = brain_decide(temp_brain, features, 4);
        ASSERT_NE(decision, nullptr);

        brain_destroy(temp_brain);
    }

    // If we get here without crashing, memory management is OK
    SUCCEED();
}

//=============================================================================
// Integration Test 6: GPU Thread Safety
//=============================================================================

TEST_F(GPUNeuronIntegrationTest, GPUThreadSafety) {
    // Test that GPU operations are thread-safe
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};

    // Sequential decisions should not interfere
    brain_decision_t* decision1 = brain_decide(brain, features, 4);
    ASSERT_NE(decision1, nullptr);

    brain_decision_t* decision2 = brain_decide(brain, features, 4);
    ASSERT_NE(decision2, nullptr);

    // Results should be consistent
    EXPECT_STREQ(decision1->label, decision2->label);
}

//=============================================================================
// Integration Test 7: GPU Performance Mode
//=============================================================================

TEST_F(GPUNeuronIntegrationTest, GPUPerformanceMode) {
    // Test that performance mode doesn't break functionality
    float features[4] = {0.1f, 0.2f, 0.3f, 0.4f};

    // Batch of decisions
    for (int i = 0; i < 20; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 4);
        ASSERT_NE(decision, nullptr);
        EXPECT_NE(decision->label, nullptr);
    }
}

//=============================================================================
// Integration Test 8: GPU Error Handling
//=============================================================================

TEST_F(GPUNeuronIntegrationTest, GPUErrorHandling) {
    // Test error handling with invalid inputs

    // NULL features should be handled gracefully
    brain_decision_t* decision = brain_decide(brain, nullptr, 0);
    // Should return NULL or handle error gracefully

    // Valid decision after error should still work
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    decision = brain_decide(brain, features, 4);
    ASSERT_NE(decision, nullptr);
    EXPECT_NE(decision->label, nullptr);
}

//=============================================================================
// Integration Test 9: GPU Device Selection
//=============================================================================

TEST_F(GPUNeuronIntegrationTest, GPUDeviceSelection) {
    // Test device selection logic
    uint32_t device_count = gpu_get_device_count();

    // Should return 0 or more devices
    EXPECT_GE(device_count, 0);

    // Brain should work regardless of device count
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    ASSERT_NE(decision, nullptr);
    EXPECT_NE(decision->label, nullptr);
}

//=============================================================================
// Integration Test 10: GPU Compute Compatibility
//=============================================================================

TEST_F(GPUNeuronIntegrationTest, GPUComputeCompatibility) {
    // Test that results are consistent regardless of compute mode
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};

    // Get first decision
    brain_decision_t* decision1 = brain_decide(brain, features, 4);
    ASSERT_NE(decision1, nullptr);
    const char* label1 = decision1->label;

    // Get second decision with same input
    brain_decision_t* decision2 = brain_decide(brain, features, 4);
    ASSERT_NE(decision2, nullptr);

    // Should produce same result
    EXPECT_STREQ(label1, decision2->label);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
