#include <stdio.h>
#include "src/include/nimcp.h"

int main() {
    printf("Step 1: Create brain\n");
    nimcp_brain_t brain = nimcp_brain_create("test", NIMCP_BRAIN_SMALL, NIMCP_TASK_CLASSIFICATION, 10, 3);
    if (!brain) {
        printf("FAIL: create brain\n");
        return 1;
    }

    float features[10] = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0};

    printf("Step 2: Train once\n");
    nimcp_brain_learn_example(brain, features, 10, "label1", 0.9f);

    printf("Step 3: Create snapshot\n");
    nimcp_brain_snapshot_t snapshot = nimcp_brain_snapshot_cow(brain);
    if (!snapshot) {
        printf("FAIL: create snapshot\n");
        return 1;
    }

    printf("Step 4: Train more\n");
    for (int i = 0; i < 10; i++) {
        nimcp_brain_learn_example(brain, features, 10, "label2", 0.9f);
    }

    printf("Step 5: Restore from snapshot\n");
    nimcp_status_t status = nimcp_brain_restore_cow(brain, snapshot);
    if (status != NIMCP_OK) {
        printf("FAIL: restore returned %d\n", status);
        return 1;
    }
    printf("Restore succeeded\n");

    printf("Step 6: Probe restored brain\n");
    nimcp_brain_probe_t probe;
    nimcp_brain_probe(brain, &probe);
    printf("Probe succeeded, learning_steps=%llu\n", (unsigned long long)probe.total_learning_steps);

    printf("Step 7: Destroy snapshot\n");
    nimcp_brain_snapshot_destroy(snapshot);
    printf("Snapshot destroyed\n");

    printf("SLEEPING 1 second before Step 8\n");
    sleep(1);

    printf("Step 8: Destroy brain\n");
    fflush(stdout);
    fprintf(stderr, "About to call nimcp_brain_destroy(%p)\n", (void*)brain);
    fflush(stderr);
    nimcp_brain_destroy(brain);
    printf("Brain destroyed\n");

    printf("SUCCESS!\n");
    return 0;
}
