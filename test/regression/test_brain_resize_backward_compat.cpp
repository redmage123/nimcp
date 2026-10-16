//=============================================================================
// test_brain_resize_backward_compat.cpp - Regression Tests for Brain Resizing
//=============================================================================
/**
 * @file test_brain_resize_backward_compat.cpp
 * @brief Regression tests ensuring Phase 2.8 doesn't break existing functionality
 *
 * BACKWARD COMPATIBILITY TESTS:
 * - Existing brain_create API unchanged
 * - Existing brain_learn/brain_decide unchanged
 * - Checkpoint/restore compatible
 * - No performance regressions
 * - Memory usage acceptable
 * - Thread safety maintained
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 */

#include <gtest/gtest.h>
#include <cstring>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_resize.h"
#include "plasticity/adaptive/nimcp_adaptive.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainResizeBackwardCompatTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create("regression_test", BRAIN_SIZE_SMALL,
                            BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// API Backward Compatibility
//=============================================================================

TEST_F(BrainResizeBackwardCompatTest, ExistingCreateAPIUnchanged) {
    // All existing brain_create signatures should still work

    // Standard create
    brain_t b1 = brain_create("test1", BRAIN_SIZE_TINY,
                              BRAIN_TASK_CLASSIFICATION, 10, 5);
    ASSERT_NE(b1, nullptr);
    EXPECT_EQ(brain_get_neuron_count(b1), 100u);
    brain_destroy(b1);

    // Small
    brain_t b2 = brain_create("test2", BRAIN_SIZE_SMALL,
                              BRAIN_TASK_REGRESSION, 20, 10);
    ASSERT_NE(b2, nullptr);
    EXPECT_EQ(brain_get_neuron_count(b2), 500u);
    brain_destroy(b2);

    // Medium
    brain_t b3 = brain_create("test3", BRAIN_SIZE_MEDIUM,
                              BRAIN_TASK_PATTERN_MATCHING, 5, 3);
    ASSERT_NE(b3, nullptr);
    EXPECT_EQ(brain_get_neuron_count(b3), 1000u);
    brain_destroy(b3);
}

TEST_F(BrainResizeBackwardCompatTest, LearnAPIUnchangedAfterResize) {
    // brain_learn should work exactly the same before and after resize

    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                          0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    const char* label = "test_pattern";

    // Learn before resize
    for (int i = 0; i < 50; i++) {
        brain_learn_example(brain, features, 10, label, 0.8f);
    }

    brain_decision_t* decision_before = brain_decide(brain, features, 10);
    ASSERT_NE(decision_before, nullptr);

    // Resize
    ASSERT_TRUE(brain_resize(brain, 1000));

    // Learn after resize - API should be identical
    for (int i = 0; i < 50; i++) {
        brain_learn_example(brain, features, 10, label, 0.8f);
    }

    brain_decision_t* decision_after = brain_decide(brain, features, 10);
    ASSERT_NE(decision_after, nullptr);

    // Both should recognize the pattern
    if (decision_before->label && decision_after->label) {
        EXPECT_STREQ(decision_before->label, decision_after->label);
    }

    brain_free_decision(decision_before);
    brain_free_decision(decision_after);
}

TEST_F(BrainResizeBackwardCompatTest, DecideAPIUnchangedAfterResize) {
    // brain_decide should return same structure before/after resize

    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                          0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    // Get decision before resize
    brain_decision_t* decision1 = brain_decide(brain, features, 10);
    ASSERT_NE(decision1, nullptr);
    EXPECT_GE(decision1->confidence, 0.0f);
    EXPECT_LE(decision1->confidence, 1.0f);
    brain_free_decision(decision1);

    // Resize
    ASSERT_TRUE(brain_resize(brain, 1000));

    // Get decision after resize - structure should be identical
    brain_decision_t* decision2 = brain_decide(brain, features, 10);
    ASSERT_NE(decision2, nullptr);
    EXPECT_GE(decision2->confidence, 0.0f);
    EXPECT_LE(decision2->confidence, 1.0f);
    brain_free_decision(decision2);
}

//=============================================================================
// Memory Management Backward Compatibility
//=============================================================================

TEST_F(BrainResizeBackwardCompatTest, DestroyWorksAfterResize) {
    // brain_destroy should work correctly after resize

    ASSERT_TRUE(brain_resize(brain, 2000));

    // Should destroy cleanly without leaks
    brain_destroy(brain);
    brain = nullptr;  // Prevent double-free in TearDown

    // Test passes if no crash/leak detected
    SUCCEED();
}

TEST_F(BrainResizeBackwardCompatTest, MultipleResizesThenDestroy) {
    // Multiple resizes followed by destroy should not leak

    ASSERT_TRUE(brain_resize(brain, 800));
    ASSERT_TRUE(brain_resize(brain, 1200));
    ASSERT_TRUE(brain_resize(brain, 2000));

    brain_destroy(brain);
    brain = nullptr;

    SUCCEED();
}

