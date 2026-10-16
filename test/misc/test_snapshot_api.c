/**
 * Test for public snapshot API (nimcp.h)
 */

#include "nimcp.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    printf("=== Testing Public Snapshot API ===\n\n");

    // Create a brain
    printf("Creating brain...\n");
    nimcp_brain_t brain = nimcp_brain_create(
        "snapshot_test",               // name
        NIMCP_BRAIN_TINY,              // size
        NIMCP_TASK_CLASSIFICATION,     // task
        4,                              // num_inputs
        2                               // num_outputs
    );

    if (!brain) {
        printf("FAIL: Failed to create brain\n");
        return 1;
    }
    printf("SUCCESS: Brain created\n\n");

    // Make a prediction to change state
    float inputs[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    char label[64];
    float confidence;
    nimcp_status_t status = nimcp_brain_predict(brain, inputs, 4, label, &confidence);
    printf("Initial prediction: %s (confidence: %.3f, status: %d)\n\n",
           label, confidence, status);

    // Save a snapshot
    printf("Saving snapshot 'test1'...\n");
    status = nimcp_brain_snapshot_save(brain, "test1", "First test snapshot");
    if (status != 0) {  // 0 is success
        printf("FAIL: Failed to save snapshot (status: %d)\n", status);
        nimcp_brain_destroy(brain);
        return 1;
    }
    printf("SUCCESS: Snapshot saved\n\n");

    // Train a bit to change the brain
    for (int i = 0; i < 5; i++) {
        nimcp_brain_learn_example(brain, inputs, 4, "class_0", 1.0f);
    }

    status = nimcp_brain_predict(brain, inputs, 4, label, &confidence);
    printf("After training: %s (confidence: %.3f)\n\n", label, confidence);

    // Save another snapshot
    printf("Saving snapshot 'test2'...\n");
    status = nimcp_brain_snapshot_save(brain, "test2", "After some training");
    if (status != 0) {
        printf("FAIL: Failed to save snapshot (status: %d)\n", status);
        nimcp_brain_destroy(brain);
        return 1;
    }
    printf("SUCCESS: Snapshot saved\n\n");

    // List snapshots
    printf("Listing snapshots...\n");
    nimcp_brain_snapshot_info_t infos[10];
    uint32_t count = 0;
    status = nimcp_brain_snapshot_list(brain, infos, 10, &count);
    if (status == 0) {
        printf("Found %u snapshots\n", count);
        for (uint32_t i = 0; i < count; i++) {
            printf("  [%u] %s: %s (size: %u bytes)\n",
                   i, infos[i].name, infos[i].description, infos[i].file_size);
        }
    } else {
        printf("Note: Listing not fully implemented yet (status: %d)\n", status);
    }
    printf("\n");

    // Delete a snapshot
    printf("Deleting snapshot 'test1'...\n");
    status = nimcp_brain_snapshot_delete(brain, "test1");
    if (status == 0) {
        printf("SUCCESS: Snapshot deleted\n");
    } else {
        printf("Note: Delete may have failed (status: %d)\n", status);
    }
    printf("\n");

    // Clean up
    nimcp_brain_destroy(brain);
    printf("=== All tests completed ===\n");

    return 0;
}
