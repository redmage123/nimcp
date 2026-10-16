#include "nimcp_brain.h"
#include <stdio.h>
#include <string.h>

int main() {
    brain_config_t config = {};
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 3;
    strncpy(config.task_name, "phase10", sizeof(config.task_name) - 1);

    // Enable Phase 10 features
    config.enable_working_memory = true;
    config.working_memory_capacity = 7;
    config.enable_emotional_tagging = true;
    config.enable_executive_control = true;
    config.enable_sleep_wake_cycle = true;
    config.enable_mental_health_monitoring = true;
    config.enable_theory_of_mind = true;
    config.enable_natural_explanations = true;
    config.enable_meta_learning = true;
    config.enable_predictive_processing = true;
    config.enable_mirror_neurons = true;
    config.mirror_neuron_count = 1000;

    printf("Creating brain with all Phase 10 features enabled...\n");
    brain_t brain = brain_create_custom(&config);

    if (!brain) {
        const char* error = brain_get_last_error();
        printf("ERROR: Failed to create brain: %s\n", error ? error : "Unknown error");
        return 1;
    }

    printf("SUCCESS: Brain created successfully\n");
    brain_destroy(brain);
    return 0;
}
