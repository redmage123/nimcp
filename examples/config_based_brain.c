/**
 * @file config_based_brain.c
 * @brief Example of creating a brain from YAML/JSON configuration file
 *
 * This demonstrates how to use NIMCP configuration files to define
 * brain architecture, training parameters, and other settings.
 */

#include "nimcp.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    // Initialize NIMCP
    if (nimcp_init() != NIMCP_OK) {
        fprintf(stderr, "Failed to initialize NIMCP\n");
        return 1;
    }

    printf("NIMCP version: %s\n\n", nimcp_version());

    // Use config file from command line, or default
    const char* config_file = (argc > 1) ? argv[1] : "../configs/brain_simple.json";

    printf("=== Creating Brain from Config ===\"\n");
    printf("Config file: %s\n\n", config_file);

    // Create brain from configuration file
    nimcp_brain_t brain = nimcp_brain_create_from_config(config_file);

    if (!brain) {
        fprintf(stderr, "Failed to create brain: %s\n", nimcp_get_error());
        nimcp_shutdown();
        return 1;
    }

    printf("Brain created successfully!\n\n");

    // Train with some example data
    printf("=== Training Phase ===\n");

    float features1[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f};
    float features2[] = {10.0f, 9.0f, 8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f};
    float features3[] = {5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f};

    nimcp_brain_learn_example(brain, features1, 10, "class_a", 0.95f);
    printf("Learned example 1: class_a\n");

    nimcp_brain_learn_example(brain, features2, 10, "class_b", 0.90f);
    printf("Learned example 2: class_b\n");

    nimcp_brain_learn_example(brain, features3, 10, "class_c", 0.85f);
    printf("Learned example 3: class_c\n\n");

    // Make predictions
    printf("=== Prediction Phase ===\n");

    float test_features[] = {1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f, 8.5f, 9.5f, 10.5f};
    char predicted_label[64];
    float confidence;

    if (nimcp_brain_predict(brain, test_features, 10, predicted_label, &confidence) == NIMCP_OK) {
        printf("Test features: [1.5, 2.5, 3.5, 4.5, 5.5, ...]\n");
        printf("Predicted: %s\n", predicted_label);
        printf("Confidence: %.2f%%\n\n", confidence * 100.0f);
    } else {
        fprintf(stderr, "Prediction failed: %s\n", nimcp_get_error());
    }

    // Save brain
    printf("=== Saving Brain ===\n");
    const char* save_path = "/tmp/configured_brain.model";

    if (nimcp_brain_save(brain, save_path) == NIMCP_OK) {
        printf("Brain saved to: %s\n\n", save_path);
    } else {
        fprintf(stderr, "Failed to save brain: %s\n", nimcp_get_error());
    }

    // Cleanup
    nimcp_brain_destroy(brain);
    nimcp_shutdown();

    printf("=== Done! ===\n");
    return 0;
}
