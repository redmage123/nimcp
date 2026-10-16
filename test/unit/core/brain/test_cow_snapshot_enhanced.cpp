//=============================================================================
// test_cow_snapshot_enhanced.cpp - Enhanced COW Clone Unit Tests
//=============================================================================
/**
 * @file test_cow_snapshot_enhanced.cpp
 * @brief Comprehensive unit tests for COW clone functionality
 *
 * WHAT: Tests for copy-on-write brain cloning
 * WHY:  Verify COW clone isolation and memory efficiency
 * HOW:  GoogleTest framework with detailed assertions
 *
 * TEST COVERAGE:
 * 1. Clone creation and isolation
 * 2. Memory sharing via COW
 * 3. Multiple clone scenarios
 * 4. Error handling and edge cases
 */

#include <gtest/gtest.h>
#include "core/brain/nimcp_brain.h"
#include <cstring>

class COWSnapshotEnhancedTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test brain
        brain = brain_create("test_cow_brain", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
        }
    }

    brain_t brain = nullptr;
};

//=============================================================================
// Test 1: COW Clone Creation
//=============================================================================

TEST_F(COWSnapshotEnhancedTest, CloneCreation) {
    // WHAT: Verify COW clone creation works
    // WHY:  Ensure basic cloning functionality
    // HOW:  Create clone and verify it exists

    // Create COW clone
    brain_t clone = brain_clone_cow(brain);
    ASSERT_NE(clone, nullptr);

    // Cleanup
    brain_destroy(clone);
}

//=============================================================================
// Test 2: COW Memory Sharing
//=============================================================================

TEST_F(COWSnapshotEnhancedTest, COWMemorySharing) {
    // WHAT: Verify COW clones share memory
    // WHY:  Ensure memory efficiency via COW
    // HOW:  Check COW statistics

    // Create clone
    brain_t clone = brain_clone_cow(brain);
    ASSERT_NE(clone, nullptr);

    // Get COW stats
    brain_cow_stats_t cow_stats;
    if (brain_get_cow_stats(clone, &cow_stats)) {
        // VERIFY: Clone is marked as COW
        EXPECT_TRUE(cow_stats.is_cow_clone);

        // VERIFY: Reference count increased
        EXPECT_GE(cow_stats.cow_ref_count, 1);
    }

    // Cleanup
    brain_destroy(clone);
}

//=============================================================================
// Test 3: Clone Isolation
//=============================================================================

TEST_F(COWSnapshotEnhancedTest, CloneIsolation) {
    // WHAT: Verify clone is isolated from original brain modifications
    // WHY:  Ensure COW semantics work correctly
    // HOW:  Modify original after clone and verify clone unchanged

    // Train original brain
    float inputs[10] = {1.0f, 0.5f, 0.3f, 0.8f, 0.2f, 0.6f, 0.4f, 0.9f, 0.1f, 0.7f};
    brain_learn_example(brain, inputs, 10, "class_A", 0.8f);

    // Create clone
    brain_t clone = brain_clone_cow(brain);
    ASSERT_NE(clone, nullptr);

    // Get decision from clone
    brain_decision_t* clone_decision = brain_decide(clone, inputs, 10);
    ASSERT_NE(clone_decision, nullptr);

    // Store result
    char first_label[128];
    strncpy(first_label, clone_decision->label, sizeof(first_label) - 1);
    first_label[sizeof(first_label) - 1] = '\0';
    brain_free_decision(clone_decision);

    // Train original brain on different data
    float new_inputs[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    for (int i = 0; i < 100; i++) {
        brain_learn_example(brain, new_inputs, 10, "class_B", 0.8f);
    }

    // Get decision from clone again
    brain_decision_t* clone_decision2 = brain_decide(clone, inputs, 10);
    ASSERT_NE(clone_decision2, nullptr);

    // VERIFY: Clone output unchanged despite original brain training
    EXPECT_STREQ(first_label, clone_decision2->label);

    // Cleanup
    brain_free_decision(clone_decision2);
    brain_destroy(clone);
}

//=============================================================================
// Test 4: Multiple Clones Share Data
//=============================================================================

TEST_F(COWSnapshotEnhancedTest, MultipleClonesShareData) {
    // WHAT: Verify multiple clones share underlying network data
    // WHY:  Ensure memory efficiency with multiple clones
    // HOW:  Create multiple clones and check stats

    // Create first clone
    brain_t clone1 = brain_clone_cow(brain);
    ASSERT_NE(clone1, nullptr);

    // Create second clone
    brain_t clone2 = brain_clone_cow(brain);
    ASSERT_NE(clone2, nullptr);

    // Create third clone
    brain_t clone3 = brain_clone_cow(brain);
    ASSERT_NE(clone3, nullptr);

    // Get COW stats from one clone
    brain_cow_stats_t cow_stats;
    if (brain_get_cow_stats(clone1, &cow_stats)) {
        // VERIFY: Reference count reflects multiple clones
        EXPECT_GE(cow_stats.cow_ref_count, 2);
    }

    // Cleanup
    brain_destroy(clone1);
    brain_destroy(clone2);
    brain_destroy(clone3);
}

//=============================================================================
// Test 5: Error Handling - NULL Brain
//=============================================================================

TEST_F(COWSnapshotEnhancedTest, ErrorHandlingNullBrain) {
    // WHAT: Verify proper error handling for NULL brain
    // WHY:  Ensure robustness against invalid inputs
    // HOW:  Pass NULL and check for NULL return

    brain_t clone = brain_clone_cow(nullptr);
    EXPECT_EQ(clone, nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
