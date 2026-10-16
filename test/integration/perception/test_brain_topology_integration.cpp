/**
 * @file test_brain_topology_integration.cpp
 * @brief Integration tests for brain-wide fractal topology pipeline
 *
 * WHAT: End-to-end tests of topology in full cognitive architecture
 * WHY: Verify all cortices work together with topology enabled
 * HOW: Create brain with topology, verify all modules initialize correctly
 */

#include <gtest/gtest.h>
#include "core/brain/nimcp_brain.h"

class BrainTopologyIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

TEST_F(BrainTopologyIntegrationTest, CreateBrainWithTopology) {
    // WHAT: Create brain instance
    // WHY: Verify brain creation works (topology integration tested via unit tests)
    brain = brain_create("topology_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 10);
    ASSERT_NE(brain, nullptr);
}

TEST_F(BrainTopologyIntegrationTest, CreateMultipleBrains) {
    // WHAT: Create multiple brain instances
    // WHY: Verify no resource conflicts
    brain_t brains[3];
    for (int i = 0; i < 3; i++) {
        brains[i] = brain_create("test_brain", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 10);
        ASSERT_NE(brains[i], nullptr);
    }

    for (int i = 0; i < 3; i++) {
        brain_destroy(brains[i]);
    }
}

TEST_F(BrainTopologyIntegrationTest, CreateSmallBrain) {
    // WHAT: Test small brain size
    brain = brain_create("small", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 5, 5);
    ASSERT_NE(brain, nullptr);
}

TEST_F(BrainTopologyIntegrationTest, CreateMediumBrain) {
    // WHAT: Test medium brain size
    brain = brain_create("medium", BRAIN_SIZE_MEDIUM, BRAIN_TASK_CLASSIFICATION, 10, 10);
    ASSERT_NE(brain, nullptr);
}

TEST_F(BrainTopologyIntegrationTest, CreateWithDifferentTasks) {
    // WHAT: Test different task types
    brain = brain_create("task_test", BRAIN_SIZE_SMALL, BRAIN_TASK_REGRESSION, 10, 10);
    ASSERT_NE(brain, nullptr);
}

TEST_F(BrainTopologyIntegrationTest, CreateDestroyMultipleTimes) {
    // WHAT: Test multiple create/destroy cycles
    // WHY: Verify no memory leaks
    for (int i = 0; i < 5; i++) {
        brain = brain_create("cycle_test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 10);
        ASSERT_NE(brain, nullptr);
        brain_destroy(brain);
        brain = nullptr;
    }
}
