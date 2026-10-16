/**
 * @file test_pink_weights.c
 * @brief Test program for pink noise weight initialization
 *
 * Verifies that the network builder can initialize synapse weights using pink noise.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "core/topology/nimcp_network_builder.h"

int main(void) {
    printf("========================================\n");
    printf("Pink Noise Weight Initialization Test\n");
    printf("========================================\n\n");

    // Create a scale-free network with verbose output
    printf("Creating scale-free network (100 neurons, gamma=-2.1)...\n");
    network_builder_config_t config = network_builder_default();
    config.num_neurons = 100;
    config.use_topology = true;
    config.topology_config.type = TOPOLOGY_SCALE_FREE;
    config.topology_config.params.scale_free = topology_default_scale_free_config();
    config.topology_config.params.scale_free.power_law_gamma = -2.1f;
    config.verbose = true;  // Enable verbose output to see what's happening

    neural_network_t network = network_builder_build(&config);
    if (!network) {
        fprintf(stderr, "ERROR: Failed to create network\n");
        return 1;
    }
    printf("\n");

    // Initialize weights with pink noise
    printf("Initializing weights with pink noise (amplitude=0.5)...\n");
    bool success = network_init_weights_pink_noise(network, 0.5f, 0.0f);
    if (!success) {
        fprintf(stderr, "ERROR: Failed to initialize weights\n");
        neural_network_destroy(network);
        return 1;
    }
    printf("Weight initialization complete!\n\n");

    printf("========================================\n");
    printf("Test PASSED!\n");
    printf("========================================\n");

    // Cleanup
    neural_network_destroy(network);
    return 0;
}
