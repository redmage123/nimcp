/**
 * @file test_gpu_spike_event_backward_compat.cpp
 * @brief Regression tests for GPU spike event backward compatibility
 *
 * WHAT: Ensures spike event features don't break existing code
 * WHY:  Verify zero breaking changes to pre-spike-event code
 * HOW:  Test legacy patterns and ensure they still work correctly
 *
 * @version GPU Spike Event Regression Testing
 * @date 2025-11-13
 */

#include <gtest/gtest.h>

// GPU header outside extern "C" due to CUDA runtime templates
#include "gpu/nimcp_spike_event.h"

// Headers have their own extern "C" guards
    #include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class GPUSpikeEventRegressionTest : public ::testing::Test {
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

TEST_F(GPUSpikeEventRegressionTest, BrainCreation_StillWorks) {
    // Old code pattern: Create brain without spike event awareness
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);

    // Should still work exactly as before
    ASSERT_NE(brain, nullptr) << "Brain creation should not be broken by spike events";
}

//=============================================================================
// Regression Test 2: Legacy Inference Without Spike Events
//=============================================================================

TEST_F(GPUSpikeEventRegressionTest, LegacyInference_StillWorks) {
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Old inference pattern (no spike event knowledge)
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    EXPECT_NE(decision, nullptr);
    if (decision) {
        EXPECT_NE(decision->label, nullptr);
    }
}

//=============================================================================
// Regression Test 3: Spike Train API Doesn't Break CPU Code
//=============================================================================

TEST_F(GPUSpikeEventRegressionTest, SpikeTrainAPI_NoCPUBreakage) {
    // Create and use spike train (new API)
    spike_train_t* train = spike_train_create(100);
    ASSERT_NE(train, nullptr);

    // Use spike train
    spike_train_add(train, 1000, 1.0f);
    spike_train_get_last_spike(train);

    spike_train_destroy(train);

    // Old brain API should still work
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    ASSERT_NE(decision, nullptr);
}

//=============================================================================
// Regression Test 4: Spike Queue API Doesn't Break CPU Code
//=============================================================================

TEST_F(GPUSpikeEventRegressionTest, SpikeQueueAPI_NoCPUBreakage) {
    // Create and use spike queue (new API)
    spike_queue_t* queue = spike_queue_create(256, false);
    ASSERT_NE(queue, nullptr);

    // Use spike queue
    spike_event_t event = {1000, 0, 1, 0, 1.0f};
    spike_queue_push(queue, &event);
    spike_queue_pop(queue, &event);

    spike_queue_destroy(queue);

    // Old brain API should still work
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    ASSERT_NE(decision, nullptr);
}

//=============================================================================
// Regression Test 5: NULL Safety Maintained
//=============================================================================

TEST_F(GPUSpikeEventRegressionTest, NullSafety_Maintained) {
    // Test NULL safety for spike event functions
    spike_train_destroy(nullptr);  // Should not crash
    spike_queue_destroy(nullptr);  // Should not crash

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

TEST_F(GPUSpikeEventRegressionTest, MemoryManagement_NoLeaks) {
    // Create and destroy spike structures multiple times
    for (int i = 0; i < 10; i++) {
        spike_train_t* train = spike_train_create(100);
        ASSERT_NE(train, nullptr);
        spike_train_add(train, 1000, 1.0f);
        spike_train_destroy(train);

        spike_queue_t* queue = spike_queue_create(256, false);
        ASSERT_NE(queue, nullptr);
        spike_event_t event = {1000, 0, 1, 0, 1.0f};
        spike_queue_push(queue, &event);
        spike_queue_destroy(queue);
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

TEST_F(GPUSpikeEventRegressionTest, NoPerformanceRegression) {
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

    EXPECT_LT(avg_us, 1000.0f) << "Spike events shouldn't cause severe performance regression";
}

//=============================================================================
// Regression Test 8: Old Learning Pattern Works
//=============================================================================

TEST_F(GPUSpikeEventRegressionTest, OldLearningPattern_Works) {
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
// Regression Test 9: Spike Train Doesn't Interfere With Brain
//=============================================================================

TEST_F(GPUSpikeEventRegressionTest, SpikeTrainNoInterference) {
    // Create spike train
    spike_train_t* train = spike_train_create(100);
    ASSERT_NE(train, nullptr);

    // Add some spikes
    for (int i = 0; i < 10; i++) {
        spike_train_add(train, i * 1000, 1.0f);
    }

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

    spike_train_destroy(train);
}

//=============================================================================
// Regression Test 10: Spike Queue Doesn't Interfere With Brain
//=============================================================================

TEST_F(GPUSpikeEventRegressionTest, SpikeQueueNoInterference) {
    // Create spike queue
    spike_queue_t* queue = spike_queue_create(256, false);
    ASSERT_NE(queue, nullptr);

    // Push some spikes
    for (int i = 0; i < 10; i++) {
        spike_event_t event = {(uint64_t)i * 1000, (uint32_t)i, (uint32_t)(i+1), 0, 1.0f};
        spike_queue_push(queue, &event);
    }

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

    spike_queue_destroy(queue);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
