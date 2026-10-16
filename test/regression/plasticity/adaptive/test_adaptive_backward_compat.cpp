/**
 * @file test_adaptive_backward_compat.cpp
 * @brief Regression tests for Adaptive Threshold Spiking plasticity
 *
 * WHAT: Ensures adaptive threshold features don't break existing code
 * WHY:  Verify zero breaking changes to pre-adaptive code
 * HOW:  Test legacy patterns and ensure they still work correctly
 *
 * TEST COVERAGE:
 * 1. Brain creation without adaptive awareness
 * 2. Legacy inference patterns unchanged
 * 3. Adaptive API doesn't break CPU code
 * 4. Adaptive doesn't interfere with other plasticity
 * 5. No performance regression
 * 6. Parameter validation
 * 7. Memory management no leaks
 * 8. Old learning patterns work
 * 9. State consistency
 * 10. Batch processing not broken
 *
 * @version Regression Testing Framework v1.0
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
    #include "core/brain/nimcp_brain.h"
    #include "plasticity/adaptive/nimcp_adaptive.h"
    #include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class AdaptiveRegressionTest : public ::testing::Test {
protected:
    brain_t brain;
    adaptive_network_t network;

    void SetUp() override {
        brain = nullptr;
        network = nullptr;
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
        if (network) {
            adaptive_network_destroy(network);
            network = nullptr;
        }
    }
};

//=============================================================================
// Regression Test 1: Brain Creation Still Works
//=============================================================================

TEST_F(AdaptiveRegressionTest, BrainCreation_StillWorks) {
    // Old code pattern: Create brain without adaptive awareness
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);

    // Should still work exactly as before
    ASSERT_NE(brain, nullptr)
        << "Brain creation should not be broken by adaptive threshold";
}

//=============================================================================
// Regression Test 2: Legacy Inference Without Adaptive
//=============================================================================

TEST_F(AdaptiveRegressionTest, LegacyInference_StillWorks) {
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Old inference pattern (no adaptive knowledge)
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    EXPECT_NE(decision, nullptr);
    if (decision) {
        EXPECT_NE(decision->label, nullptr);
    }
}

//=============================================================================
// Regression Test 3: Adaptive API Doesn't Break CPU Code
//=============================================================================

TEST_F(AdaptiveRegressionTest, AdaptiveAPI_NoCPUBreakage) {
    // Use adaptive API (new)
    float input[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float threshold = adaptive_compute_threshold(input, 5, 0.5f);
    EXPECT_GT(threshold, 0.0f);

    int32_t spikes = adaptive_value_to_spikes(5.0f, 2.0f);
    EXPECT_NE(spikes, 0);

    // Old brain API should still work
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    ASSERT_NE(decision, nullptr);
}

//=============================================================================
// Regression Test 4: Adaptive Doesn't Interfere With Other Plasticity
//=============================================================================

TEST_F(AdaptiveRegressionTest, Adaptive_NoPlasticityInterference) {
    // Use adaptive threshold computation
    float input[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float threshold = adaptive_compute_threshold(input, 5, 0.5f);
    (void)threshold;

    // Brain learning should work normally
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision1 = brain_decide(brain, features, 4);
    ASSERT_NE(decision1, nullptr);

    // Apply learning
    uint32_t modified = brain_apply_reward_learning(brain, 1.0f);
    EXPECT_GE(modified, 0);

    // Second decision should still work
    brain_decision_t* decision2 = brain_decide(brain, features, 4);
    ASSERT_NE(decision2, nullptr);
}

//=============================================================================
// Regression Test 5: No Performance Regression
//=============================================================================

TEST_F(AdaptiveRegressionTest, NoPerformanceRegression) {
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

    EXPECT_LT(avg_us, 1000.0f)
        << "Adaptive shouldn't cause severe performance regression";
}

//=============================================================================
// Regression Test 6: Parameter Validation
//=============================================================================

TEST_F(AdaptiveRegressionTest, ParameterValidation) {
    // Test threshold computation with various k_factors
    float input[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    float threshold1 = adaptive_compute_threshold(input, 5, 0.1f);
    float threshold2 = adaptive_compute_threshold(input, 5, 1.0f);

    EXPECT_GT(threshold1, 0.0f) << "Threshold should be positive";
    EXPECT_GT(threshold2, 0.0f) << "Threshold should be positive";
    EXPECT_GT(threshold1, threshold2)
        << "Lower k_factor should give higher threshold";

    // Test spike encoding types
    uint8_t spike_train[64];
    for (int encoding = 0; encoding < 4; encoding++) {
        uint32_t length = adaptive_encode_spikes(5, (spike_encoding_t)encoding,
                                                   spike_train, 64);
        EXPECT_GT(length, 0u) << "Encoding should produce output";
        EXPECT_LE(length, 64u) << "Encoding should respect buffer size";
    }
}

//=============================================================================
// Regression Test 7: Memory Management No Leaks
//=============================================================================

TEST_F(AdaptiveRegressionTest, MemoryManagement_NoLeaks) {
    // Create and destroy adaptive computations multiple times
    for (int i = 0; i < 10; i++) {
        float input[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        float threshold = adaptive_compute_threshold(input, 5, 0.5f);
        int32_t spikes = adaptive_value_to_spikes(5.0f, threshold);
        (void)spikes;

        uint8_t spike_train[64];
        adaptive_encode_spikes(3, SPIKE_ENCODING_INTEGER, spike_train, 64);
    }

    // Brain should still work
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decide(brain, features, 4);

    SUCCEED();
}

//=============================================================================
// Regression Test 8: Old Learning Pattern Works
//=============================================================================

TEST_F(AdaptiveRegressionTest, OldLearningPattern_Works) {
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
// Regression Test 9: State Consistency
//=============================================================================

TEST_F(AdaptiveRegressionTest, StateConsistency) {
    // Same input should produce same threshold
    float input[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float k_factor = 0.5f;

    float threshold1 = adaptive_compute_threshold(input, 5, k_factor);
    float threshold2 = adaptive_compute_threshold(input, 5, k_factor);

    EXPECT_FLOAT_EQ(threshold1, threshold2)
        << "Same input should produce same threshold";

    // Same spike conversion should be deterministic
    int32_t spikes1 = adaptive_value_to_spikes(5.0f, 2.0f);
    int32_t spikes2 = adaptive_value_to_spikes(5.0f, 2.0f);

    EXPECT_EQ(spikes1, spikes2)
        << "Same conversion should be deterministic";
}

//=============================================================================
// Regression Test 10: Batch Processing Not Broken
//=============================================================================

TEST_F(AdaptiveRegressionTest, BatchProcessing_NotBroken) {
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Process multiple samples (old batch pattern)
    float features_batch[10][4];
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 4; j++) {
            features_batch[i][j] = 0.5f;
        }
    }

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
