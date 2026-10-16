/**
 * @file nimcp_early_exit.c
 * @brief Early exit heads for adaptive computation depth on edge devices.
 *
 * Each exit head is a linear projection from a hidden layer to the output
 * space, followed by softmax for confidence estimation. If confidence
 * exceeds the threshold, inference terminates early.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#include "edge/nimcp_edge.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

/* ============================================================================
 * Helper: Xavier uniform initialization
 * fan_in = layer_size, fan_out = output_size
 * range = sqrt(6 / (fan_in + fan_out))
 * ============================================================================ */

static float xavier_uniform(uint32_t fan_in, uint32_t fan_out) {
    uint32_t total = fan_in + fan_out;
    if (total == 0) return 0.0f;
    float limit = sqrtf(6.0f / (float)total);
    /* rand() is not ideal but sufficient for initialization */
    float r = (float)rand() / (float)RAND_MAX; /* [0, 1] */
    return (2.0f * r - 1.0f) * limit;          /* [-limit, +limit] */
}

/* ============================================================================
 * nimcp_early_exit_create
 * ============================================================================ */

nimcp_early_exit_t* nimcp_early_exit_create(
    const uint32_t* exit_layers, const float* thresholds,
    const uint32_t* layer_sizes, uint32_t num_exits, uint32_t output_size)
{
    if (!exit_layers || !thresholds || !layer_sizes || num_exits == 0 || output_size == 0) {
        return NULL;
    }

    if (num_exits > 8) {
        num_exits = 8; /* Clamp to max exit points */
    }

    nimcp_early_exit_t* ee = (nimcp_early_exit_t*)nimcp_calloc(1, sizeof(nimcp_early_exit_t));
    if (!ee) {
        return NULL;
    }

    ee->num_exits = num_exits;
    ee->output_size = output_size;
    ee->enabled = true;
    ee->total_inferences = 0;
    ee->full_depth_count = 0;

    for (uint32_t i = 0; i < num_exits; i++) {
        ee->exit_layers[i] = exit_layers[i];
        ee->confidence_thresholds[i] = thresholds[i];
        ee->early_exits[i] = 0;

        uint32_t ls = layer_sizes[i];

        /* Overflow check for weight matrix allocation */
        if (ls > 0 && output_size > UINT32_MAX / ls / sizeof(float)) {
            nimcp_early_exit_destroy(ee);
            return NULL;
        }

        /* Allocate weight matrix: layer_size x output_size */
        ee->exit_weights[i] = (float*)nimcp_malloc(ls * output_size * sizeof(float));
        if (!ee->exit_weights[i]) {
            nimcp_early_exit_destroy(ee);
            return NULL;
        }

        /* Xavier initialization */
        for (uint32_t j = 0; j < ls * output_size; j++) {
            ee->exit_weights[i][j] = xavier_uniform(ls, output_size);
        }

        /* Allocate bias vector: output_size */
        ee->exit_biases[i] = (float*)nimcp_calloc(output_size, sizeof(float));
        if (!ee->exit_biases[i]) {
            nimcp_early_exit_destroy(ee);
            return NULL;
        }
        /* Biases initialized to zero by calloc */
    }

    return ee;
}

/* ============================================================================
 * nimcp_early_exit_destroy
 * ============================================================================ */

void nimcp_early_exit_destroy(nimcp_early_exit_t* ee) {
    if (!ee) {
        return;
    }

    for (uint32_t i = 0; i < ee->num_exits; i++) {
        if (ee->exit_weights[i]) {
            nimcp_free(ee->exit_weights[i]);
            ee->exit_weights[i] = NULL;
        }
        if (ee->exit_biases[i]) {
            nimcp_free(ee->exit_biases[i]);
            ee->exit_biases[i] = NULL;
        }
    }

    nimcp_free(ee);
}

/* ============================================================================
 * nimcp_early_exit_evaluate
 *
 * Linear projection: output = activation * W + bias
 * Then softmax confidence = max(softmax(output))
 * Returns exit_idx if confidence > threshold, -1 otherwise.
 * ============================================================================ */

int nimcp_early_exit_evaluate(
    nimcp_early_exit_t* ee, uint32_t exit_idx,
    const float* layer_activation, uint32_t layer_size,
    float* output, float* confidence)
{
    if (!ee || !ee->enabled || !layer_activation || !output || !confidence ||
        ee->output_size == 0 || layer_size == 0) {
        return -1;
    }

    if (exit_idx >= ee->num_exits) {
        return -1;
    }

    const float* W = ee->exit_weights[exit_idx];
    const float* b = ee->exit_biases[exit_idx];
    uint32_t out_size = ee->output_size;

    if (!W || !b) {
        return -1;
    }

    /* Matrix-vector multiply: output[j] = sum_i(activation[i] * W[i * out_size + j]) + bias[j] */
    for (uint32_t j = 0; j < out_size; j++) {
        float sum = b[j];
        for (uint32_t i = 0; i < layer_size; i++) {
            sum += layer_activation[i] * W[i * out_size + j];
        }
        output[j] = sum;
    }

    /* Softmax with numerical stability (subtract max) */
    float max_val = output[0];
    for (uint32_t j = 1; j < out_size; j++) {
        if (output[j] > max_val) {
            max_val = output[j];
        }
    }

    float sum_exp = 0.0f;
    for (uint32_t j = 0; j < out_size; j++) {
        output[j] = expf(output[j] - max_val);
        sum_exp += output[j];
    }

    if (sum_exp < 1e-10f) {
        sum_exp = 1e-10f;
    }

    float max_prob = 0.0f;
    for (uint32_t j = 0; j < out_size; j++) {
        output[j] /= sum_exp;
        if (output[j] > max_prob) {
            max_prob = output[j];
        }
    }

    *confidence = max_prob;

    /* Update stats */
    ee->total_inferences++;

    /* Check threshold */
    if (max_prob >= ee->confidence_thresholds[exit_idx]) {
        ee->early_exits[exit_idx]++;
        return (int)exit_idx;
    }

    return -1; /* No early exit */
}

/* ============================================================================
 * nimcp_early_exit_get_stats
 * ============================================================================ */

void nimcp_early_exit_get_stats(
    const nimcp_early_exit_t* ee,
    uint64_t* total, uint64_t* early, uint64_t* full)
{
    if (!ee) {
        if (total) *total = 0;
        if (early) *early = 0;
        if (full)  *full  = 0;
        return;
    }

    if (total) {
        *total = ee->total_inferences;
    }

    if (early) {
        uint64_t sum = 0;
        for (uint32_t i = 0; i < ee->num_exits; i++) {
            sum += ee->early_exits[i];
        }
        *early = sum;
    }

    if (full) {
        uint64_t sum_early = 0;
        for (uint32_t i = 0; i < ee->num_exits; i++) {
            sum_early += ee->early_exits[i];
        }
        *full = ee->total_inferences - sum_early;
    }
}
