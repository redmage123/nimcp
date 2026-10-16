#include <stdio.h>
#include <string.h>
#include "src/include/cognitive/nimcp_executive.h"
#include "src/utils/time/nimcp_time.h"

int main() {
    printf("Creating executive controller...\n");
    executive_controller_t* exec = executive_create();
    if (!exec) {
        printf("ERROR: Failed to create executive controller\n");
        printf("Error: %s\n", executive_get_last_error());
        return 1;
    }
    printf("SUCCESS: Created executive controller\n");

    // Try adding tasks
    for (int i = 0; i < 100; i++) {
        char name[64];
        snprintf(name, sizeof(name), "Task%d", i);

        task_descriptor_t task = {0};
        strncpy(task.name, name, sizeof(task.name) - 1);
        task.type = TASK_TYPE_CLASSIFICATION;
        task.priority = PRIORITY_NORMAL;
        task.status = TASK_STATUS_PENDING;
        task.created_ms = nimcp_time_monotonic_ms();

        uint32_t task_id = executive_add_task(exec, &task);
        if (task_id == 0) {
            printf("FAILED to add task %d\n", i);
            printf("Error: %s\n", executive_get_last_error());
            break;
        } else {
            printf("Added task %d with ID %u\n", i, task_id);
        }
    }

    executive_destroy(exec);
    return 0;
}
