#include <stdio.h>
#include "src/include/nimcp.h"

int main() {
    printf("Creating brain...\n");
    nimcp_brain_t brain = nimcp_brain_create("test", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 10, 3);
    if (!brain) {
        printf("ERROR: Failed to create brain\n");
        return 1;
    }
    printf("Brain created\n");

    float features[10] = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0};

    printf("Training once...\n");
    nimcp_brain_learn_example(brain, features, 10, "label1", 0.9f);
    printf("Trained\n");

    printf("Creating snapshot...\n");
    nimcp_brain_snapshot_t snapshot = nimcp_brain_snapshot_cow(brain);
    if (!snapshot) {
        printf("ERROR: Failed to create snapshot\n");
        nimcp_brain_destroy(brain);
        return 1;
    }
    printf("Snapshot created\n");

    printf("Training more...\n");
    for (int i = 0; i < 5; i++) {
        nimcp_brain_learn_example(brain, features, 10, "label2", 0.9f);
    }
    printf("Trained more\n");

    printf("Restoring from snapshot...\n");
    nimcp_status_t status = nimcp_brain_restore_cow(brain, snapshot);
    printf("Restore returned: %d\n", status);

    if (status != NIMCP_OK) {
        printf("ERROR: Restore failed\n");
    }

    printf("Destroying snapshot...\n");
    nimcp_brain_snapshot_destroy(snapshot);
    printf("Snapshot destroyed\n");

    printf("Destroying brain...\n");
    nimcp_brain_destroy(brain);
    printf("Brain destroyed\n");

    printf("SUCCESS!\n");
    return 0;
}
