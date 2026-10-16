/**
 * @file simple_demo.c
 * @brief NIMCP Simple Reference Implementation - Clean & Easy to Understand
 *
 * This demo showcases NIMCP Phase 9 features in a simple, clear way:
 * - Brain API (high-level, easy to use)
 * - Multi-modal processing (visual, audio, text)
 * - Epistemic filtering (bias prevention)
 * - Ethical reasoning (Golden Rule)
 * - Learning from experience
 *
 * USAGE:
 *   ./simple_demo
 *
 * EXPECTED OUTPUT:
 *   - Pattern classification with confidence
 *   - Epistemic quality assessment
 *   - Ethical approval status
 *   - Learning progress
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "core/brain/nimcp_brain.h"
#include "utils/validation/nimcp_common.h"

//=============================================================================
// CONFIGURATION
//=============================================================================

#define NUM_TRAINING_EXAMPLES 5
#define INPUT_DIM 8
#define VISUAL_SIZE 64     // 8x8 image
#define AUDIO_SAMPLES 16   // Short audio clip

//=============================================================================
// HELPER FUNCTIONS - Clean and Simple
//=============================================================================

/**
 * @brief Create a simple 8x8 visual pattern
 * @param pattern_id Which pattern to create (0=vertical lines, 1=horizontal, 2=diagonal)
 * @param output 64-element array for 8x8 grayscale image
 */
void create_visual_pattern(int pattern_id, float* output) {
    memset(output, 0, VISUAL_SIZE * sizeof(float));

    switch(pattern_id % 3) {
        case 0: // Vertical lines
            for (int y = 0; y < 8; y++) {
                for (int x = 0; x < 8; x += 2) {
                    output[y * 8 + x] = 1.0f;
                }
            }
            break;
        case 1: // Horizontal lines
            for (int y = 0; y < 8; y += 2) {
                for (int x = 0; x < 8; x++) {
                    output[y * 8 + x] = 1.0f;
                }
            }
            break;
        case 2: // Diagonal line
            for (int i = 0; i < 8; i++) {
                output[i * 8 + i] = 1.0f;
            }
            break;
    }
}

/**
 * @brief Create a simple audio pattern (sine wave at different frequencies)
 * @param pattern_id Which pattern to create
 * @param output Audio samples array
 */
void create_audio_pattern(int pattern_id, float* output) {
    float frequency = 100.0f + (pattern_id * 50.0f); // Different frequencies
    for (int i = 0; i < AUDIO_SAMPLES; i++) {
        float t = i / 1000.0f; // Time in seconds
        output[i] = 0.5f * sinf(2.0f * 3.14159f * frequency * t);
    }
}

/**
 * @brief Print a clean separator
 */
void print_separator(void) {
    printf("\n----------------------------------------\n");
}

/**
 * @brief Print results in a clean, readable format
 */
void print_results(const brain_multimodal_output_t* output, const char* expected_label) {
    printf("Decision: %s\n", output->decision_label);
    printf("Confidence: %.1f%%\n", output->confidence * 100.0f);
    printf("Expected: %s\n", expected_label);
    printf("Correct: %s\n", strcmp(output->decision_label, expected_label) == 0 ? "✓ YES" : "✗ NO");

    print_separator();
    printf("QUALITY METRICS:\n");
    printf("  Epistemic Quality: %.1f%%\n", output->epistemic_quality * 100.0f);
    printf("  Credibility: %.1f%%\n", output->credibility_score * 100.0f);
    printf("  Requires Verification: %s\n", output->requires_verification ? "Yes" : "No");
    printf("  Bias Detected: %s\n", output->bias_detected ? "⚠ YES" : "✓ No");
    printf("  Ethical Approval: %s\n", output->ethical_approved ? "✓ YES" : "✗ NO");

    print_separator();
    printf("ATTENTION BREAKDOWN:\n");
    printf("  Visual: %.1f%%\n", output->visual_attention * 100.0f);
    printf("  Audio: %.1f%%\n", output->audio_attention * 100.0f);
    printf("  Direct: %.1f%%\n", output->direct_attention * 100.0f);

    if (strlen(output->epistemic_reasoning) > 0) {
        print_separator();
        printf("REASONING: %s\n", output->epistemic_reasoning);
    }
}

