/**
 * @file programmable_synapses_demo.c
 * @brief Demonstration of NIMCP 2.7 Programmable Synapse Computation
 *
 * This demo showcases the power of synapse-level computation by implementing:
 * 1. Attention-modulated synapses (query-key similarity)
 * 2. Neuromodulator-sensitive synapses (dopamine effects)
 * 3. Gating synapses (context-dependent routing)
 *
 * MOTIVATION:
 * Traditional SNNs treat synapses as static weights. NIMCP 2.7 makes each
 * synapse a computational unit, enabling:
 * - Attention mechanisms at the synapse level
 * - Context-dependent transmission
 * - Dynamic routing and gating
 * - Semantic similarity computation
 *
 * This is a game-changer for NLP and cognitive tasks.
 *
 * @author Claude Code + NIMCP Development Team
 * @date 2025-11-07
 * @version 2.7.0
 */

// NIMCP 2.7 includes
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/synapse_compute/nimcp_synapse_compute.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

//=============================================================================
// Internal Access Macro (for demo purposes)
//=============================================================================
// WHAT: Macro to access internal network structure
// WHY: neural_network_t is opaque, but demos need direct neuron access
// HOW: Cast opaque pointer to internal struct type
// NOTE: Production code should use accessor functions instead
#define NETWORK_INTERNAL(net) ((struct neural_network_internal_t*)(net))

// Forward declare internal structure
struct neural_network_internal_t {
    neuron_t* neurons;
    uint32_t num_neurons;
    // ... other fields omitted for brevity
    uint64_t network_time;
};

//=============================================================================
// Helper Functions
//=============================================================================

// Helper: Print a divider
static void print_divider(const char* title) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  %s\n", title);
    printf("═══════════════════════════════════════════════════════════\n");
}

// Helper: Print synapse information
static void print_synapse_info(const char* name, synapse_t* syn, float transmission) {
    printf("%-30s  Weight: %6.3f  Transmission: %6.3f\n",
           name, syn->weight, transmission);
}

/**
 * Demo 1: Attention-Modulated Synapses
 *
 * Shows how synapses can compute attention weights based on query-key similarity.
 * This is the foundation for Transformer-like attention in SNNs.
 */
static void demo_attention_synapses(void) {
    print_divider("DEMO 1: Attention-Modulated Synapses");

    printf("\nCreating a small network with attention-modulated synapses...\n");

    // Create network
    network_config_t config = {
        .num_neurons = 5,
        .ei_ratio = 0.8f,
        .learning_rate = 0.01f,
        .initial_weight_range = 0.5f
    };

    neural_network_t net = neural_network_create(&config);
    if (!net) {
        printf("ERROR: Failed to create network\n");
        return;
    }

    // Create 3 input neurons and 2 output neurons
    for (int i = 0; i < 5; i++) {
        neuron_t* neuron = neural_network_add_neuron(net, (i < 3) ? EXCITATORY : EXCITATORY);
        if (neuron) {
            neuron->threshold = 0.5f;
            neuron->bias = 0.1f;
        }
    }

    // Add synapses with attention computation
    // Input neurons (0,1,2) -> Output neuron 3
    printf("\nAdding attention-modulated synapses...\n");

    for (int i = 0; i < 3; i++) {
        if (neural_network_add_synapse(net, i, 3, 1.0f)) {
            // Get the synapse we just added
            neuron_t* post_neuron = &NETWORK_INTERNAL(net)->neurons[3];
            if (post_neuron->num_incoming > 0) {
                synapse_t* syn = &post_neuron->incoming_synapses[post_neuron->num_incoming - 1];

                // Attach attention compute function
                synapse_set_compute_function(syn,
                                            synapse_compute_attention,
                                            NULL,  // Use default learning
                                            NULL,
                                            NULL);

                printf("  Synapse %d→3: Attention-modulated\n", i);
            }
        }
    }

    // Activate input neurons with different patterns
    printf("\nActivating input neurons...\n");
    NETWORK_INTERNAL(net)->neurons[0].state = 0.8f;  // High activity
    NETWORK_INTERNAL(net)->neurons[1].state = 0.3f;  // Medium activity
    NETWORK_INTERNAL(net)->neurons[2].state = 0.1f;  // Low activity

    printf("  Neuron 0: activity = %.2f\n", NETWORK_INTERNAL(net)->neurons[0].state);
    printf("  Neuron 1: activity = %.2f\n", NETWORK_INTERNAL(net)->neurons[1].state);
    printf("  Neuron 2: activity = %.2f\n", NETWORK_INTERNAL(net)->neurons[2].state);

    // Simulate one timestep
    printf("\nSimulating network (attention weights computed automatically)...\n");
    neural_network_step(net);

    // Check output neuron activation
    printf("\nOutput neuron 3 state: %.4f\n", NETWORK_INTERNAL(net)->neurons[3].state);
    printf("(Higher state means attention mechanism is working)\n");

    // Cleanup
    neural_network_destroy(net);
    printf("\n✓ Demo 1 complete!\n");
}

