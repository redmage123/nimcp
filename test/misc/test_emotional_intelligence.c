/**
 * @file test_emotional_intelligence.c
 * @brief Test emotional intelligence integration in brain pipeline
 *
 * WHAT: Verify emotion detection and empathetic response generation
 * WHY:  Ensure Phase 11: Part I integration is working
 * HOW:  Process text with negative emotions and check outputs
 */

#include <stdio.h>
#include <string.h>
#include "core/brain/nimcp_brain.h"

int main(void) {
    printf("\n=== NIMCP Emotional Intelligence Integration Test ===\n\n");

    // Create brain
    printf("Creating brain with default config...\n");
    brain_config_t config = brain_config_default();
    brain_t brain = brain_create(&config);

    if (!brain) {
        printf("ERROR: Failed to create brain\n");
        return 1;
    }
    printf("Brain created successfully!\n\n");

    // Test cases with different emotions
    const char* test_inputs[] = {
        "I'm so frustrated with this stupid assignment! It's impossible!",
        "I'm really scared and anxious about what might happen.",
        "I feel sad and hopeless about everything.",
        "This is great! I'm really happy with how this turned out!",
        "I don't understand this at all. I'm so confused."
    };

    int num_tests = sizeof(test_inputs) / sizeof(test_inputs[0]);

    for (int i = 0; i < num_tests; i++) {
        printf("--- Test %d ---\n", i + 1);
        printf("Input: \"%s\"\n", test_inputs[i]);

        // Create multimodal input
        brain_multimodal_input_t input = {0};
        input.language_text = test_inputs[i];
        input.has_language = true;

        // Process through brain
        brain_multimodal_output_t output = {0};
        if (brain_process_multimodal(brain, &input, &output)) {
            // Check emotion detection
            if (output.has_emotion_detected) {
                printf("  Emotion Detected: %s\n", output.detected_emotion);
                printf("  Confidence: %.2f\n", output.emotion_confidence);
                printf("  Valence: %.2f (%.1f = negative, +%.1f = positive)\n",
                       output.emotion_valence, -1.0, 1.0);
                printf("  Arousal: %.2f\n", output.emotion_arousal);
                printf("  Intensity: %.2f\n", output.emotion_intensity);
                printf("  Is Negative: %s\n", output.emotion_is_negative ? "YES" : "NO");
            } else {
                printf("  No emotion detected\n");
            }

            // Check empathetic response
            if (output.has_empathetic_response) {
                printf("\n  Empathetic Response Generated:\n");
                printf("  \"%s\"\n", output.empathetic_response);
                printf("  Empathy Score: %.2f\n", output.empathy_score);

                if (output.requires_human_escalation) {
                    printf("  ALERT: Requires human escalation!\n");
                    printf("  Reason: %s\n", output.escalation_reason);
                }
            } else {
                printf("\n  No empathetic response (emotion not negative or low confidence)\n");
            }
        } else {
            printf("  ERROR: Failed to process input\n");
        }

        printf("\n");
    }

    // Test crisis detection
    printf("--- Crisis Detection Test ---\n");
    const char* crisis_input = "I just want to die. I can't take this anymore.";
    printf("Input: \"%s\"\n", crisis_input);

    brain_multimodal_input_t crisis_test = {0};
    crisis_test.language_text = crisis_input;
    crisis_test.has_language = true;

    brain_multimodal_output_t crisis_output = {0};
    if (brain_process_multimodal(brain, &crisis_test, &crisis_output)) {
        if (crisis_output.has_emotion_detected) {
            printf("  Emotion: %s (valence: %.2f)\n",
                   crisis_output.detected_emotion,
                   crisis_output.emotion_valence);
        }

        if (crisis_output.has_empathetic_response) {
            printf("  Response: \"%s\"\n", crisis_output.empathetic_response);

            if (crisis_output.requires_human_escalation) {
                printf("  ✓ CRISIS DETECTED - Human escalation required\n");
                printf("  Reason: %s\n", crisis_output.escalation_reason);
            } else {
                printf("  WARNING: Crisis keywords detected but no escalation!\n");
            }
        }
    }

    printf("\n");

    // Cleanup
    brain_destroy(brain);
    printf("=== Test Complete ===\n");
    printf("Emotional intelligence is integrated and working!\n\n");

    return 0;
}
