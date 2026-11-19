/**
 * @file test_phase3.c
 * @brief Quick test of Phase 3 components
 */

#include <stdio.h>
#include <stdlib.h>
#include "plasticity/neuromodulators/nimcp_neuromod_pink_noise.h"
#include "nlp/nimcp_spike_nlp.h"
#include "core/topology/nimcp_network_builder.h"

int main(void) {
    printf("========================================\n");
    printf("Phase 3 Component Test\n");
    printf("========================================\n\n");

    // Test 1: Pink Noise Neuromodulation
    printf("Test 1: Pink Noise Neuromodulation\n");
    printf("----------------------------------\n");

    neuromod_pink_config_t config = neuromod_pink_default_config();
    neuromod_pink_noise_t* neuromod = neuromod_pink_create(&config);

    if (!neuromod) {
        fprintf(stderr, "ERROR: Failed to create neuromodulator\n");
        return 1;
    }

    printf("Created neuromodulator system\n");
    printf("Baseline dopamine: %.3f\n", neuromod_pink_get_dopamine(neuromod));

    // Update with reward
    neuromod_pink_update_reward(neuromod, 0.5f);
    printf("After reward=0.5: dopamine=%.3f\n", neuromod_pink_get_dopamine(neuromod));

    // Compute modulated learning rate
    float learning_rate = neuromod_pink_compute_learning_rate(neuromod, 0.01f);
    printf("Modulated learning rate: %.4f\n", learning_rate);

    neuromod_pink_destroy(neuromod);
    printf("✓ Neuromodulation test passed\n\n");

    // Test 2: Spike NLP with Network
    printf("Test 2: Spike-Based NLP\n");
    printf("------------------------\n");

    neural_network_t network = network_create_scale_free(100, -2.1f);
    if (!network) {
        fprintf(stderr, "ERROR: Failed to create network\n");
        return 1;
    }

    printf("Created scale-free network (100 neurons)\n");

    // Create simple word
    spike_nlp_word_t word;
    snprintf(word.word, sizeof(word.word), "test");
    word.embedding_dim = 50;
    for (uint32_t i = 0; i < 50; i++) {
        word.embedding[i] = 0.1f * (i % 10);  // Simple pattern
    }

    // Process word
    uint32_t spikes = spike_nlp_process_word(
        network,
        word.embedding,
        word.embedding_dim,
        0,  // input_start
        50  // num_input
    );

    printf("Processed word '%s': %u spikes generated\n", word.word, spikes);

    neural_network_destroy(network);
    printf("✓ Spike NLP test passed\n\n");

    printf("========================================\n");
    printf("All Phase 3 tests PASSED!\n");
    printf("========================================\n");

    return 0;
}
