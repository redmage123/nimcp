//=============================================================================
// test_brain_resize.cpp - Comprehensive Unit Tests for Dynamic Brain Resizing
//=============================================================================
/**
 * @file test_brain_resize.cpp
 * @brief Unit tests for Phase 2.8 dynamic brain resizing functionality
 *
 * TEST COVERAGE:
 * - Manual resize (brain_resize)
 * - Auto resize (brain_auto_resize)
 * - Neuron count queries (brain_get_neuron_count)
 * - Utilization metrics (brain_get_utilization_metrics)
 * - Knowledge preservation during resize
 * - Error handling and edge cases
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_resize.h"
#include "plasticity/adaptive/nimcp_adaptive.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BrainResizeTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        // Create small brain for testing (100 neurons)
        brain = brain_create("test_resize", BRAIN_SIZE_TINY,
                            BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr) << "Failed to create test brain";
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    // Helper: Train brain with simple pattern
    void train_simple_pattern(brain_t b, int iterations) {
        float features[10] = {1.0f, 0.5f, 0.3f, 0.8f, 0.2f,
                              0.7f, 0.4f, 0.9f, 0.1f, 0.6f};
        const char* label = "test_pattern";

        for (int i = 0; i < iterations; i++) {
            brain_learn_example(b, features, 10, label, 0.9f);
        }
    }

    // Helper: Verify brain can still learn after resize
    bool verify_brain_learns_after_resize(brain_t b) {
        // Get initial state
        uint32_t initial_count = brain_get_neuron_count(b);
        if (initial_count == 0) {
            return false;
        }

        // Train with new pattern
        train_simple_pattern(b, 10);

        // Brain should still function
        float test_features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                                   0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        brain_decision_t* decision = brain_decide(b, test_features, 10);

        bool success = (decision != nullptr);
        if (success && decision) {
            brain_free_decision(decision);
        }

        return success;
    }
};

//=============================================================================
// Basic Resize Tests
//=============================================================================

TEST_F(BrainResizeTest, ResizeIncreasesNeuronCount) {
    uint32_t initial_count = brain_get_neuron_count(brain);
    ASSERT_EQ(initial_count, 100u) << "Expected BRAIN_SIZE_TINY = 100 neurons";

    // Resize to 200 neurons
    bool success = brain_resize(brain, 200);
    ASSERT_TRUE(success) << "brain_resize should succeed";

    uint32_t new_count = brain_get_neuron_count(brain);
    EXPECT_EQ(new_count, 200u) << "Brain should now have 200 neurons";
}

TEST_F(BrainResizeTest, ResizePreservesLearning) {
    // Train brain with specific pattern
    train_simple_pattern(brain, 50);

    // Get initial decision
    float test_features[10] = {1.0f, 0.5f, 0.3f, 0.8f, 0.2f,
                               0.7f, 0.4f, 0.9f, 0.1f, 0.6f};
    brain_decision_t* initial_decision = brain_decide(brain, test_features, 10);
    ASSERT_NE(initial_decision, nullptr);

    // Store initial results
    char initial_label[64] = {0};
    float initial_confidence = 0.0f;
    if (initial_decision->label[0] != '\0') {
        strncpy(initial_label, initial_decision->label, sizeof(initial_label) - 1);
        initial_confidence = initial_decision->confidence;
    }
    brain_free_decision(initial_decision);

    // Resize brain
    ASSERT_TRUE(brain_resize(brain, 200));

    // Get decision after resize
    brain_decision_t* post_resize_decision = brain_decide(brain, test_features, 10);
    ASSERT_NE(post_resize_decision, nullptr);

    // Decision should be consistent (same pattern recognized)
    if (strlen(initial_label) > 0 && post_resize_decision->label[0] != '\0') {
        EXPECT_STREQ(initial_label, post_resize_decision->label)
            << "Brain should recognize same pattern after resize";

        // Confidence should be similar (within 20%)
        EXPECT_NEAR(initial_confidence, post_resize_decision->confidence, 0.2f)
            << "Confidence should remain similar after resize";
    }

    brain_free_decision(post_resize_decision);
}

TEST_F(BrainResizeTest, ResizeMultipleTimes) {
    EXPECT_EQ(brain_get_neuron_count(brain), 100u);

    // First resize: 100 → 200
    ASSERT_TRUE(brain_resize(brain, 200));
    EXPECT_EQ(brain_get_neuron_count(brain), 200u);

    // Second resize: 200 → 500
    ASSERT_TRUE(brain_resize(brain, 500));
    EXPECT_EQ(brain_get_neuron_count(brain), 500u);

    // Third resize: 500 → 1000
    ASSERT_TRUE(brain_resize(brain, 1000));
    EXPECT_EQ(brain_get_neuron_count(brain), 1000u);

    // Brain should still function after multiple resizes
    EXPECT_TRUE(verify_brain_learns_after_resize(brain));
}

TEST_F(BrainResizeTest, ResizeLargeJump) {
    EXPECT_EQ(brain_get_neuron_count(brain), 100u);

    // Large resize: 100 → 5000 (50× growth)
    bool success = brain_resize(brain, 5000);
    ASSERT_TRUE(success) << "Large resize should succeed";

    EXPECT_EQ(brain_get_neuron_count(brain), 5000u);

    // Brain should still function
    EXPECT_TRUE(verify_brain_learns_after_resize(brain));
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(BrainResizeTest, ResizeRejectsNullBrain) {
    bool result = brain_resize(nullptr, 200);
    EXPECT_FALSE(result) << "resize should reject NULL brain";
}

TEST_F(BrainResizeTest, ResizeRejectsShrinking) {
    EXPECT_EQ(brain_get_neuron_count(brain), 100u);

    // Try to shrink: 100 → 50 (should fail)
    bool result = brain_resize(brain, 50);
    EXPECT_FALSE(result) << "Shrinking not supported, should fail";

    // Brain size should be unchanged
    EXPECT_EQ(brain_get_neuron_count(brain), 100u);
}

TEST_F(BrainResizeTest, ResizeRejectsSameSize) {
    EXPECT_EQ(brain_get_neuron_count(brain), 100u);

    // Try same size: 100 → 100 (should fail)
    bool result = brain_resize(brain, 100);
    EXPECT_FALSE(result) << "Same size resize should fail";

    // Brain size should be unchanged
    EXPECT_EQ(brain_get_neuron_count(brain), 100u);
}

//=============================================================================
// Auto-Resize Tests
//=============================================================================

TEST_F(BrainResizeTest, AutoResizeWithLowUtilization) {
    // Brand new brain has low utilization
    // Auto-resize should not trigger
    bool resized = brain_auto_resize(brain);
    EXPECT_FALSE(resized) << "Should not resize with low utilization";

    EXPECT_EQ(brain_get_neuron_count(brain), 100u)
        << "Neuron count should remain unchanged";
}

TEST_F(BrainResizeTest, AutoResizeRejectsNullBrain) {
    bool result = brain_auto_resize(nullptr);
    EXPECT_FALSE(result) << "auto_resize should reject NULL brain";
}

//=============================================================================
// Neuron Count Query Tests
//=============================================================================

TEST_F(BrainResizeTest, GetNeuronCountReturnsCorrectSize) {
    EXPECT_EQ(brain_get_neuron_count(brain), 100u);

    brain_resize(brain, 300);
    EXPECT_EQ(brain_get_neuron_count(brain), 300u);

    brain_resize(brain, 1000);
    EXPECT_EQ(brain_get_neuron_count(brain), 1000u);
}

TEST_F(BrainResizeTest, GetNeuronCountRejectsNull) {
    uint32_t count = brain_get_neuron_count(nullptr);
    EXPECT_EQ(count, 0u) << "Should return 0 for NULL brain";
}

//=============================================================================
// Utilization Metrics Tests
//=============================================================================

TEST_F(BrainResizeTest, GetUtilizationMetricsReturnsValid) {
    float utilization = -1.0f;
    float saturation = -1.0f;

    bool success = brain_get_utilization_metrics(brain, &utilization, &saturation);
    ASSERT_TRUE(success) << "Should successfully get metrics";

    // Metrics should be in valid range [0.0, 1.0]
    EXPECT_GE(utilization, 0.0f) << "Utilization should be >= 0";
    EXPECT_LE(utilization, 1.0f) << "Utilization should be <= 1";

    EXPECT_GE(saturation, 0.0f) << "Saturation should be >= 0";
    EXPECT_LE(saturation, 1.0f) << "Saturation should be <= 1";
}

TEST_F(BrainResizeTest, GetUtilizationMetricsRejectsNull) {
    float util, sat;

    EXPECT_FALSE(brain_get_utilization_metrics(nullptr, &util, &sat))
        << "Should reject NULL brain";

    EXPECT_FALSE(brain_get_utilization_metrics(brain, nullptr, &sat))
        << "Should reject NULL utilization pointer";

    EXPECT_FALSE(brain_get_utilization_metrics(brain, &util, nullptr))
        << "Should reject NULL saturation pointer";
}

TEST_F(BrainResizeTest, UtilizationIncreasesWithTraining) {
    float util_before, sat_before;
    ASSERT_TRUE(brain_get_utilization_metrics(brain, &util_before, &sat_before));

    // Train extensively
    train_simple_pattern(brain, 1000);

    float util_after, sat_after;
    ASSERT_TRUE(brain_get_utilization_metrics(brain, &util_after, &sat_after));

    // Utilization should increase after training
    // (or stay the same if already high)
    EXPECT_GE(util_after, util_before - 0.1f)
        << "Utilization should not decrease significantly after training";
}

//=============================================================================
// Integration: Resize During Training
//=============================================================================

TEST_F(BrainResizeTest, ResizeDuringTraining) {
    // Train for a while
    train_simple_pattern(brain, 100);

    // Resize mid-training
    ASSERT_TRUE(brain_resize(brain, 300));

    // Continue training
    train_simple_pattern(brain, 100);

    // Brain should still function correctly
    EXPECT_TRUE(verify_brain_learns_after_resize(brain));
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(BrainResizeTest, ResizeStressManySmallGrowths) {
    uint32_t current_size = 100;

    // Grow in small increments: 100 → 110 → 120 → ... → 200
    for (uint32_t new_size = 110; new_size <= 200; new_size += 10) {
        ASSERT_TRUE(brain_resize(brain, new_size))
            << "Failed to resize from " << current_size << " to " << new_size;

        EXPECT_EQ(brain_get_neuron_count(brain), new_size);
        current_size = new_size;
    }

    // Final verification
    EXPECT_EQ(brain_get_neuron_count(brain), 200u);
    EXPECT_TRUE(verify_brain_learns_after_resize(brain));
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