/**
 * Demo 2: Neuromodulator-Sensitive Synapses
 *
 * Shows how dopamine/serotonin can modulate synaptic transmission.
 * Critical for reward learning and emotional modulation.
 */
static void demo_neuromodulated_synapses(void) {
    print_divider("DEMO 2: Neuromodulator-Sensitive Synapses");

    printf("\nCreating network with dopamine-sensitive synapses...\n");

    network_config_t config = {
        .num_neurons = 3,
        .ei_ratio = 1.0f,
        .learning_rate = 0.01f,
        .initial_weight_range = 0.5f
    };

    neural_network_t net = neural_network_create(&config);
    if (!net) {
        printf("ERROR: Failed to create network\n");
        return;
    }

    // Create neurons
    for (int i = 0; i < 3; i++) {
        neuron_t* neuron = neural_network_add_neuron(net, EXCITATORY);
        if (neuron) {
            neuron->threshold = 0.5f;
            neuron->bias = 0.0f;
        }
    }

    // Add neuromodulated synapse: 0 → 1
    printf("\nAdding neuromodulated synapse...\n");
    if (neural_network_add_synapse(net, 0, 1, 1.0f)) {
        neuron_t* post_neuron = &NETWORK_INTERNAL(net)->neurons[1];
        if (post_neuron->num_incoming > 0) {
            synapse_t* syn = &post_neuron->incoming_synapses[post_neuron->num_incoming - 1];

            // Allocate compute state
            syn->compute_state = calloc(1, sizeof(synapse_compute_state_t));
            if (syn->compute_state) {
                // Set sensitivity to dopamine (higher = more sensitive)
                syn->compute_state->local_memory[0] = 2.0f;  // 2x sensitivity

                // Attach neuromodulated compute function
                syn->compute_function = synapse_compute_neuromodulated;

                printf("  Synapse 0→1: Dopamine-sensitive (sensitivity = 2.0)\n");
            }
        }
    }

    // Test with different dopamine levels
    NETWORK_INTERNAL(net)->neurons[0].state = 0.5f;  // Constant input activity

    printf("\n--- Testing with NO dopamine (neuromodulation = 0.0) ---\n");
    synapse_compute_context_t context = {
        .neuromodulation = 0.0f,
        .current_time = 0
    };

    // Manually compute transmission for demonstration
    neuron_t* post_neuron = &NETWORK_INTERNAL(net)->neurons[1];
    synapse_t* syn = &post_neuron->incoming_synapses[0];
    float transmission_no_dop = syn->compute_function(syn, &NETWORK_INTERNAL(net)->neurons[0], post_neuron, 0.5f, &context);

    printf("Input activity: 0.5\n");
    printf("Synaptic transmission: %.4f\n", transmission_no_dop);

    printf("\n--- Testing with HIGH dopamine (neuromodulation = 0.5) ---\n");
    context.neuromodulation = 0.5f;
    float transmission_high_dop = syn->compute_function(syn, &NETWORK_INTERNAL(net)->neurons[0], post_neuron, 0.5f, &context);

    printf("Input activity: 0.5\n");
    printf("Synaptic transmission: %.4f\n", transmission_high_dop);
    printf("Amplification: %.2fx\n", transmission_high_dop / transmission_no_dop);

    // Cleanup
    if (syn->compute_state) free(syn->compute_state);
    neural_network_destroy(net);
    printf("\n✓ Demo 2 complete!\n");
}

/**
 * Demo 3: Gating Synapses
 *
 * Shows how synapses can act as gates controlled by external signals.
 * Essential for LSTM-like gating and context-dependent routing.
 */
