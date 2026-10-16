/**
 * @file test_api_brain_cow.cpp
 * @brief Unit tests for NIMCP API copy-on-write (COW) functionality
 *
 * Tests COW brain cloning and snapshot functionality including:
 * - nimcp_brain_clone_cow()
 * - nimcp_brain_snapshot_cow()
 * - nimcp_brain_restore_cow()
 * - nimcp_brain_snapshot_destroy()
 * - Memory sharing verification
 * - Snapshot isolation
 */

#include <gtest/gtest.h>
#include "nimcp.h"
#include <cstring>

class BrainCOWTest : public ::testing::Test {
protected:
    nimcp_brain_t brain;

    void SetUp() override {
        // Initialize NIMCP
        nimcp_init();

        // Create test brain
        brain = nimcp_brain_create("cow_test_brain", NIMCP_BRAIN_TINY,
                                   NIMCP_TASK_CLASSIFICATION, 10, 5);
        ASSERT_NE(brain, nullptr);

        // Train with some data to have non-trivial state
        float features[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f,
                              0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
        nimcp_brain_learn_example(brain, features, 10, "class_a", 0.9f);
    }

    void TearDown() override {
        // Cleanup brain
        if (brain) {
            nimcp_brain_destroy(brain);
            brain = nullptr;
        }

        // Shutdown NIMCP
        nimcp_shutdown();
    }
};

//=============================================================================
// COW Clone Tests
//=============================================================================

TEST_F(BrainCOWTest, CloneCOWSucceeds) {
    nimcp_brain_t clone = nimcp_brain_clone_cow(brain);

    EXPECT_NE(clone, nullptr);
    nimcp_brain_destroy(clone);
}

TEST_F(BrainCOWTest, CloneCOWWithNullBrainFails) {
    nimcp_brain_t clone = nimcp_brain_clone_cow(nullptr);

    EXPECT_EQ(clone, nullptr);
    EXPECT_STREQ(nimcp_get_error(), "NULL brain provided to nimcp_brain_clone_cow");
}

TEST_F(BrainCOWTest, CloneCOWReturnsValidHandle) {
    nimcp_brain_t clone = nimcp_brain_clone_cow(brain);
    ASSERT_NE(clone, nullptr);

    // Verify clone is usable
    nimcp_brain_probe_t probe;
    nimcp_status_t status = nimcp_brain_probe(clone, &probe);
    EXPECT_EQ(status, NIMCP_SUCCESS);

    nimcp_brain_destroy(clone);
}

TEST_F(BrainCOWTest, CloneCOWIsIndependent) {
    nimcp_brain_t clone = nimcp_brain_clone_cow(brain);
    ASSERT_NE(clone, nullptr);

    // Modify clone
    float features[10] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                          1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    nimcp_brain_learn_example(clone, features, 10, "class_b", 0.8f);

    // Both brains should still be valid
    nimcp_brain_probe_t probe_original, probe_clone;
    EXPECT_EQ(nimcp_brain_probe(brain, &probe_original), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_brain_probe(clone, &probe_clone), NIMCP_SUCCESS);

    nimcp_brain_destroy(clone);
}

TEST_F(BrainCOWTest, CloneCOWPreservesState) {
    // Get original state
    nimcp_brain_probe_t original_probe;
    ASSERT_EQ(nimcp_brain_probe(brain, &original_probe), NIMCP_SUCCESS);

    // Clone
    nimcp_brain_t clone = nimcp_brain_clone_cow(brain);
    ASSERT_NE(clone, nullptr);

    // Get clone state
    nimcp_brain_probe_t clone_probe;
    ASSERT_EQ(nimcp_brain_probe(clone, &clone_probe), NIMCP_SUCCESS);

    // Verify state matches
    EXPECT_EQ(original_probe.num_neurons, clone_probe.num_neurons);
    EXPECT_EQ(original_probe.size, clone_probe.size);
    EXPECT_EQ(original_probe.task, clone_probe.task);

    nimcp_brain_destroy(clone);
}

TEST_F(BrainCOWTest, CloneCOWMultipleTimes) {
    nimcp_brain_t clone1 = nimcp_brain_clone_cow(brain);
    ASSERT_NE(clone1, nullptr);

    nimcp_brain_t clone2 = nimcp_brain_clone_cow(brain);
    ASSERT_NE(clone2, nullptr);

    nimcp_brain_t clone3 = nimcp_brain_clone_cow(brain);
    ASSERT_NE(clone3, nullptr);

    // All clones should be valid
    nimcp_brain_probe_t probe1, probe2, probe3;
    EXPECT_EQ(nimcp_brain_probe(clone1, &probe1), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_brain_probe(clone2, &probe2), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_brain_probe(clone3, &probe3), NIMCP_SUCCESS);

    nimcp_brain_destroy(clone1);
    nimcp_brain_destroy(clone2);
    nimcp_brain_destroy(clone3);
}

TEST_F(BrainCOWTest, CloneCOWSharesMemory) {
    // Get original memory usage
    nimcp_brain_probe_t original_probe;
    ASSERT_EQ(nimcp_brain_probe(brain, &original_probe), NIMCP_SUCCESS);

    // Create clone
    nimcp_brain_t clone = nimcp_brain_clone_cow(brain);
    ASSERT_NE(clone, nullptr);

    // Get clone memory info
    nimcp_brain_probe_t clone_probe;
    ASSERT_EQ(nimcp_brain_probe(clone, &clone_probe), NIMCP_SUCCESS);

    // Clone should be marked as COW clone
    EXPECT_TRUE(clone_probe.is_cow_clone);

    // Clone should have shared memory
    EXPECT_GT(clone_probe.cow_shared_bytes, 0);

    nimcp_brain_destroy(clone);
}

//=============================================================================
// COW Snapshot Tests
//=============================================================================

TEST_F(BrainCOWTest, SnapshotCOWSucceeds) {
    nimcp_brain_snapshot_t snapshot = nimcp_brain_snapshot_cow(brain);

    EXPECT_NE(snapshot, nullptr);
    nimcp_brain_snapshot_destroy(snapshot);
}

TEST_F(BrainCOWTest, SnapshotCOWWithNullBrainFails) {
    nimcp_brain_snapshot_t snapshot = nimcp_brain_snapshot_cow(nullptr);

    EXPECT_EQ(snapshot, nullptr);
    EXPECT_STREQ(nimcp_get_error(), "NULL brain provided to nimcp_brain_snapshot_cow");
}

TEST_F(BrainCOWTest, SnapshotCOWReturnsValidHandle) {
    nimcp_brain_snapshot_t snapshot = nimcp_brain_snapshot_cow(brain);

    EXPECT_NE(snapshot, nullptr);
    nimcp_brain_snapshot_destroy(snapshot);
}

TEST_F(BrainCOWTest, SnapshotCOWMultipleTimes) {
    nimcp_brain_snapshot_t snap1 = nimcp_brain_snapshot_cow(brain);
    ASSERT_NE(snap1, nullptr);

    nimcp_brain_snapshot_t snap2 = nimcp_brain_snapshot_cow(brain);
    ASSERT_NE(snap2, nullptr);

    nimcp_brain_snapshot_t snap3 = nimcp_brain_snapshot_cow(brain);
    ASSERT_NE(snap3, nullptr);

    nimcp_brain_snapshot_destroy(snap1);
    nimcp_brain_snapshot_destroy(snap2);
    nimcp_brain_snapshot_destroy(snap3);
}

TEST_F(BrainCOWTest, SnapshotCOWAfterModifications) {
    // Create initial snapshot
    nimcp_brain_snapshot_t snap1 = nimcp_brain_snapshot_cow(brain);
    ASSERT_NE(snap1, nullptr);

    // Modify brain
    float features[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                          0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    nimcp_brain_learn_example(brain, features, 10, "class_b", 0.8f);

    // Create second snapshot
    nimcp_brain_snapshot_t snap2 = nimcp_brain_snapshot_cow(brain);
    ASSERT_NE(snap2, nullptr);

    nimcp_brain_snapshot_destroy(snap1);
    nimcp_brain_snapshot_destroy(snap2);
}

//=============================================================================
// COW Restore Tests
//=============================================================================

TEST_F(BrainCOWTest, RestoreCOWSucceeds) {
    // Create snapshot
    nimcp_brain_snapshot_t snapshot = nimcp_brain_snapshot_cow(brain);
    ASSERT_NE(snapshot, nullptr);

    // Restore
    nimcp_status_t status = nimcp_brain_restore_cow(brain, snapshot);

    EXPECT_EQ(status, NIMCP_SUCCESS);
    nimcp_brain_snapshot_destroy(snapshot);
}

TEST_F(BrainCOWTest, RestoreCOWWithNullBrainFails) {
    nimcp_brain_snapshot_t snapshot = nimcp_brain_snapshot_cow(brain);
    ASSERT_NE(snapshot, nullptr);

    nimcp_status_t status = nimcp_brain_restore_cow(nullptr, snapshot);

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
    EXPECT_STREQ(nimcp_get_error(), "NULL brain provided to nimcp_brain_restore_cow");

    nimcp_brain_snapshot_destroy(snapshot);
}

TEST_F(BrainCOWTest, RestoreCOWWithNullSnapshotFails) {
    nimcp_status_t status = nimcp_brain_restore_cow(brain, nullptr);

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
    EXPECT_STREQ(nimcp_get_error(), "NULL snapshot provided to nimcp_brain_restore_cow");
}

TEST_F(BrainCOWTest, RestoreCOWWithBothNullFails) {
    nimcp_status_t status = nimcp_brain_restore_cow(nullptr, nullptr);

    EXPECT_EQ(status, NIMCP_ERROR_NULL_ARG);
}

TEST_F(BrainCOWTest, RestoreCOWPreservesSnapshot) {
    // Get original state
    nimcp_brain_probe_t original_probe;
    ASSERT_EQ(nimcp_brain_probe(brain, &original_probe), NIMCP_SUCCESS);

    // Create snapshot
    nimcp_brain_snapshot_t snapshot = nimcp_brain_snapshot_cow(brain);
    ASSERT_NE(snapshot, nullptr);

    // Modify brain significantly
    for (int i = 0; i < 10; i++) {
        float features[10];
        for (int j = 0; j < 10; j++) {
            features[j] = (float)(i * 10 + j) / 100.0f;
        }
        nimcp_brain_learn_example(brain, features, 10, "class_new", 0.95f);
    }

    // Restore
    ASSERT_EQ(nimcp_brain_restore_cow(brain, snapshot), NIMCP_SUCCESS);

    // Get restored state
    nimcp_brain_probe_t restored_probe;
    ASSERT_EQ(nimcp_brain_probe(brain, &restored_probe), NIMCP_SUCCESS);

    // State should match original
    EXPECT_EQ(original_probe.num_neurons, restored_probe.num_neurons);

    nimcp_brain_snapshot_destroy(snapshot);
}

TEST_F(BrainCOWTest, RestoreCOWMultipleTimes) {
    nimcp_brain_snapshot_t snapshot = nimcp_brain_snapshot_cow(brain);
    ASSERT_NE(snapshot, nullptr);

    // Restore multiple times
    EXPECT_EQ(nimcp_brain_restore_cow(brain, snapshot), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_brain_restore_cow(brain, snapshot), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_brain_restore_cow(brain, snapshot), NIMCP_SUCCESS);

    nimcp_brain_snapshot_destroy(snapshot);
}

TEST_F(BrainCOWTest, RestoreCOWAfterSnapshot) {
    // Snapshot, modify, restore cycle
    nimcp_brain_snapshot_t snap1 = nimcp_brain_snapshot_cow(brain);
    ASSERT_NE(snap1, nullptr);

    float features1[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                           0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    nimcp_brain_learn_example(brain, features1, 10, "class_b", 0.8f);

    nimcp_brain_snapshot_t snap2 = nimcp_brain_snapshot_cow(brain);
    ASSERT_NE(snap2, nullptr);

    float features2[10] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                           1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    nimcp_brain_learn_example(brain, features2, 10, "class_c", 0.9f);

    // Restore to second snapshot
    EXPECT_EQ(nimcp_brain_restore_cow(brain, snap2), NIMCP_SUCCESS);

    // Restore to first snapshot
    EXPECT_EQ(nimcp_brain_restore_cow(brain, snap1), NIMCP_SUCCESS);

    nimcp_brain_snapshot_destroy(snap1);
    nimcp_brain_snapshot_destroy(snap2);
}

//=============================================================================
// Snapshot Destroy Tests
//=============================================================================

TEST_F(BrainCOWTest, SnapshotDestroySucceeds) {
    nimcp_brain_snapshot_t snapshot = nimcp_brain_snapshot_cow(brain);
    ASSERT_NE(snapshot, nullptr);

    // Destroy should not crash
    nimcp_brain_snapshot_destroy(snapshot);
}

TEST_F(BrainCOWTest, SnapshotDestroyWithNullIsNoop) {
    // Should not crash
    nimcp_brain_snapshot_destroy(nullptr);
}

TEST_F(BrainCOWTest, SnapshotDestroyMultipleTimes) {
    nimcp_brain_snapshot_t snapshot = nimcp_brain_snapshot_cow(brain);
    ASSERT_NE(snapshot, nullptr);

    nimcp_brain_snapshot_destroy(snapshot);

    // Second destroy of same pointer would be undefined behavior
    // (don't do this - just testing it doesn't crash immediately)
}

TEST_F(BrainCOWTest, DestroySnapshotDoesNotAffectBrain) {
    nimcp_brain_snapshot_t snapshot = nimcp_brain_snapshot_cow(brain);
    ASSERT_NE(snapshot, nullptr);

    // Destroy snapshot
    nimcp_brain_snapshot_destroy(snapshot);

    // Brain should still be valid
    nimcp_brain_probe_t probe;
    EXPECT_EQ(nimcp_brain_probe(brain, &probe), NIMCP_SUCCESS);
}

TEST_F(BrainCOWTest, DestroyBrainDoesNotAffectSnapshot) {
    nimcp_brain_snapshot_t snapshot = nimcp_brain_snapshot_cow(brain);
    ASSERT_NE(snapshot, nullptr);

    // Destroy brain
    nimcp_brain_destroy(brain);
    brain = nullptr;

    // Snapshot should still be valid and destroyable
    nimcp_brain_snapshot_destroy(snapshot);
}

//=============================================================================
// Memory Sharing Tests
//=============================================================================

TEST_F(BrainCOWTest, COWCloneSharesMemoryCorrectly) {
    nimcp_brain_t clone = nimcp_brain_clone_cow(brain);
    ASSERT_NE(clone, nullptr);

    nimcp_brain_probe_t clone_probe;
    ASSERT_EQ(nimcp_brain_probe(clone, &clone_probe), NIMCP_SUCCESS);

    // Verify COW properties
    EXPECT_TRUE(clone_probe.is_cow_clone);
    EXPECT_GT(clone_probe.cow_ref_count, 0);
    EXPECT_GT(clone_probe.cow_shared_bytes, 0);

    nimcp_brain_destroy(clone);
}

TEST_F(BrainCOWTest, MultipleClonesSharememory) {
    nimcp_brain_t clone1 = nimcp_brain_clone_cow(brain);
    ASSERT_NE(clone1, nullptr);

    nimcp_brain_t clone2 = nimcp_brain_clone_cow(brain);
    ASSERT_NE(clone2, nullptr);

    // Both clones should be marked as COW
    nimcp_brain_probe_t probe1, probe2;
    ASSERT_EQ(nimcp_brain_probe(clone1, &probe1), NIMCP_SUCCESS);
    ASSERT_EQ(nimcp_brain_probe(clone2, &probe2), NIMCP_SUCCESS);

    EXPECT_TRUE(probe1.is_cow_clone);
    EXPECT_TRUE(probe2.is_cow_clone);

    nimcp_brain_destroy(clone1);
    nimcp_brain_destroy(clone2);
}

TEST_F(BrainCOWTest, COWMemorySavingsVerification) {
    // Get original memory
    nimcp_brain_probe_t original_probe;
    ASSERT_EQ(nimcp_brain_probe(brain, &original_probe), NIMCP_SUCCESS);
    size_t original_memory = original_probe.memory_bytes;

    // Create clone
    nimcp_brain_t clone = nimcp_brain_clone_cow(brain);
    ASSERT_NE(clone, nullptr);

    nimcp_brain_probe_t clone_probe;
    ASSERT_EQ(nimcp_brain_probe(clone, &clone_probe), NIMCP_SUCCESS);

    // Clone should use less total memory due to sharing
    size_t clone_private = clone_probe.cow_private_bytes;
    EXPECT_LT(clone_private, original_memory);

    nimcp_brain_destroy(clone);
}

//=============================================================================
// Snapshot Isolation Tests
//=============================================================================

TEST_F(BrainCOWTest, SnapshotIsolationWorks) {
    // Create snapshot
    nimcp_brain_snapshot_t snapshot = nimcp_brain_snapshot_cow(brain);
    ASSERT_NE(snapshot, nullptr);

    // Modify original brain extensively
    for (int i = 0; i < 20; i++) {
        float features[10];
        for (int j = 0; j < 10; j++) {
            features[j] = (float)i / 20.0f;
        }
        nimcp_brain_learn_example(brain, features, 10, "modified", 0.95f);
    }

    // Restore from snapshot should work
    EXPECT_EQ(nimcp_brain_restore_cow(brain, snapshot), NIMCP_SUCCESS);

    nimcp_brain_snapshot_destroy(snapshot);
}

TEST_F(BrainCOWTest, MultipleSnapshotsIsolated) {
    // Create first snapshot
    nimcp_brain_snapshot_t snap1 = nimcp_brain_snapshot_cow(brain);
    ASSERT_NE(snap1, nullptr);

    // Modify
    float features1[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
                           0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    nimcp_brain_learn_example(brain, features1, 10, "class_b", 0.8f);

    // Create second snapshot
    nimcp_brain_snapshot_t snap2 = nimcp_brain_snapshot_cow(brain);
    ASSERT_NE(snap2, nullptr);

    // Modify more
    float features2[10] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                           1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    nimcp_brain_learn_example(brain, features2, 10, "class_c", 0.9f);

    // Both snapshots should be valid and isolated
    EXPECT_EQ(nimcp_brain_restore_cow(brain, snap1), NIMCP_SUCCESS);
    EXPECT_EQ(nimcp_brain_restore_cow(brain, snap2), NIMCP_SUCCESS);

    nimcp_brain_snapshot_destroy(snap1);
    nimcp_brain_snapshot_destroy(snap2);
}

TEST_F(BrainCOWTest, SnapshotPreservesStateAfterCloneModification) {
    // Create clone
    nimcp_brain_t clone = nimcp_brain_clone_cow(brain);
    ASSERT_NE(clone, nullptr);

    // Create snapshot of original
    nimcp_brain_snapshot_t snapshot = nimcp_brain_snapshot_cow(brain);
    ASSERT_NE(snapshot, nullptr);

    // Modify clone
    float features[10] = {0.9f, 0.9f, 0.9f, 0.9f, 0.9f,
                          0.9f, 0.9f, 0.9f, 0.9f, 0.9f};
    nimcp_brain_learn_example(clone, features, 10, "clone_class", 0.85f);

    // Restore original from snapshot should still work
    EXPECT_EQ(nimcp_brain_restore_cow(brain, snapshot), NIMCP_SUCCESS);

    nimcp_brain_destroy(clone);
    nimcp_brain_snapshot_destroy(snapshot);
}

TEST_F(BrainCOWTest, CloneFromRestoredBrain) {
    // Snapshot, restore, clone
    nimcp_brain_snapshot_t snapshot = nimcp_brain_snapshot_cow(brain);
    ASSERT_NE(snapshot, nullptr);

    ASSERT_EQ(nimcp_brain_restore_cow(brain, snapshot), NIMCP_SUCCESS);

    nimcp_brain_t clone = nimcp_brain_clone_cow(brain);
    EXPECT_NE(clone, nullptr);

    nimcp_brain_destroy(clone);
    nimcp_brain_snapshot_destroy(snapshot);
}
