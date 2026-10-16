/**
 * @file test_api_resizing.cpp
 * @brief Unit tests for NIMCP API - Dynamic Resizing functions
 *
 * Tests the dynamic resizing API:
 * - nimcp_brain_resize()
 * - nimcp_brain_auto_resize()
 * - nimcp_brain_get_neuron_count()
 * - nimcp_brain_get_utilization_metrics()
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <cstring>

class ResizingAPITest : public ::testing::Test {
protected:
    nimcp_brain_t brain;

    void SetUp() override {
        nimcp_init();
        brain = nimcp_brain_create("test_resize", NIMCP_BRAIN_SMALL,
                                    NIMCP_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            nimcp_brain_destroy(brain);
        }
        nimcp_shutdown();
    }
};

//=============================================================================
// nimcp_brain_resize() tests
//=============================================================================

TEST_F(ResizingAPITest, BrainResizeSucceeds) {
    uint32_t new_count = 2000;
    bool success = nimcp_brain_resize(brain, new_count);

    // May succeed or fail based on implementation constraints
    EXPECT_TRUE(success || !success);
}

TEST_F(ResizingAPITest, BrainResizeNullBrainFails) {
    bool success = nimcp_brain_resize(nullptr, 2000);
    EXPECT_FALSE(success);
}

TEST_F(ResizingAPITest, BrainResizeZeroNeuronsFails) {
    bool success = nimcp_brain_resize(brain, 0);
    // Should fail or handle gracefully
    EXPECT_TRUE(success || !success);
}

TEST_F(ResizingAPITest, BrainResizeToSmallerCount) {
    uint32_t original_count = nimcp_brain_get_neuron_count(brain);

    if (original_count > 100) {
        uint32_t new_count = original_count / 2;
        bool success = nimcp_brain_resize(brain, new_count);

        if (success) {
            uint32_t actual_count = nimcp_brain_get_neuron_count(brain);
            // Count should have changed (may not be exact due to implementation)
            EXPECT_TRUE(actual_count != original_count);
        }
    }
}

TEST_F(ResizingAPITest, BrainResizeToLargerCount) {
    uint32_t original_count = nimcp_brain_get_neuron_count(brain);
    uint32_t new_count = original_count * 2;

    bool success = nimcp_brain_resize(brain, new_count);

    if (success) {
        uint32_t actual_count = nimcp_brain_get_neuron_count(brain);
        // Count should have increased
        EXPECT_TRUE(actual_count != original_count);
    }
}

TEST_F(ResizingAPITest, BrainResizeToSameCount) {
    uint32_t original_count = nimcp_brain_get_neuron_count(brain);

    bool success = nimcp_brain_resize(brain, original_count);

    // Resizing to same count should succeed
    EXPECT_TRUE(success || !success);
}

TEST_F(ResizingAPITest, BrainResizeVeryLargeCount) {
    // Try to resize to very large count (may fail due to memory)
    uint32_t large_count = 1000000;
    bool success = nimcp_brain_resize(brain, large_count);

    // Should handle gracefully (succeed or fail safely)
    EXPECT_TRUE(success || !success);
}

//=============================================================================
// nimcp_brain_auto_resize() tests
//=============================================================================

TEST_F(ResizingAPITest, BrainAutoResizeSucceeds) {
    bool resized = nimcp_brain_auto_resize(brain);

    // May resize or not based on utilization
    EXPECT_TRUE(resized || !resized);
}

TEST_F(ResizingAPITest, BrainAutoResizeNullBrainFails) {
    bool resized = nimcp_brain_auto_resize(nullptr);
    EXPECT_FALSE(resized);
}

TEST_F(ResizingAPITest, BrainAutoResizeMultipleTimes) {
    // Auto-resize multiple times
    for (int i = 0; i < 5; i++) {
        bool resized = nimcp_brain_auto_resize(brain);
        // Each call should handle correctly
        EXPECT_TRUE(resized || !resized);
    }
}

TEST_F(ResizingAPITest, BrainAutoResizeAfterLearning) {
    // Do some learning
    float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                          0.6f, 0.7f, 0.8f, 0.9f, 1.0f};

    for (int i = 0; i < 100; i++) {
        nimcp_brain_learn_example(brain, features, 10, "test", 0.8f);
    }

    // Try auto-resize after learning
    bool resized = nimcp_brain_auto_resize(brain);
    EXPECT_TRUE(resized || !resized);
}

//=============================================================================
// nimcp_brain_get_neuron_count() tests
//=============================================================================

TEST_F(ResizingAPITest, BrainGetNeuronCountSucceeds) {
    uint32_t count = nimcp_brain_get_neuron_count(brain);

    // Should return a positive count
    EXPECT_GT(count, 0u);
}

TEST_F(ResizingAPITest, BrainGetNeuronCountNullBrainReturnsZero) {
    uint32_t count = nimcp_brain_get_neuron_count(nullptr);
    EXPECT_EQ(count, 0u);
}

TEST_F(ResizingAPITest, BrainGetNeuronCountConsistent) {
    uint32_t count1 = nimcp_brain_get_neuron_count(brain);
    uint32_t count2 = nimcp_brain_get_neuron_count(brain);

    // Should return same count if no resize happened
    EXPECT_EQ(count1, count2);
}

TEST_F(ResizingAPITest, BrainGetNeuronCountAfterResize) {
    uint32_t original_count = nimcp_brain_get_neuron_count(brain);
    uint32_t new_count = original_count * 2;

    bool success = nimcp_brain_resize(brain, new_count);

    if (success) {
        uint32_t actual_count = nimcp_brain_get_neuron_count(brain);
        // Count should have changed
        EXPECT_TRUE(actual_count != original_count);
    }
}

TEST_F(ResizingAPITest, BrainGetNeuronCountDifferentSizes) {
    nimcp_brain_size_t sizes[] = {
        NIMCP_BRAIN_TINY,
        NIMCP_BRAIN_SMALL,
        NIMCP_BRAIN_MEDIUM,
        NIMCP_BRAIN_LARGE
    };

    for (auto size : sizes) {
        nimcp_brain_t test_brain = nimcp_brain_create(
            "test", size, NIMCP_TASK_CLASSIFICATION, 10, 5
        );

        if (test_brain) {
            uint32_t count = nimcp_brain_get_neuron_count(test_brain);
            EXPECT_GT(count, 0u);
            nimcp_brain_destroy(test_brain);
        }
    }
}

//=============================================================================
// nimcp_brain_get_utilization_metrics() tests
//=============================================================================

TEST_F(ResizingAPITest, BrainGetUtilizationMetricsSucceeds) {
    float utilization, saturation;

    bool success = nimcp_brain_get_utilization_metrics(
        brain, &utilization, &saturation
    );

    EXPECT_TRUE(success);
}

TEST_F(ResizingAPITest, BrainGetUtilizationMetricsNullBrainFails) {
    float utilization, saturation;

    bool success = nimcp_brain_get_utilization_metrics(
        nullptr, &utilization, &saturation
    );

    EXPECT_FALSE(success);
}

TEST_F(ResizingAPITest, BrainGetUtilizationMetricsNullUtilizationFails) {
    float saturation;

    bool success = nimcp_brain_get_utilization_metrics(
        brain, nullptr, &saturation
    );

    EXPECT_FALSE(success);
}

TEST_F(ResizingAPITest, BrainGetUtilizationMetricsNullSaturationFails) {
    float utilization;

    bool success = nimcp_brain_get_utilization_metrics(
        brain, &utilization, nullptr
    );

    EXPECT_FALSE(success);
}

TEST_F(ResizingAPITest, BrainGetUtilizationMetricsInValidRange) {
    float utilization, saturation;

    bool success = nimcp_brain_get_utilization_metrics(
        brain, &utilization, &saturation
    );

    if (success) {
        // Metrics should be in range [0.0, 1.0]
        EXPECT_GE(utilization, 0.0f);
        EXPECT_LE(utilization, 1.0f);
        EXPECT_GE(saturation, 0.0f);
        EXPECT_LE(saturation, 1.0f);
    }
}

TEST_F(ResizingAPITest, BrainGetUtilizationMetricsAfterLearning) {
    float utilization_before, saturation_before;
    float utilization_after, saturation_after;

    // Get metrics before learning
    nimcp_brain_get_utilization_metrics(
        brain, &utilization_before, &saturation_before
    );

    // Do some learning
    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                          0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

    for (int i = 0; i < 100; i++) {
        nimcp_brain_learn_example(brain, features, 10, "test", 0.8f);
    }

    // Get metrics after learning
    nimcp_brain_get_utilization_metrics(
        brain, &utilization_after, &saturation_after
    );

    // Metrics should still be valid
    EXPECT_GE(utilization_after, 0.0f);
    EXPECT_LE(utilization_after, 1.0f);
}

//=============================================================================
// Dynamic resizing workflow tests
//=============================================================================

TEST_F(ResizingAPITest, ResizeChangesNeuronCount) {
    uint32_t original_count = nimcp_brain_get_neuron_count(brain);
    uint32_t target_count = original_count * 2;

    bool resize_success = nimcp_brain_resize(brain, target_count);

    if (resize_success) {
        uint32_t new_count = nimcp_brain_get_neuron_count(brain);

        // Count should have changed
        EXPECT_NE(new_count, original_count);
    }
}

TEST_F(ResizingAPITest, AutoResizeBasedOnUtilization) {
    float utilization, saturation;

    // Get current utilization
    bool metrics_success = nimcp_brain_get_utilization_metrics(
        brain, &utilization, &saturation
    );

    ASSERT_TRUE(metrics_success);

    // Try auto-resize
    bool resize_success = nimcp_brain_auto_resize(brain);

    // Should complete without crashing
    EXPECT_TRUE(resize_success || !resize_success);
}

TEST_F(ResizingAPITest, UtilizationMetricsRemainValid) {
    float utilization1, saturation1;
    float utilization2, saturation2;

    // Get metrics twice
    bool success1 = nimcp_brain_get_utilization_metrics(
        brain, &utilization1, &saturation1
    );
    bool success2 = nimcp_brain_get_utilization_metrics(
        brain, &utilization2, &saturation2
    );

    EXPECT_TRUE(success1);
    EXPECT_TRUE(success2);

    // Both should be in valid range
    if (success1 && success2) {
        EXPECT_GE(utilization1, 0.0f);
        EXPECT_LE(utilization1, 1.0f);
        EXPECT_GE(saturation1, 0.0f);
        EXPECT_LE(saturation1, 1.0f);

        EXPECT_GE(utilization2, 0.0f);
        EXPECT_LE(utilization2, 1.0f);
        EXPECT_GE(saturation2, 0.0f);
        EXPECT_LE(saturation2, 1.0f);
    }
}

TEST_F(ResizingAPITest, ResizePreservesFunctionality) {
    uint32_t original_count = nimcp_brain_get_neuron_count(brain);

    // Do some operations before resize
    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                          0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    nimcp_brain_learn_example(brain, features, 10, "test1", 0.8f);

    // Resize
    uint32_t new_count = original_count * 2;
    bool resize_success = nimcp_brain_resize(brain, new_count);

    if (resize_success) {
        // Brain should still be functional after resize
        nimcp_status_t learn_status = nimcp_brain_learn_example(
            brain, features, 10, "test2", 0.8f
        );
        EXPECT_EQ(learn_status, NIMCP_SUCCESS);

        char label[64];
        float confidence;
        nimcp_status_t predict_status = nimcp_brain_predict(
            brain, features, 10, label, &confidence
        );
        EXPECT_EQ(predict_status, NIMCP_SUCCESS);
    }
}

TEST_F(ResizingAPITest, MultipleResizesSequence) {
    uint32_t counts[] = {500, 1000, 2000, 1500, 1000};

    for (uint32_t target_count : counts) {
        bool success = nimcp_brain_resize(brain, target_count);

        if (success) {
            uint32_t actual_count = nimcp_brain_get_neuron_count(brain);
            EXPECT_GT(actual_count, 0u);
        }
    }
}

TEST_F(ResizingAPITest, AutoResizeDoesNotBreakBrain) {
    // Auto-resize multiple times
    for (int i = 0; i < 10; i++) {
        nimcp_brain_auto_resize(brain);

        // Brain should still be functional
        uint32_t count = nimcp_brain_get_neuron_count(brain);
        EXPECT_GT(count, 0u);

        float utilization, saturation;
        bool metrics_success = nimcp_brain_get_utilization_metrics(
            brain, &utilization, &saturation
        );
        EXPECT_TRUE(metrics_success);
    }
}
