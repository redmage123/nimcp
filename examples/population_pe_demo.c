/**
 * @file population_pe_demo.c
 * @brief Demonstration of positional encoding integration with population coding
 *
 * WHAT: Example usage of position-aware population coding
 * WHY:  Show how to use PE to encode spatial organization of neural populations
 * HOW:  Create encoder, configure PE, encode positions, decode with position awareness
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "middleware/encoding/nimcp_population_coding.h"

#define NUM_NEURONS 100
#define PE_DIM 64

/**
 * WHAT: Demo of position-aware population coding
 * WHY:  Illustrate how positional encoding enhances population vector decoding
 * HOW:  Create population, encode positions, decode with position weighting
 */
int main(void) {
    printf("=== Population Coding with Positional Encoding Demo ===\n\n");

    // Create population coding encoder
    population_coding_config_t config = population_coding_default_config();
    population_coding_encoder_t encoder = population_coding_create(&config);
    if (!encoder) {
        fprintf(stderr, "Failed to create encoder\n");
        return 1;
    }

    printf("Step 1: Configure positional encoding\n");
    printf("  - Embedding dimension: %d\n", PE_DIM);
    printf("  - Frequency base: 10000.0\n");
    printf("  - Position weight: 0.4\n\n");

    // Configure positional encoding
    if (!population_coding_set_pe_config(encoder, PE_DIM, 10000.0f, 0.4f)) {
        fprintf(stderr, "Failed to configure PE\n");
        population_coding_destroy(encoder);
        return 1;
    }

    printf("Step 2: Create neuron population with tuning curves\n");

    // Create tuning curves for neurons (uniform distribution around unit circle)
    tuning_curve_t* tuning_curves = malloc(NUM_NEURONS * sizeof(tuning_curve_t));
    if (!tuning_curves) {
        fprintf(stderr, "Failed to allocate tuning curves\n");
        population_coding_destroy(encoder);
        return 1;
    }

    for (uint32_t i = 0; i < NUM_NEURONS; i++) {
        float angle = 2.0f * M_PI * (float)i / (float)NUM_NEURONS;
        tuning_curves[i].preferred_direction.x = cosf(angle);
        tuning_curves[i].preferred_direction.y = sinf(angle);
        tuning_curves[i].preferred_direction.z = 0.0f;
        tuning_curves[i].preferred_direction.magnitude = 1.0f;
        tuning_curves[i].tuning_width = M_PI / 4.0f;  // 45 degree tuning
        tuning_curves[i].max_rate = 100.0f;
    }

    printf("  - Created %d neurons with uniform directional tuning\n\n", NUM_NEURONS);

    printf("Step 3: Encode neuron positions using PE\n");

    // Encode neuron positions
    float* position_encodings = malloc(NUM_NEURONS * PE_DIM * sizeof(float));
    if (!position_encodings) {
        fprintf(stderr, "Failed to allocate position encodings\n");
        free(tuning_curves);
        population_coding_destroy(encoder);
        return 1;
    }

    if (!population_coding_encode_neuron_positions(encoder, NUM_NEURONS, position_encodings)) {
        fprintf(stderr, "Failed to encode neuron positions\n");
        free(position_encodings);
        free(tuning_curves);
        population_coding_destroy(encoder);
        return 1;
    }

    printf("  - Encoded positions for %d neurons\n", NUM_NEURONS);
    printf("  - Position encoding shape: [%d x %d]\n\n", NUM_NEURONS, PE_DIM);

    printf("Step 4: Generate firing rates for target direction (45 degrees)\n");

    // Create firing rates for a target direction
    float* rates = malloc(NUM_NEURONS * sizeof(float));
    if (!rates) {
        fprintf(stderr, "Failed to allocate rates\n");
        free(position_encodings);
        free(tuning_curves);
        population_coding_destroy(encoder);
        return 1;
    }

    // Target direction: 45 degrees
    vector3d_t target_dir;
    target_dir.x = cosf(M_PI / 4.0f);
    target_dir.y = sinf(M_PI / 4.0f);
    target_dir.z = 0.0f;
    target_dir.magnitude = 1.0f;

    // Generate rates based on cosine tuning
    for (uint32_t i = 0; i < NUM_NEURONS; i++) {
        float dot = population_coding_vector3d_dot(&target_dir, &tuning_curves[i].preferred_direction);
        rates[i] = tuning_curves[i].max_rate * fmaxf(0.0f, dot);
    }

    printf("  - Target direction: (%.2f, %.2f, %.2f)\n", target_dir.x, target_dir.y, target_dir.z);
    printf("  - Generated rates with cosine tuning\n\n");

    printf("Step 5: Decode with position-aware weighting\n");

    // Create query position encoding (position 25 - favoring neurons near this index)
    float* query_position = malloc(PE_DIM * sizeof(float));
    if (!query_position) {
        fprintf(stderr, "Failed to allocate query position\n");
        free(rates);
        free(position_encodings);
        free(tuning_curves);
        population_coding_destroy(encoder);
        return 1;
    }

    // Copy position encoding of neuron 25 as query
    for (uint32_t d = 0; d < PE_DIM; d++) {
        query_position[d] = position_encodings[25 * PE_DIM + d];
    }

    // Decode with position awareness
    vector3d_t decoded_vector;
    if (!population_coding_position_aware_decode(
        encoder,
        rates,
        position_encodings,
        NUM_NEURONS,
        query_position,
        tuning_curves,
        &decoded_vector
    )) {
        fprintf(stderr, "Failed to decode with position awareness\n");
        free(query_position);
        free(rates);
        free(position_encodings);
        free(tuning_curves);
        population_coding_destroy(encoder);
        return 1;
    }

    printf("  - Query position: neuron index 25\n");
    printf("  - Decoded vector: (%.3f, %.3f, %.3f)\n", decoded_vector.x, decoded_vector.y, decoded_vector.z);
    printf("  - Magnitude: %.3f\n\n", decoded_vector.magnitude);

    printf("Step 6: Compare with standard decoding (no position weighting)\n");

    // Standard vector sum encoding for comparison
    vector3d_t standard_vector;
    if (!population_coding_encode_vector_sum(
        encoder,
        rates,
        tuning_curves,
        NUM_NEURONS,
        &standard_vector
    )) {
        fprintf(stderr, "Failed to encode standard vector sum\n");
        free(query_position);
        free(rates);
        free(position_encodings);
        free(tuning_curves);
        population_coding_destroy(encoder);
        return 1;
    }

    printf("  - Standard vector: (%.3f, %.3f, %.3f)\n", standard_vector.x, standard_vector.y, standard_vector.z);
    printf("  - Magnitude: %.3f\n\n", standard_vector.magnitude);

    printf("Results:\n");
    printf("  - Target direction:    (%.3f, %.3f, %.3f)\n", target_dir.x, target_dir.y, target_dir.z);
    printf("  - Standard decoding:   (%.3f, %.3f, %.3f)\n", standard_vector.x, standard_vector.y, standard_vector.z);
    printf("  - Position-aware:      (%.3f, %.3f, %.3f)\n", decoded_vector.x, decoded_vector.y, decoded_vector.z);
    printf("\n");

    printf("Biological Interpretation:\n");
    printf("  - Position-aware decoding weights neurons by spatial proximity\n");
    printf("  - Simulates how cortical readout may favor nearby neurons\n");
    printf("  - Similar to attention mechanisms in spatial processing\n");
    printf("  - Useful for modeling place cells, grid cells, and topographic maps\n");

    // Cleanup
    free(query_position);
    free(rates);
    free(position_encodings);
    free(tuning_curves);
    population_coding_destroy(encoder);

    printf("\n=== Demo completed successfully ===\n");
    return 0;
}
