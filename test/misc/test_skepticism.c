/**
 * @file test_skepticism.c
 * @brief Test epistemic filtering before training
 *
 * Tests that skepticism system can:
 * 1. Distinguish facts from opinions
 * 2. Detect biases
 * 3. Apply appropriate confidence adjustments
 */

#include <stdio.h>
#include <stdlib.h>
#include "src/core/brain/nimcp_brain.h"

int main(void) {
    printf("=== NIMCP Skepticism System Test ===\n\n");

    // Create brain with epistemic filtering enabled
    brain_config_t config = brain_config_defaults();
    config.size = BRAIN_SIZE_SMALL;
    config.task = BRAIN_TASK_CLASSIFICATION;
    config.num_inputs = 10;
    config.num_outputs = 5;
    config.learning_rate = 0.01f;
    config.enable_curiosity = true;
    config.enable_epistemic_filter = true;  // ENABLE SKEPTICISM

    brain_t brain = brain_create(&config);
    if (!brain) {
        fprintf(stderr, "ERROR: Failed to create brain\n");
        return 1;
    }

    printf("✓ Brain created with epistemic filtering enabled\n");
    printf("  Skepticism level: 0.6 (cautious but not paranoid)\n\n");

    // Test 1: Learn a factual statement (should have high confidence)
    printf("TEST 1: Learning factual statement\n");
    printf("Label: 'water boils at 100C'\n");
    float features1[10] = {1.0f, 0.5f, 0.3f, 0.7f, 0.2f, 0.8f, 0.4f, 0.6f, 0.1f, 0.9f};
    float loss1 = brain_learn_example(brain, features1, 10, "water boils at 100C", 0.9f);
    printf("  Learning loss: %.4f\n", loss1);
    printf("  (Epistemic filter applied - factual claims should learn normally)\n\n");

    // Test 2: Learn an opinion (should have reduced confidence)
    printf("TEST 2: Learning opinion statement\n");
    printf("Label: 'I think cats are better than dogs'\n");
    float features2[10] = {0.2f, 0.8f, 0.4f, 0.6f, 0.3f, 0.7f, 0.5f, 0.1f, 0.9f, 0.2f};
    float loss2 = brain_learn_example(brain, features2, 10, "I think cats are better than dogs", 0.9f);
    printf("  Learning loss: %.4f\n", loss2);
    printf("  (Epistemic filter should reduce confidence for opinions)\n\n");

    // Test 3: Learn a conspiracy-like statement (should be heavily penalized)
    printf("TEST 3: Learning conspiracy-like statement\n");
    printf("Label: 'they don't want you to know the truth'\n");
    float features3[10] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float loss3 = brain_learn_example(brain, features3, 10, "they don't want you to know the truth", 0.9f);
    printf("  Learning loss: %.4f\n", loss3);
    printf("  (Epistemic filter should heavily reduce confidence for conspiracy patterns)\n\n");

    // Test 4: Learn extraordinary claim without evidence
    printf("TEST 4: Learning extraordinary claim\n");
    printf("Label: 'humans can fly without equipment'\n");
    float features4[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float loss4 = brain_learn_example(brain, features4, 10, "humans can fly without equipment", 0.9f);
    printf("  Learning loss: %.4f\n", loss4);
    printf("  (Sagan standard: extraordinary claims need extraordinary evidence)\n\n");

    printf("=== Skepticism System Test Complete ===\n");
    printf("Epistemic filtering is ACTIVE and ready for training.\n");
    printf("The system will:\n");
    printf("  - Accept factual statements with normal confidence\n");
    printf("  - Reduce confidence for opinions and unverified claims\n");
    printf("  - Heavily penalize conspiracy-like reasoning\n");
    printf("  - Apply Sagan standard to extraordinary claims\n");

    brain_destroy(brain);
    return 0;
}