static void demo_gating_synapses(void) {
    print_divider("DEMO 3: Gating Synapses (LSTM-like)");

    printf("\nCreating network with gating synapses...\n");

    network_config_t config = {
        .num_neurons = 4,
        .ei_ratio = 1.0f,
        .learning_rate = 0.01f,
        .initial_weight_range = 0.5f
    };

    neural_network_t net = neural_network_create(&config);
    if (!net) {
        printf("ERROR: Failed to create network\n");
        return;
    }

    // Create neurons
    for (int i = 0; i < 4; i++) {
        neuron_t* neuron = neural_network_add_neuron(net, EXCITATORY);
        if (neuron) {
            neuron->threshold = 0.5f;
            neuron->bias = 0.0f;
        }
    }

    // Add gating synapse: 0 → 1 (gated by signal)
    printf("\nAdding gating synapse...\n");
    if (neural_network_add_synapse(net, 0, 1, 1.0f)) {
        neuron_t* post_neuron = &NETWORK_INTERNAL(net)->neurons[1];
        if (post_neuron->num_incoming > 0) {
            synapse_t* syn = &post_neuron->incoming_synapses[post_neuron->num_incoming - 1];

            // Allocate compute state
            syn->compute_state = calloc(1, sizeof(synapse_compute_state_t));
            if (syn->compute_state) {
                // Initial gate state = closed (0.0)
                syn->compute_state->local_memory[0] = 0.0f;

                // Attach gating compute function
                syn->compute_function = synapse_compute_gating;

                printf("  Synapse 0→1: Gating synapse (initial gate = closed)\n");
            }
        }
    }

    // Test with gate closed
    NETWORK_INTERNAL(net)->neurons[0].state = 0.8f;  // High input activity

    printf("\n--- Testing with GATE CLOSED (gate = 0.0) ---\n");
    printf("Input activity: 0.8\n");
    neural_network_step(net);
    printf("Output neuron state: %.4f (should be ~0)\n", NETWORK_INTERNAL(net)->neurons[1].state);

    // Open the gate
    neuron_t* post_neuron = &NETWORK_INTERNAL(net)->neurons[1];
    synapse_t* syn = &post_neuron->incoming_synapses[0];
    if (syn->compute_state) {
        syn->compute_state->local_memory[0] = 1.0f;  // Open gate
    }

    printf("\n--- Testing with GATE OPEN (gate = 1.0) ---\n");
    printf("Input activity: 0.8\n");
    neural_network_step(net);
    printf("Output neuron state: %.4f (should be high)\n", NETWORK_INTERNAL(net)->neurons[1].state);

    // Partial gate
    if (syn->compute_state) {
        syn->compute_state->local_memory[0] = 0.3f;  // 30% open
    }

    printf("\n--- Testing with PARTIAL GATE (gate = 0.3) ---\n");
    printf("Input activity: 0.8\n");
    neural_network_step(net);
    printf("Output neuron state: %.4f (should be moderate)\n", NETWORK_INTERNAL(net)->neurons[1].state);

    // Cleanup
    if (syn->compute_state) free(syn->compute_state);
    neural_network_destroy(net);
    printf("\n✓ Demo 3 complete!\n");
}

/**
 * Main entry point
 */
int main(int argc, char** argv) {
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║                                                           ║\n");
    printf("║   NIMCP 2.7 PROGRAMMABLE SYNAPSES DEMONSTRATION          ║\n");
    printf("║                                                           ║\n");
    printf("║   Revolutionary Feature: Synapses as Processors          ║\n");
    printf("║   Not Just Connections - Active Computational Units      ║\n");
    printf("║                                                           ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");

    // Run demos
    demo_attention_synapses();
    demo_neuromodulated_synapses();
    demo_gating_synapses();

    // Final summary
    print_divider("SUMMARY");
    printf("\nNIMCP 2.7 Programmable Synapses enable:\n\n");
    printf("  ✓ Attention mechanisms at synapse level\n");
    printf("  ✓ Neuromodulator-sensitive transmission\n");
    printf("  ✓ LSTM-like gating and routing\n");
    printf("  ✓ Context-dependent computation\n");
    printf("  ✓ Semantic similarity modulation\n");
    printf("  ✓ Custom synapse functions\n\n");

    printf("This transforms synapses from passive weights into active\n");
    printf("computational units - a game changer for cognitive tasks!\n\n");

    printf("═══════════════════════════════════════════════════════════\n");
    printf("Demo complete! Check the code to see implementation details.\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    return 0;
}
