/**
 * @file test_brain_error_paths.cpp
 * @brief Targeted tests for brain error handling paths
 *
 * PURPOSE: Cover uncovered error handling code paths to increase coverage
 * TARGET: Memory allocation failures, initialization errors, edge cases
 * COVERAGE GOAL: Add ~3% coverage by testing error paths
 */

#include <gtest/gtest.h>
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
#include "nimcp.h"

class BrainErrorPathTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_init();
        brain_clear_error();
    }

    void TearDown() override {
        nimcp_shutdown();
    }
};

//=============================================================================
// Edge Case Tests - Size Presets
//=============================================================================

TEST_F(BrainErrorPathTest, CustomSizePreset) {
    // Test BRAIN_SIZE_CUSTOM enum value
    brain_t brain = brain_create("custom_size", BRAIN_SIZE_CUSTOM,
                                BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Custom size should fall back to default size (1000 neurons)
    brain_decision_t* decision = brain_decide(brain, nullptr, 0);
    // Even with null input, brain should not crash

    brain_destroy(brain);
}

TEST_F(BrainErrorPathTest, AllSizePresets) {
    // Test all brain size presets to cover all switch cases
    brain_size_t sizes[] = {
        BRAIN_SIZE_TINY,
        BRAIN_SIZE_SMALL,
        BRAIN_SIZE_MEDIUM,
        BRAIN_SIZE_LARGE,
        BRAIN_SIZE_CUSTOM
    };

    const char* size_names[] = {"tiny", "small", "medium", "large", "custom"};

    for (size_t i = 0; i < 5; i++) {
        brain_t brain = brain_create(size_names[i], sizes[i],
                                    BRAIN_TASK_CLASSIFICATION, 5, 2);
        ASSERT_NE(brain, nullptr) << "Failed to create brain with size " << size_names[i];
        brain_destroy(brain);
    }
}

//=============================================================================
// Null Input Tests
//=============================================================================

TEST_F(BrainErrorPathTest, NullBrainGetNetwork) {
    // Test brain_get_network with NULL brain
    adaptive_network_t network = brain_get_network(nullptr);
    EXPECT_EQ(network, nullptr);

    // Check that error was set
    const char* error = brain_get_last_error();
    EXPECT_NE(error, nullptr);
    EXPECT_TRUE(strstr(error, "NULL brain") != nullptr);

    brain_clear_error();
}

TEST_F(BrainErrorPathTest, NullBrainGetNeuromodulatorSystem) {
    // Test brain_get_neuromodulator_system with NULL brain
    neuromodulator_system_t system = brain_get_neuromodulator_system(nullptr);
    EXPECT_EQ(system, nullptr);

    // Check that error was set
    const char* error = brain_get_last_error();
    EXPECT_NE(error, nullptr);
    EXPECT_TRUE(strstr(error, "NULL brain") != nullptr);

    brain_clear_error();
}

TEST_F(BrainErrorPathTest, LearnExampleWithNullBrain) {
    // Test brain_learn_example with NULL brain
    float features[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float result = brain_learn_example(nullptr, features, 5, "label", 1.0f);
    EXPECT_EQ(result, -1.0f);  // Should return error value
}

TEST_F(BrainErrorPathTest, DecideWithNullBrain) {
    // Test brain_decide with NULL brain
    float features[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    brain_decision_t* decision = brain_decide(nullptr, features, 5);
    EXPECT_EQ(decision, nullptr);
}

TEST_F(BrainErrorPathTest, DestroyNullBrain) {
    // Test brain_destroy with NULL brain (should not crash)
    brain_destroy(nullptr);
    // If we get here without crashing, test passes
    SUCCEED();
}

//=============================================================================
// Invalid Input Tests
//=============================================================================

TEST_F(BrainErrorPathTest, CreateBrainWithZeroInputs) {
    // Test creating brain with zero input dimension
    brain_t brain = brain_create("zero_inputs", BRAIN_SIZE_TINY,
                                BRAIN_TASK_CLASSIFICATION, 0, 3);
    // This might succeed or fail depending on validation
    // Just ensure it doesn't crash
    if (brain != nullptr) {
        brain_destroy(brain);
    }
}

TEST_F(BrainErrorPathTest, CreateBrainWithZeroOutputs) {
    // Test creating brain with zero output dimension
    brain_t brain = brain_create("zero_outputs", BRAIN_SIZE_TINY,
                                BRAIN_TASK_CLASSIFICATION, 10, 0);
    // This might succeed or fail depending on validation
    // Just ensure it doesn't crash
    if (brain != nullptr) {
        brain_destroy(brain);
    }
}

TEST_F(BrainErrorPathTest, LearnWithNullFeatures) {
    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Try to learn with NULL features
    float result = brain_learn_example(brain, nullptr, 10, "label", 1.0f);
    EXPECT_EQ(result, -1.0f);  // Should return error value

    brain_destroy(brain);
}

TEST_F(BrainErrorPathTest, LearnWithNullLabel) {
    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    float features[10] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};

    // Try to learn with NULL label
    float result = brain_learn_example(brain, features, 10, nullptr, 1.0f);
    EXPECT_EQ(result, -1.0f);  // Should return error value

    brain_destroy(brain);
}

TEST_F(BrainErrorPathTest, DecideWithNullFeatures) {
    brain_t brain = brain_create("test", BRAIN_SIZE_TINY,
                                BRAIN_TASK_CLASSIFICATION, 10, 3);
    ASSERT_NE(brain, nullptr);

    // Try to decide with NULL features
    brain_decision_t* decision = brain_decide(brain, nullptr, 10);
    EXPECT_EQ(decision, nullptr);

    brain_destroy(brain);
}

//=============================================================================
// Extreme Value Tests
//=============================================================================

TEST_F(BrainErrorPathTest, LearnWithVeryLargeReward) {
    brain_t brain = brain_create("large_reward", BRAIN_SIZE_TINY,
                                BRAIN_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    float features[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    // Test with extremely large reward value
    bool success = brain_learn_example(brain, features, 5, "label", 1000000.0f);
    EXPECT_TRUE(success);  // Should handle large values gracefully

    brain_destroy(brain);
}

TEST_F(BrainErrorPathTest, LearnWithNegativeReward) {
    brain_t brain = brain_create("negative_reward", BRAIN_SIZE_TINY,
                                BRAIN_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    float features[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    // Test with negative reward (punishment)
    // Negative confidence gets clamped to minimum (0.01f) by epistemic filter
    float loss = brain_learn_example(brain, features, 5, "label", -1.0f);
    EXPECT_GE(loss, 0.0f);  // Should clamp and return valid loss (not error -1.0f)

    brain_destroy(brain);
}

TEST_F(BrainErrorPathTest, LearnWithZeroReward) {
    brain_t brain = brain_create("zero_reward", BRAIN_SIZE_TINY,
                                BRAIN_TASK_CLASSIFICATION, 5, 2);
    ASSERT_NE(brain, nullptr);

    float features[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    // Test with zero reward (no reinforcement)
    float result = brain_learn_example(brain, features, 5, "label", 0.0f);
    EXPECT_GE(result, 0.0f);  // Should succeed (loss >= 0)

    brain_destroy(brain);
}

TEST_F(BrainErrorPathTest, FeaturesWithInfinityValues) {
    brain_t brain = brain_create("infinity_test", BRAIN_SIZE_TINY,
                                BRAIN_TASK_REGRESSION, 5, 1);
    ASSERT_NE(brain, nullptr);

    float features[5] = {1.0f, 2.0f, INFINITY, 4.0f, 5.0f};

    // Test with infinity values - should handle gracefully
    bool success = brain_learn_example(brain, features, 5, "3.0", 1.0f);
    // May succeed or fail depending on validation

    brain_destroy(brain);
}

TEST_F(BrainErrorPathTest, FeaturesWithNaNValues) {
    brain_t brain = brain_create("nan_test", BRAIN_SIZE_TINY,
                                BRAIN_TASK_REGRESSION, 5, 1);
    ASSERT_NE(brain, nullptr);

    float features[5] = {1.0f, NAN, 3.0f, 4.0f, 5.0f};

    // Test with NaN values - should handle gracefully
    bool success = brain_learn_example(brain, features, 5, "2.5", 1.0f);
    // May succeed or fail depending on validation

    brain_destroy(brain);
}

//=============================================================================
// Multiple Operations Tests
//=============================================================================

TEST_F(BrainErrorPathTest, MultipleLearnOperations) {
    brain_t brain = brain_create("multiple_learn", BRAIN_SIZE_TINY,
                                BRAIN_TASK_CLASSIFICATION, 8, 3);
    ASSERT_NE(brain, nullptr);

    float features[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};

    // Perform many learning operations to test stability
    for (int i = 0; i < 20; i++) {
        bool success = brain_learn_example(brain, features, 8, "repeated_label", 1.0f);
        EXPECT_TRUE(success);
    }

    brain_destroy(brain);
}

TEST_F(BrainErrorPathTest, AlternatingLearnDecide) {
    brain_t brain = brain_create("alternating", BRAIN_SIZE_TINY,
                                BRAIN_TASK_CLASSIFICATION, 6, 2);
    ASSERT_NE(brain, nullptr);

    float features1[6] = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f};
    float features2[6] = {0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f};

    // Alternate between learning and deciding
    for (int i = 0; i < 10; i++) {
        bool success = brain_learn_example(brain, features1, 6, "pattern_a", 1.0f);
        EXPECT_TRUE(success);

        brain_decision_t* decision = brain_decide(brain, features2, 6);
        ASSERT_NE(decision, nullptr);
        brain_free_decision(decision);

        success = brain_learn_example(brain, features2, 6, "pattern_b", 1.0f);
        EXPECT_TRUE(success);

        decision = brain_decide(brain, features1, 6);
        ASSERT_NE(decision, nullptr);
        brain_free_decision(decision);
    }

    brain_destroy(brain);
}

//=============================================================================
// Error Message Tests
//=============================================================================

TEST_F(BrainErrorPathTest, ErrorMessagePersistence) {
    // Trigger an error
    brain_get_network(nullptr);

    // Error should be set
    const char* error1 = brain_get_last_error();
    ASSERT_NE(error1, nullptr);

    // Error should persist
    const char* error2 = brain_get_last_error();
    EXPECT_STREQ(error1, error2);

    // Clear error
    brain_clear_error();

    // Error should be cleared
    const char* error3 = brain_get_last_error();
    EXPECT_EQ(error3, nullptr);
}

TEST_F(BrainErrorPathTest, MultipleErrorOverwrite) {
    // Trigger first error
    brain_get_network(nullptr);
    const char* error1_ptr = brain_get_last_error();
    ASSERT_NE(error1_ptr, nullptr);

    // Copy first error message (since it's a pointer to internal buffer)
    char error1_copy[256];
    strncpy(error1_copy, error1_ptr, sizeof(error1_copy) - 1);
    error1_copy[sizeof(error1_copy) - 1] = '\0';

    // Trigger second error (should overwrite first)
    brain_get_neuromodulator_system(nullptr);
    const char* error2 = brain_get_last_error();
    ASSERT_NE(error2, nullptr);
    EXPECT_STRNE(error1_copy, error2);  // Different error messages

    brain_clear_error();
}

//=============================================================================
// Task Type Edge Cases
//=============================================================================

TEST_F(BrainErrorPathTest, AllTaskTypes) {
    // Test all task types to cover all strategy creation paths
    brain_task_t tasks[] = {
        BRAIN_TASK_CLASSIFICATION,
        BRAIN_TASK_REGRESSION,
        BRAIN_TASK_PATTERN_MATCHING,
        BRAIN_TASK_SEQUENCE,
        BRAIN_TASK_ASSOCIATION,
        BRAIN_TASK_CUSTOM
    };

    const char* task_names[] = {
        "classification", "regression", "pattern_matching",
        "sequence", "association", "custom"
    };

    for (size_t i = 0; i < 6; i++) {
        brain_t brain = brain_create(task_names[i], BRAIN_SIZE_TINY, tasks[i], 5, 2);
        ASSERT_NE(brain, nullptr) << "Failed to create brain for task " << task_names[i];

        // Perform a simple operation to ensure strategy works
        float features[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
        bool success = brain_learn_example(brain, features, 5, "test", 1.0f);
        EXPECT_TRUE(success);

        brain_destroy(brain);
    }
}

TEST_F(BrainErrorPathTest, SequenceTaskStrategy) {
    // Specifically test BRAIN_TASK_SEQUENCE (often less tested)
    brain_t brain = brain_create("sequence", BRAIN_SIZE_TINY,
                                BRAIN_TASK_SEQUENCE, 10, 5);
    ASSERT_NE(brain, nullptr);

    // Test with temporal sequence
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    for (int t = 0; t < 5; t++) {
        bool success = brain_learn_example(brain, features, 10, "seq_label", 1.0f);
        EXPECT_TRUE(success);
    }

    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);

    brain_free_decision(decision);
    brain_destroy(brain);
}

//=============================================================================
// Brain State Tests
//=============================================================================

TEST_F(BrainErrorPathTest, CreateAndDestroyMultipleBrains) {
    // Test creating and destroying multiple brains in sequence
    for (int i = 0; i < 5; i++) {
        brain_t brain = brain_create("sequential", BRAIN_SIZE_TINY,
                                    BRAIN_TASK_CLASSIFICATION, 5, 2);
        ASSERT_NE(brain, nullptr);
        brain_destroy(brain);
    }
}

TEST_F(BrainErrorPathTest, MultipleSimultaneousBrains) {
    // Test multiple brains existing simultaneously
    brain_t brain1 = brain_create("brain1", BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION, 5, 2);
    brain_t brain2 = brain_create("brain2", BRAIN_SIZE_TINY,
                                 BRAIN_TASK_REGRESSION, 5, 1);
    brain_t brain3 = brain_create("brain3", BRAIN_SIZE_TINY,
                                 BRAIN_TASK_PATTERN_MATCHING, 5, 2);

    ASSERT_NE(brain1, nullptr);
    ASSERT_NE(brain2, nullptr);
    ASSERT_NE(brain3, nullptr);

    // Use all three brains
    float features[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};

    bool success1 = brain_learn_example(brain1, features, 5, "class1", 1.0f);
    bool success2 = brain_learn_example(brain2, features, 5, "2.5", 1.0f);
    bool success3 = brain_learn_example(brain3, features, 5, "pattern1", 1.0f);

    EXPECT_TRUE(success1);
    EXPECT_TRUE(success2);
    EXPECT_TRUE(success3);

    brain_destroy(brain1);
    brain_destroy(brain2);
    brain_destroy(brain3);
}