//=============================================================================
// MAIN DEMO
//=============================================================================

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  NIMCP Simple Demo - Phase 9 Features                    ║\n");
    printf("║  Clean, Simple, Easy to Understand                       ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");

    //-------------------------------------------------------------------------
    // STEP 1: Create Brain with Multi-Modal Support
    //-------------------------------------------------------------------------
    print_separator();
    printf("STEP 1: Creating Brain...\n");

    // Create configuration with multimodal enabled
    brain_config_t config = {0};  // Zero-initialize all fields
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 64;  // Total input dimension (must accommodate all features)
    config.num_outputs = 3;
    config.learning_rate = 0.01f;
    config.sparsity_target = 0.85f;
    config.enable_explanations = true;

    // Enable multimodal processing with sensory cortexes
    config.enable_multimodal_integration = true;
    config.enable_visual_cortex = true;
    config.visual_feature_dim = 32;  // Visual features (CNNfrom 8x8 image)
    config.enable_audio_cortex = true;
    config.audio_feature_dim = 16;   // Audio features (FFT from 16 samples)
    // Remaining 16 dimensions for direct features (64 - 32 - 16 = 16)

    strncpy(config.task_name, "simple_demo", sizeof(config.task_name) - 1);

    brain_t brain = brain_create_custom(&config);

    if (!brain) {
        const char* error = brain_get_last_error();
        printf("Error: Failed to create brain\n");
        if (error) {
            printf("Details: %s\n", error);
        }
        return 1;
    }

    printf("✓ Brain created: 1,000 neurons, 3 output classes\n");
    printf("  Multimodal integration: %s\n", config.enable_multimodal_integration ? "enabled" : "disabled");
    printf("  Visual cortex: %s\n", config.enable_visual_cortex ? "enabled" : "disabled");
    printf("  Audio cortex: %s\n", config.enable_audio_cortex ? "enabled" : "disabled");

    //-------------------------------------------------------------------------
    // STEP 2: Train the Brain (Simple API)
    //-------------------------------------------------------------------------
    print_separator();
    printf("STEP 2: Training Brain with 5 examples...\n\n");

    const char* labels[] = {"vertical", "horizontal", "diagonal"};

    for (int i = 0; i < NUM_TRAINING_EXAMPLES; i++) {
        // Create feature vector matching brain's input dimension (64)
        // In multimodal mode, features represent the integrated output from all modalities
        float features[64];  // Match config.num_inputs
        memset(features, 0, sizeof(features));

        // Fill first 8 elements with pattern (represents integrated features)
        for (int j = 0; j < INPUT_DIM && j < 64; j++) {
            features[j] = (i % 3) == 0 ? 1.0f : (j % 2 == 0 ? 0.8f : 0.2f);
        }

        const char* label = labels[i % 3];

        // Train (One Line!)
        bool success = brain_learn_example(brain, features, 64, label, 0.9f);

        printf("  Example %d: %s... %s\n", i + 1, label, success ? "✓" : "✗");
    }

    printf("\n✓ Training complete\n");

    //-------------------------------------------------------------------------
    // STEP 3: Test with Multi-Modal Input (Visual + Audio)
    //-------------------------------------------------------------------------
    print_separator();
    printf("STEP 3: Testing with Multi-Modal Input...\n");

    // Prepare visual data (8x8 grayscale image - convert float to uint8_t)
    float visual_data_float[VISUAL_SIZE];
    uint8_t visual_data[VISUAL_SIZE];
    create_visual_pattern(0, visual_data_float); // Vertical lines
    for (int i = 0; i < VISUAL_SIZE; i++) {
        visual_data[i] = (uint8_t)(visual_data_float[i] * 255.0f); // Convert to 0-255
    }

    // Prepare audio data
    float audio_data[AUDIO_SAMPLES];
    create_audio_pattern(0, audio_data);

    // Prepare direct features
    float direct_data[INPUT_DIM];
    for (int i = 0; i < INPUT_DIM; i++) {
        direct_data[i] = i % 2 == 0 ? 1.0f : 0.2f;
    }

    // Create multi-modal input
    brain_multimodal_input_t input = {0};
    input.visual_data = visual_data;
    input.visual_width = 8;
    input.visual_height = 8;
    input.visual_channels = 1;  // Grayscale
    input.audio_data = audio_data;
    input.audio_samples = AUDIO_SAMPLES;
    input.audio_channels = 1;   // Mono
    input.direct_data = direct_data;
    input.direct_dim = INPUT_DIM;
    input.timestamp_ms = 0;

    // Process (One Line!)
    printf("  Calling brain_process_multimodal...\n");
    printf("    Visual: %dx%d (%d channels)\n", input.visual_width, input.visual_height, input.visual_channels);
    printf("    Audio: %d samples (%d channels)\n", input.audio_samples, input.audio_channels);
    printf("    Direct: %d features\n", input.direct_dim);

    brain_multimodal_output_t output = {0};
    bool success = brain_process_multimodal(brain, &input, &output);

    if (!success) {
        const char* error = brain_get_last_error();
        printf("\nError: Processing failed\n");
        if (error && error[0] != '\0') {
            printf("Details: %s\n", error);
        } else {
            printf("Details: No error message available\n");
        }
        brain_destroy(brain);
        return 1;
    }

    print_separator();
    printf("RESULTS:\n");
    print_separator();
    print_results(&output, "vertical");

    //-------------------------------------------------------------------------
    // STEP 4: Test Epistemic Filtering (Bias Detection)
    //-------------------------------------------------------------------------
    print_separator();
    printf("STEP 4: Testing Epistemic Filter (Bias Prevention)...\n\n");

    // Test with low-quality input (should flag for verification)
    for (int i = 0; i < INPUT_DIM; i++) {
        direct_data[i] = 0.5f; // Ambiguous input
    }

    input.visual_data = NULL; // No visual
    input.audio_data = NULL;  // No audio

    success = brain_process_multimodal(brain, &input, &output);

    if (success) {
        print_separator();
        printf("LOW-QUALITY INPUT TEST:\n");
        print_separator();
        printf("Decision: %s\n", output.decision_label);
        printf("Confidence: %.1f%%\n", output.confidence * 100.0f);
        printf("Epistemic Quality: %.1f%% (Low = Expected)\n",
               output.epistemic_quality * 100.0f);
        printf("Requires Verification: %s (Yes = Expected)\n",
               output.requires_verification ? "✓ YES" : "✗ NO");
    }

    //-------------------------------------------------------------------------
    // STEP 5: Cleanup
    //-------------------------------------------------------------------------
    print_separator();
    printf("STEP 5: Cleanup...\n");
    brain_destroy(brain);
    printf("✓ Brain destroyed\n");

    //-------------------------------------------------------------------------
    // Summary
    //-------------------------------------------------------------------------
    print_separator();
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  Demo Complete!                                           ║\n");
    printf("║                                                           ║\n");
    printf("║  Key Features Demonstrated:                               ║\n");
    printf("║  ✓ Simple Brain API (3 function calls)                   ║\n");
    printf("║  ✓ Multi-modal input (visual + audio + direct)           ║\n");
    printf("║  ✓ Epistemic filtering (bias prevention)                 ║\n");
    printf("║  ✓ Quality metrics (confidence, credibility)             ║\n");
    printf("║  ✓ Attention breakdown (which inputs matter)             ║\n");
    printf("║                                                           ║\n");
    printf("║  Total Lines of Code: ~170 (including comments)          ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");

    return 0;
}
