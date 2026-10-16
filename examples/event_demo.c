//=============================================================================
// event_demo.c - NIMCP Event Packet System Demonstration
//=============================================================================
/**
 * @file event_demo.c
 * @brief Demonstrates sparse event-based neural communication
 *
 * WHAT THIS DEMONSTRATES:
 * - Event packet creation with feature codes
 * - Subscription filters for selective event reception
 * - Event generation from neural network spikes
 * - Event-to-neural input conversion
 * - Sparse, efficient neural communication
 *
 * WHY EVENT-BASED COMMUNICATION:
 * - Reduces bandwidth: Only transmit when neurons spike
 * - Biological realism: Real neurons communicate via spikes
 * - Energy efficient: ~100x less data than dense tensors
 * - Enables distributed neural networks across nodes
 *
 * ARCHITECTURE:
 * - Observer Pattern: Event subscription and callbacks
 * - Strategy Pattern: Filter evaluation
 * - Repository Pattern: Feature-to-neuron mappings
 *
 * COMPLEXITY:
 * - Event generation: O(k) where k = spiking neurons
 * - Event filtering: O(f) where f = active filters
 * - Event processing: O(1) per event
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "networking/events/nimcp_events.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "networking/protocol/nimcp_protocol.h"

// Global counter for received events
static uint32_t events_received = 0;

/**
 * @brief Callback for generated events
 */
static void event_generated_callback(const event_packet_t* packet, const void* payload,
                                     uint32_t payload_len, void* context)
{
    (void) payload;      // Unused
    (void) payload_len;  // Unused
    (void) context;      // Unused

    feature_code_t feature = EVENT_GET_FEATURE_CODE(packet);
    uint8_t domain = GET_FEATURE_DOMAIN(feature);
    uint16_t subcode = GET_FEATURE_SUBCODE(feature);
    float confidence = EVENT_CONFIDENCE_TO_FLOAT(packet->confidence);
    uint8_t flags = EVENT_GET_FLAGS(packet);

    printf("Event Generated:\n");
    printf("  Source Node: %u\n", packet->source_node_id);
    printf("  Feature Code: 0x%06X (Domain: 0x%02X, Subcode: 0x%04X)\n", feature, domain, subcode);
    printf("  Type: %s\n", (flags & EVENT_FLAG_EXCITATORY) ? "Excitatory" : "Inhibitory");
    printf("  Confidence: %.3f\n", confidence);
    printf("  Hop Count: %u\n", packet->hop_count);
    printf("  Timestamp: %lu\n\n", packet->timestamp);

    events_received++;
}

/**
 * @brief Main demonstration
 */
