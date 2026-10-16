/**
 * @file brain_probe_demo.c
 * @brief Demonstrates the brain_probe function for monitoring brain state
 *
 * This example shows how to:
 * 1. Create a brain
 * 2. Train it with some examples
 * 3. Use brain_probe to get comprehensive statistics
 * 4. Display the statistics
 */

#include "nimcp.h"
#include <stdio.h>
#include <stdlib.h>

void print_probe_stats(const nimcp_brain_probe_t* probe) {
    printf("\n=== Brain Probe Statistics ===\n");
    printf("Name: %s\n", probe->task_name);

    // Size
    const char* size_str = "UNKNOWN";
    switch(probe->size) {
        case NIMCP_BRAIN_TINY: size_str = "TINY"; break;
        case NIMCP_BRAIN_SMALL: size_str = "SMALL"; break;
        case NIMCP_BRAIN_MEDIUM: size_str = "MEDIUM"; break;
        case NIMCP_BRAIN_LARGE: size_str = "LARGE"; break;
    }
    printf("Size: %s\n", size_str);

    // Task
    const char* task_str = "UNKNOWN";
    switch(probe->task) {
        case NIMCP_TASK_CLASSIFICATION: task_str = "CLASSIFICATION"; break;
        case NIMCP_TASK_REGRESSION: task_str = "REGRESSION"; break;
        case NIMCP_TASK_PATTERN_MATCHING: task_str = "PATTERN_MATCHING"; break;
        case NIMCP_TASK_SEQUENCE: task_str = "SEQUENCE"; break;
        case NIMCP_TASK_ASSOCIATION: task_str = "ASSOCIATION"; break;
    }
    printf("Task: %s\n", task_str);

    // Architecture
    printf("\n--- Architecture ---\n");
    printf("Neurons: %u\n", probe->num_neurons);
    printf("Synapses: %u (active: %u, %.1f%%)\n",
           probe->num_synapses,
           probe->num_active_synapses,
           probe->num_synapses > 0 ?
               (float)probe->num_active_synapses / probe->num_synapses * 100.0f : 0.0f);
    printf("Inputs: %u\n", probe->num_inputs);
    printf("Outputs: %u\n", probe->num_outputs);

    // Performance
    printf("\n--- Performance ---\n");
    printf("Total inferences: %lu\n", (unsigned long)probe->total_inferences);
    printf("Total learning steps: %lu\n", (unsigned long)probe->total_learning_steps);
    printf("Avg inference time: %.2f μs\n", probe->avg_inference_time_us);

    // Learning
    printf("\n--- Learning ---\n");
    printf("Learning rate: %.6f\n", probe->current_learning_rate);
    printf("Avg sparsity: %.2f%%\n", probe->avg_sparsity * 100.0f);
    printf("Accuracy: %.2f%%\n", probe->accuracy * 100.0f);

    // Resources
    printf("\n--- Resources ---\n");
    printf("Memory usage: %.2f MB\n", probe->memory_bytes / (1024.0 * 1024.0));

    printf("==============================\n\n");
}

int main(void) {
    printf("Brain Probe Demo\n\n");

    // Initialize NIMCP
    if (nimcp_init() != NIMCP_OK) {
        fprintf(stderr, "Failed to initialize NIMCP: %s\n", nimcp_get_error());
        return 1;
    }

    // Create a small brain for classification
    nimcp_brain_t brain = nimcp_brain_create(
        "demo_classifier",
        NIMCP_BRAIN_SMALL,
        NIMCP_TASK_CLASSIFICATION,
        10,  // 10 inputs
        3    // 3 outputs (classes)
    );

    if (!brain) {
        fprintf(stderr, "Failed to create brain: %s\n", nimcp_get_error());
        nimcp_shutdown();
        return 1;
    }

    printf("Brain created successfully!\n");

    // Probe initial state
    nimcp_brain_probe_t probe;
    if (nimcp_brain_probe(brain, &probe) != NIMCP_OK) {
        fprintf(stderr, "Failed to probe brain: %s\n", nimcp_get_error());
        nimcp_brain_destroy(brain);
        nimcp_shutdown();
        return 1;
    }

    printf("\nInitial state after creation:");
    print_probe_stats(&probe);

    // Train with some example data
    printf("Training brain with example data...\n");
    float features[10];

    // Train 100 examples
    for (int i = 0; i < 100; i++) {
        // Generate random features
        for (int j = 0; j < 10; j++) {
            features[j] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        }

        // Assign to one of 3 classes
        const char* label = (i % 3 == 0) ? "class_a" :
                           (i % 3 == 1) ? "class_b" : "class_c";

        nimcp_status_t status = nimcp_brain_learn_example(brain, features, 10, label, 0.9f);
        if (status != NIMCP_OK) {
            fprintf(stderr, "Failed to learn example %d: %s\n", i, nimcp_get_error());
        }
    }

    printf("Training complete!\n");

    // Probe after training
    if (nimcp_brain_probe(brain, &probe) != NIMCP_OK) {
        fprintf(stderr, "Failed to probe brain: %s\n", nimcp_get_error());
        nimcp_brain_destroy(brain);
        nimcp_shutdown();
        return 1;
    }

    printf("\nState after training:");
    print_probe_stats(&probe);

    // Make some predictions to update inference count
    printf("Making predictions...\n");
    char predicted_label[64];
    float confidence;

    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            features[j] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        }

        nimcp_status_t status = nimcp_brain_predict(brain, features, 10,
                                                      predicted_label, &confidence);
        if (status == NIMCP_OK) {
            printf("  Prediction %d: %s (confidence: %.2f)\n",
                   i + 1, predicted_label, confidence);
        }
    }

    // Final probe
    if (nimcp_brain_probe(brain, &probe) != NIMCP_OK) {
        fprintf(stderr, "Failed to probe brain: %s\n", nimcp_get_error());
        nimcp_brain_destroy(brain);
        nimcp_shutdown();
        return 1;
    }

    printf("\nFinal state after predictions:");
    print_probe_stats(&probe);

    // Cleanup
    nimcp_brain_destroy(brain);
    nimcp_shutdown();

    printf("Demo completed successfully!\n");
    return 0;
}
