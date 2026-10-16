/**
 * Simple test to verify brain snapshot functionality
 * Tests: save_snapshot, restore_snapshot, initial/final snapshots
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nimcp.h"

#define TEST_SNAPSHOT_DIR "./test_snapshots"

void test_initial_final_snapshots(void)
{
    printf("\n=== Testing Initial/Final Snapshots ===\n");

    // Create brain with initial and final snapshots enabled
    brain_config_t config = {
        .task_name = "snapshot_test",
        .size = BRAIN_SIZE_SMALL,
        .num_inputs = 4,
        .num_hidden = 8,
        .num_outputs = 2,
        .learning_rate = 0.01f,
        .checkpoint_path = NULL,  // No checkpointing
        .auto_load = false,
        .snapshot_dir = TEST_SNAPSHOT_DIR,
        .save_initial_snapshot = true,
        .save_final_snapshot = true,
        .enable_auto_snapshots = false
    };

    printf("Creating brain (should save initial snapshot)...\n");
    brain_t brain = brain_create_custom(&config);
    if (!brain) {
        printf("FAIL: Failed to create brain\n");
        return;
    }

    // Make some decisions to modify brain state
    float inputs[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t decision = brain_decide(brain, inputs, 4);
    printf("Made decision: %d (confidence: %.3f)\n", decision.choice, decision.confidence);

    printf("Destroying brain (should save final snapshot)...\n");
    brain_destroy(brain);

    printf("PASS: Initial/Final snapshots completed\n");
}

void test_manual_snapshots(void)
{
    printf("\n=== Testing Manual Snapshots ===\n");

    brain_config_t config = {
        .task_name = "manual_snapshot_test",
        .size = BRAIN_SIZE_TINY,
        .num_inputs = 2,
        .num_hidden = 4,
        .num_outputs = 2,
        .learning_rate = 0.01f,
        .snapshot_dir = TEST_SNAPSHOT_DIR,
        .save_initial_snapshot = false,
        .save_final_snapshot = false
    };

    printf("Creating brain...\n");
    brain_t brain = brain_create_custom(&config);
    if (!brain) {
        printf("FAIL: Failed to create brain\n");
        return;
    }

    // Save a manual snapshot
    printf("Saving manual snapshot 'test1'...\n");
    if (!brain_save_snapshot(brain, "test1", "First test snapshot")) {
        printf("FAIL: Failed to save snapshot\n");
        brain_destroy(brain);
        return;
    }
    printf("PASS: Snapshot 'test1' saved\n");

    // Make some decisions
    float inputs[2] = {0.8f, 0.3f};
    brain_decision_t decision = brain_decide(brain, inputs, 2);
    printf("Made decision: %d\n", decision.choice);

    // Save another snapshot
    printf("Saving manual snapshot 'test2'...\n");
    if (!brain_save_snapshot(brain, "test2", "Second test snapshot")) {
        printf("FAIL: Failed to save snapshot\n");
        brain_destroy(brain);
        return;
    }
    printf("PASS: Snapshot 'test2' saved\n");

    brain_destroy(brain);
}

void test_snapshot_restore(void)
{
    printf("\n=== Testing Snapshot Restore ===\n");

    brain_config_t config = {
        .task_name = "restore_test",
        .size = BRAIN_SIZE_TINY,
        .num_inputs = 2,
        .num_hidden = 4,
        .num_outputs = 2,
        .learning_rate = 0.01f,
        .snapshot_dir = TEST_SNAPSHOT_DIR,
        .save_initial_snapshot = false,
        .save_final_snapshot = false
    };

    printf("Creating brain...\n");
    brain_t brain = brain_create_custom(&config);
    if (!brain) {
        printf("FAIL: Failed to create brain\n");
        return;
    }

    // Save state
    printf("Saving snapshot 'restore_test'...\n");
    if (!brain_save_snapshot(brain, "restore_test", "Test restore")) {
        printf("FAIL: Failed to save snapshot\n");
        brain_destroy(brain);
        return;
    }

    // Get initial decision
    float inputs[2] = {0.5f, 0.5f};
    brain_decision_t decision1 = brain_decide(brain, inputs, 2);
    printf("Initial decision: %d (confidence: %.3f)\n",
           decision1.choice, decision1.confidence);

    // Make many more decisions to change state significantly
    for (int i = 0; i < 10; i++) {
        brain_decide(brain, inputs, 2);
    }

    brain_decision_t decision2 = brain_decide(brain, inputs, 2);
    printf("After 11 decisions: %d (confidence: %.3f)\n",
           decision2.choice, decision2.confidence);

    brain_destroy(brain);

    // Restore from snapshot
    printf("Restoring from snapshot 'restore_test'...\n");
    brain_t restored = brain_restore_snapshot(NULL, "test_snapshots/restore_test");
    if (!restored) {
        printf("FAIL: Failed to restore snapshot\n");
        return;
    }

    brain_decision_t decision3 = brain_decide(restored, inputs, 2);
    printf("After restore: %d (confidence: %.3f)\n",
           decision3.choice, decision3.confidence);

    printf("PASS: Snapshot restore completed\n");

    brain_destroy(restored);
}

void test_delete_snapshot(void)
{
    printf("\n=== Testing Snapshot Delete ===\n");

    brain_config_t config = {
        .task_name = "delete_test",
        .size = BRAIN_SIZE_TINY,
        .num_inputs = 2,
        .num_hidden = 4,
        .num_outputs = 2,
        .snapshot_dir = TEST_SNAPSHOT_DIR
    };

    brain_t brain = brain_create_custom(&config);
    if (!brain) {
        printf("FAIL: Failed to create brain\n");
        return;
    }

    // Create a snapshot to delete
    printf("Creating snapshot 'to_delete'...\n");
    if (!brain_save_snapshot(brain, "to_delete", "Will be deleted")) {
        printf("FAIL: Failed to save snapshot\n");
        brain_destroy(brain);
        return;
    }

    // Delete it
    printf("Deleting snapshot 'to_delete'...\n");
    if (!brain_delete_snapshot(brain, "to_delete")) {
        printf("FAIL: Failed to delete snapshot\n");
        brain_destroy(brain);
        return;
    }

    printf("PASS: Snapshot deleted successfully\n");

    brain_destroy(brain);
}

int main(void)
{
    printf("Brain Snapshot Functionality Test\n");
    printf("==================================\n");

    // Clean up any existing test snapshots
    system("rm -rf " TEST_SNAPSHOT_DIR);

    test_initial_final_snapshots();
    test_manual_snapshots();
    test_snapshot_restore();
    test_delete_snapshot();

    printf("\n=== All Snapshot Tests Completed ===\n");
    printf("Check %s directory for saved snapshots\n", TEST_SNAPSHOT_DIR);

    return 0;
}