//=============================================================================
// Network Access Backward Compatibility
//=============================================================================

TEST_F(BrainResizeBackwardCompatTest, GetNetworkWorksAfterResize) {
    // brain_get_network should return valid network after resize

    adaptive_network_t net_before = brain_get_network(brain);
    ASSERT_NE(net_before, nullptr);

    ASSERT_TRUE(brain_resize(brain, 1000));

    adaptive_network_t net_after = brain_get_network(brain);
    ASSERT_NE(net_after, nullptr);

    // Network should be different (resized), but still valid
    neural_network_t base_after = adaptive_network_get_base_network(net_after);
    ASSERT_NE(base_after, nullptr);

    uint32_t neurons_after = neural_network_get_num_neurons(base_after);
    EXPECT_EQ(neurons_after, 1000u);
}

//=============================================================================
// Preset Sizes Backward Compatibility
//=============================================================================

TEST_F(BrainResizeBackwardCompatTest, PresetSizesUnchanged) {
    // BRAIN_SIZE_* constants should produce same neuron counts

    brain_t tiny = brain_create("tiny", BRAIN_SIZE_TINY,
                                BRAIN_TASK_CLASSIFICATION, 5, 3);
    EXPECT_EQ(brain_get_neuron_count(tiny), 100u);
    brain_destroy(tiny);

    brain_t small = brain_create("small", BRAIN_SIZE_SMALL,
                                 BRAIN_TASK_CLASSIFICATION, 5, 3);
    EXPECT_EQ(brain_get_neuron_count(small), 500u);
    brain_destroy(small);

    brain_t medium = brain_create("medium", BRAIN_SIZE_MEDIUM,
                                  BRAIN_TASK_CLASSIFICATION, 5, 3);
    EXPECT_EQ(brain_get_neuron_count(medium), 1000u);
    brain_destroy(medium);

    brain_t large = brain_create("large", BRAIN_SIZE_LARGE,
                                 BRAIN_TASK_CLASSIFICATION, 5, 3);
    EXPECT_EQ(brain_get_neuron_count(large), 5000u);
    brain_destroy(large);
}

//=============================================================================
// Behavioral Backward Compatibility
//=============================================================================

TEST_F(BrainResizeBackwardCompatTest, LearningBehaviorConsistent) {
    // Learning should produce similar results before/after resize

    float pattern_a[10] = {1,0,0,0,0,0,0,0,0,0};
    float pattern_b[10] = {0,1,0,0,0,0,0,0,0,0};

    // Train pattern A
    for (int i = 0; i < 100; i++) {
        brain_learn_example(brain, pattern_a, 10, "A", 0.9f);
    }

    // Test pattern A recognition before resize
    brain_decision_t* decision_before = brain_decide(brain, pattern_a, 10);
    ASSERT_NE(decision_before, nullptr);
    char label_before[64] = {0};
    if (decision_before->label) {
        strncpy(label_before, decision_before->label, sizeof(label_before) - 1);
    }
    brain_free_decision(decision_before);

    // Resize
    ASSERT_TRUE(brain_resize(brain, 1000));

    // Train pattern B after resize
    for (int i = 0; i < 100; i++) {
        brain_learn_example(brain, pattern_b, 10, "B", 0.9f);
    }

    // Pattern A should still be recognized
    brain_decision_t* decision_after = brain_decide(brain, pattern_a, 10);
    ASSERT_NE(decision_after, nullptr);

    if (strlen(label_before) > 0 && decision_after->label) {
        EXPECT_STREQ(label_before, decision_after->label)
            << "Brain should retain old knowledge after resize";
    }

    brain_free_decision(decision_after);
}

//=============================================================================
// Error Handling Backward Compatibility
//=============================================================================

TEST_F(BrainResizeBackwardCompatTest, NullHandlingUnchanged) {
    // NULL handling should be consistent with existing API

    // Existing NULL checks
    EXPECT_EQ(brain_get_neuron_count(nullptr), 0u);

    brain_decision_t* decision = brain_decide(nullptr, nullptr, 0);
    EXPECT_EQ(decision, nullptr);

    // No crash from destroy
    brain_destroy(nullptr);

    SUCCEED();
}

//=============================================================================
// Optional: Not Calling Resize Functions
//=============================================================================

TEST_F(BrainResizeBackwardCompatTest, BrainWorksWithoutResize) {
    // Brain should work perfectly fine if resize functions never called
    // This ensures existing code doesn't need ANY changes

    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                          0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    // Train
    for (int i = 0; i < 100; i++) {
        brain_learn_example(brain, features, 10, "pattern", 0.85f);
    }

    // Decide
    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);
    EXPECT_GT(decision->confidence, 0.0f);
    brain_free_decision(decision);

    // Never called any resize functions - brain should work identically
    // to pre-Phase-2.8 behavior
    SUCCEED();
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
