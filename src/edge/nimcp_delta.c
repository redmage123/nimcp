/**
 * @file nimcp_delta.c
 * @brief Delta weight pushes — sparse weight update transport
 *
 * WHAT: Computes, applies, and (stub) compresses sparse weight differences
 *       between model versions.
 * WHY:  Sending full weight arrays over the network is expensive; delta pushes
 *       only transmit changed weights above a sparsity threshold.
 * HOW:  Compare old vs new weights, store indices + deltas for significant
 *       changes. Compression placeholder for future LZ4 integration.
 */

#include "edge/nimcp_edge.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <math.h>
#include <lz4.h>

/* ============================================================================
 * Compute Delta
 * ============================================================================ */

int nimcp_weight_delta_compute(const float* old_weights, const float* new_weights,
                                uint32_t num_weights, float sparsity_threshold,
                                nimcp_weight_delta_t* delta) {
    if (!old_weights || !new_weights || !delta || num_weights == 0) {
        return -1;
    }

    memset(delta, 0, sizeof(nimcp_weight_delta_t));

    /* First pass: count significant changes */
    uint32_t change_count = 0;
    for (uint32_t i = 0; i < num_weights; i++) {
        if (fabsf(new_weights[i] - old_weights[i]) > sparsity_threshold) {
            change_count++;
        }
    }

    if (change_count == 0) {
        delta->num_changes = 0;
        return 0;
    }

    /* Allocate sparse arrays */
    delta->layer_indices = (uint32_t*)nimcp_malloc(change_count * sizeof(uint32_t));
    delta->neuron_indices = (uint32_t*)nimcp_malloc(change_count * sizeof(uint32_t));
    delta->weight_deltas = (float*)nimcp_malloc(change_count * sizeof(float));

    if (!delta->layer_indices || !delta->neuron_indices || !delta->weight_deltas) {
        nimcp_free(delta->layer_indices);
        nimcp_free(delta->neuron_indices);
        nimcp_free(delta->weight_deltas);
        memset(delta, 0, sizeof(nimcp_weight_delta_t));
        return -1;
    }

    /* Second pass: collect changes */
    uint32_t idx = 0;
    for (uint32_t i = 0; i < num_weights; i++) {
        float d = new_weights[i] - old_weights[i];
        if (fabsf(d) > sparsity_threshold) {
            delta->layer_indices[idx] = 0;   /* Flat index; layer info TBD */
            delta->neuron_indices[idx] = i;
            delta->weight_deltas[idx] = d;
            idx++;
        }
    }

    delta->num_changes = change_count;
    delta->compressed_size = 0;
    delta->compressed_data = NULL;

    return 0;
}

/* ============================================================================
 * Apply Delta
 * ============================================================================ */

int nimcp_weight_delta_apply(float* weights, const nimcp_weight_delta_t* delta) {
    if (!weights || !delta) {
        return -1;
    }

    if (delta->num_changes == 0) {
        return 0;
    }

    if (!delta->neuron_indices || !delta->weight_deltas) {
        return -1;
    }

    for (uint32_t i = 0; i < delta->num_changes; i++) {
        uint32_t wi = delta->neuron_indices[i];
        weights[wi] += delta->weight_deltas[i];
    }

    return 0;
}

/* ============================================================================
 * Compress / Decompress (Stubs — real LZ4 integration later)
 * ============================================================================ */

int nimcp_weight_delta_compress(nimcp_weight_delta_t* delta) {
    if (!delta) {
        return -1;
    }

    if (delta->num_changes == 0 || !delta->weight_deltas) {
        delta->compressed_size = 0;
        return 0;
    }

    /* Already compressed — no-op */
    if (delta->compressed_data) {
        return 0;
    }

    int input_size = (int)(delta->num_changes * sizeof(float));
    int max_compressed = LZ4_compressBound(input_size);

    uint8_t* compressed_buf = (uint8_t*)nimcp_malloc((size_t)max_compressed);
    if (!compressed_buf) {
        return -1;
    }

    int compressed_size = LZ4_compress_default(
        (const char*)delta->weight_deltas, (char*)compressed_buf,
        input_size, max_compressed);

    if (compressed_size <= 0) {
        nimcp_free(compressed_buf);
        return -1;
    }

    delta->compressed_data = compressed_buf;
    delta->compressed_size = (uint32_t)compressed_size;

    return 0;
}

int nimcp_weight_delta_decompress(nimcp_weight_delta_t* delta) {
    if (!delta) {
        return -1;
    }

    /* No compressed data — already decompressed */
    if (!delta->compressed_data) {
        return 0;
    }

    if (delta->num_changes == 0 || !delta->weight_deltas) {
        return 0;
    }

    int output_size = (int)(delta->num_changes * sizeof(float));

    int decompressed = LZ4_decompress_safe(
        (const char*)delta->compressed_data, (char*)delta->weight_deltas,
        (int)delta->compressed_size, output_size);

    if (decompressed != output_size) {
        return -1;
    }

    nimcp_free(delta->compressed_data);
    delta->compressed_data = NULL;
    delta->compressed_size = 0;

    return 0;
}

/* ============================================================================
 * Cleanup
 * ============================================================================ */

void nimcp_weight_delta_destroy(nimcp_weight_delta_t* delta) {
    if (!delta) {
        return;
    }

    nimcp_free(delta->layer_indices);
    nimcp_free(delta->neuron_indices);
    nimcp_free(delta->weight_deltas);
    nimcp_free(delta->compressed_data);

    memset(delta, 0, sizeof(nimcp_weight_delta_t));
}