int main(void)
{
    printf("===========================================\n");
    printf(" NIMCP 2.0 Event Packet Demonstration\n");
    printf("===========================================\n\n");

    //=========================================================================
    // Step 1: Create a neural network
    //=========================================================================
    printf("Step 1: Creating neural network...\n");

    network_config_t net_config = {.num_neurons = 10,
                                   .input_size = 10,      // Required field
                                   .output_size = 10,     // Required field
                                   .ei_ratio = 0.8f,
                                   .learning_rate = 0.01f,
                                   .hebbian_rate = 0.1f,
                                   .stdp_window = 20.0f,
                                   .homeostatic_rate = 0.001f,
                                   .target_activity = 0.1f,
                                   .adaptation_rate = 0.1f,
                                   .refractory_period = 5.0f,
                                   .min_weight = -1.0f,
                                   .max_weight = 1.0f,
                                   .update_interval = 1000};

    neural_network_t network = neural_network_create(&net_config);
    if (!network) {
        fprintf(stderr, "Failed to create neural network\n");
        return EXIT_FAILURE;
    }
    printf("  Created network with %u neurons\n\n", net_config.num_neurons);

    //=========================================================================
    // Step 2: Set up event generator
    //=========================================================================
    printf("Step 2: Setting up event generator...\n");

    // Create generator config with Vision domain base code
    event_generator_config_t gen_config = {.node_id = 1,
                                           .base_feature_code =
                                               MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0),
                                           .max_hop_count = 10,
                                           .enable_plasticity_triggers = true,
                                           .callback = event_generated_callback,
                                           .callback_context = NULL};

    event_generator_t generator = event_generator_create(&gen_config);
    if (!generator) {
        fprintf(stderr, "Failed to create event generator\n");
        neural_network_destroy(network);
        return EXIT_FAILURE;
    }

    // Set custom feature codes for specific neurons
    event_generator_set_neuron_feature(
        generator, 0, MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x0001));  // Edge detection
    event_generator_set_neuron_feature(
        generator, 1, MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x0002));  // Vertical edge
    event_generator_set_neuron_feature(
        generator, 2, MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x1000));  // Motion detection

    printf("  Event generator configured with Vision domain\n");
    printf("  Node ID: %u\n", gen_config.node_id);
    printf("  Base feature: 0x%06X\n\n", gen_config.base_feature_code);

    //=========================================================================
    // Step 3: Create subscription filters
    //=========================================================================
    printf("Step 3: Creating subscription filters...\n");

    subscription_filter_t vision_filter = {.feature_code =
                                               MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0),
                                           .feature_mask = 0xFF0000,  // Match entire Vision domain
                                           .confidence_threshold = 0.5f,
                                           .max_rate_hz = 1000};

    subscription_filter_t edge_filter = {
        .feature_code = MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x0001),
        .feature_mask = 0xFFFFFF,  // Exact match for edge detection
        .confidence_threshold = 0.7f,
        .max_rate_hz = 500};

    printf("  Filter 1: Vision domain (0x%06X, mask 0x%06X)\n", vision_filter.feature_code,
           vision_filter.feature_mask);
    printf("  Filter 2: Edge detection (0x%06X, mask 0x%06X)\n\n", edge_filter.feature_code,
           edge_filter.feature_mask);

    //=========================================================================
    // Step 4: Create event receiver
    //=========================================================================
    printf("Step 4: Setting up event receiver...\n");

    subscription_filter_t filters[] = {vision_filter, edge_filter};
    event_receiver_config_t recv_config = {
        .network = network, .filters = filters, .num_filters = 2, .auto_create_neurons = false};

    event_receiver_t receiver = event_receiver_create(&recv_config);
    if (!receiver) {
        fprintf(stderr, "Failed to create event receiver\n");
        event_generator_destroy(generator);
        neural_network_destroy(network);
        return EXIT_FAILURE;
    }

    // Map feature codes to neurons
    event_receiver_map_feature_to_neuron(receiver, MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x0001),
                                         5);
    event_receiver_map_feature_to_neuron(receiver, MAKE_FEATURE_CODE(FEATURE_DOMAIN_VISION, 0x0002),
                                         6);

    printf("  Event receiver configured with %u filters\n", recv_config.num_filters);
    printf("  Feature-to-neuron mapping established\n\n");

    //=========================================================================
    // Step 5: Simulate neural activity and event generation
    //=========================================================================
    printf("Step 5: Simulating neural activity...\n\n");

    // Add some connections to make the network interesting
    neural_network_add_connection(network, 0, 1, 0.5f);
    neural_network_add_connection(network, 1, 2, 0.3f);
    neural_network_add_connection(network, 0, 2, 0.4f);

    // Simulate some timesteps
    for (uint64_t t = 0; t < 5; t++) {
        printf("--- Timestep %lu ---\n", t);

        // Update neurons with some input
        float input = (t % 2 == 0) ? 1.0f : 0.5f;
        neural_network_update_neuron(network, 0, input, t);
        neural_network_update_neuron(network, 1, input * 0.8f, t);
        neural_network_update_neuron(network, 2, input * 0.6f, t);

        // Check for spikes and generate events
        float state;
        for (uint32_t neuron = 0; neuron < 3; neuron++) {
            neural_network_get_neuron_state(network, neuron, &state);
            if (state > 0.5f) {  // Simple spike threshold
                event_generator_on_spike(generator, network, neuron, t);
            }
        }

        printf("\n");
    }

    //=========================================================================
    // Step 6: Display results
    //=========================================================================
    printf("===========================================\n");
    printf(" Summary\n");
    printf("===========================================\n");
    printf("Total events generated: %u\n", events_received);

    network_stats_t stats;
    neural_network_get_stats(network, &stats);
    printf("\nNetwork Statistics:\n");
    printf("  Neurons: %u (E: %u, I: %u)\n", stats.num_neurons, stats.num_excitatory,
           stats.num_inhibitory);
    printf("  Synapses: %u\n", stats.total_synapses);
    printf("  Avg Activity: %.4f\n", stats.avg_activity);
    printf("  Network Stability: %.4f\n", stats.network_stability);

    //=========================================================================
    // Cleanup
    //=========================================================================
    printf("\nCleaning up...\n");
    event_receiver_destroy(receiver);
    event_generator_destroy(generator);
    neural_network_destroy(network);

    printf("Demonstration complete!\n");
    return EXIT_SUCCESS;
}
