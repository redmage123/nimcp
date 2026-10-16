/**
 * @file nimcp_output_attention.c
 * @brief Output Attention Head — learned per-task attention that focuses
 *        on relevant output dimensions for each task type.
 *
 * Simple attention: score[i] = sigmoid(W[task] . output[i])
 *                   attended[i] = output[i] * score[i]
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#define LOG_MODULE "OUTPUT_ATTENTION"

#include "cognitive/nimcp_output_attention.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

#include <string.h>
#include <math.h>

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct nimcp_output_attention {
    nimcp_oa_config_t config;

    /* Per-task learned weight vectors: [max_tasks × total_dim] where
     * total_dim = num_heads × head_dim. Each task has one weight vector
     * that produces attention scores for all output dimensions. */
    float* attention_weights;
    uint32_t total_dim;          /* num_heads * head_dim */

    /* Task label -> task index mapping (hash + label for collision detection) */
    uint32_t task_hashes[NIMCP_OA_MAX_TASKS];
    char     task_labels[NIMCP_OA_MAX_TASKS][64];
    uint32_t num_tasks;
};

/* ============================================================================
 * Helpers
 * ============================================================================ */

static float sigmoidf(float x)
{
    if (x > 20.0f) return 1.0f;
    if (x < -20.0f) return 0.0f;
    return 1.0f / (1.0f + expf(-x));
}

static uint32_t hash_label(const char* label)
{
    if (!label) return 0;
    /* FNV-1a 32-bit */
    uint32_t h = 2166136261u;
    for (const char* p = label; *p; p++) {
        h ^= (uint32_t)(unsigned char)*p;
        h *= 16777619u;
    }
    return h;
}

/**
 * @brief Get or create task index for a label.
 * @return Task index [0, MAX_TASKS), or -1 if table full.
 */
static int get_task_index(nimcp_output_attention_t* oa, const char* task_label)
{
    uint32_t h = hash_label(task_label);

    /* Search existing — compare both hash AND label string for collision safety */
    for (uint32_t i = 0; i < oa->num_tasks; i++) {
        if (oa->task_hashes[i] == h &&
            strncmp(oa->task_labels[i], task_label, 63) == 0) {
            return (int)i;
        }
    }

    /* Create new */
    if (oa->num_tasks >= NIMCP_OA_MAX_TASKS) {
        LOG_WARN("Task table full (%u tasks), cannot add '%s'",
                 NIMCP_OA_MAX_TASKS, task_label ? task_label : "(null)");
        return -1;
    }

    uint32_t idx = oa->num_tasks;
    oa->task_hashes[idx] = h;
    strncpy(oa->task_labels[idx], task_label, 63);
    oa->task_labels[idx][63] = '\0';
    oa->num_tasks++;

    /* Initialize weights for new task to small random-ish values.
     * Use hash-based deterministic init for reproducibility. */
    float* w = oa->attention_weights + (size_t)idx * oa->total_dim;
    for (uint32_t j = 0; j < oa->total_dim; j++) {
        /* Simple deterministic "random" init from hash */
        uint32_t seed = h ^ (j * 2654435761u);
        float val = ((float)(seed & 0xFFFF) / 65535.0f - 0.5f) * 0.1f;
        w[j] = val;
    }

    return (int)idx;
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

nimcp_oa_config_t nimcp_oa_config_default(void)
{
    nimcp_oa_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.num_heads          = 4;
    cfg.head_dim           = 1024;
    cfg.task_embedding_dim = 64;
    return cfg;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

nimcp_output_attention_t* nimcp_oa_create(const nimcp_oa_config_t* config)
{
    nimcp_output_attention_t* oa = (nimcp_output_attention_t*)nimcp_calloc(
        1, sizeof(nimcp_output_attention_t));
    if (!oa) {
        LOG_ERROR("Failed to allocate output attention");
        return NULL;
    }

    if (config) {
        oa->config = *config;
    } else {
        oa->config = nimcp_oa_config_default();
    }

    /* Validate */
    if (oa->config.num_heads == 0) oa->config.num_heads = 4;
    if (oa->config.head_dim == 0) oa->config.head_dim = 1024;

    oa->total_dim = oa->config.num_heads * oa->config.head_dim;
    if (oa->total_dim > NIMCP_OA_MAX_HEAD_DIM) {
        oa->total_dim = NIMCP_OA_MAX_HEAD_DIM;
    }

    /* Allocate weight matrix: MAX_TASKS x total_dim */
    size_t weight_bytes = (size_t)NIMCP_OA_MAX_TASKS * oa->total_dim * sizeof(float);
    oa->attention_weights = (float*)nimcp_calloc(1, weight_bytes);
    if (!oa->attention_weights) {
        LOG_ERROR("Failed to allocate attention weights (%zu bytes)", weight_bytes);
        nimcp_free(oa);
        return NULL;
    }

    oa->num_tasks = 0;
    memset(oa->task_hashes, 0, sizeof(oa->task_hashes));
    memset(oa->task_labels, 0, sizeof(oa->task_labels));

    LOG_INFO("Created output attention (heads=%u, head_dim=%u, total_dim=%u)",
             oa->config.num_heads, oa->config.head_dim, oa->total_dim);

    return oa;
}

void nimcp_oa_destroy(nimcp_output_attention_t* oa)
{
    if (!oa) {
        return;
    }
    if (oa->attention_weights) {
        nimcp_free(oa->attention_weights);
    }
    nimcp_free(oa);
}

/* ============================================================================
 * Attention
 * ============================================================================ */

int nimcp_oa_attend(nimcp_output_attention_t* oa,
    const float* brain_output, uint32_t output_dim,
    const char* task_label,
    float* attended_output)
{
    if (!oa || !brain_output || !attended_output || !task_label) {
        return -1;
    }
    if (output_dim == 0) {
        return -1;
    }

    int task_idx = get_task_index(oa, task_label);
    if (task_idx < 0) {
        return -1;
    }

    const float* w = oa->attention_weights + (size_t)task_idx * oa->total_dim;
    uint32_t dim = output_dim < oa->total_dim ? output_dim : oa->total_dim;

    /* Compute attention scores and apply */
    for (uint32_t i = 0; i < output_dim; i++) {
        if (i < dim) {
            float score = sigmoidf(w[i] * brain_output[i]);
            attended_output[i] = brain_output[i] * score;
        } else {
            /* Beyond weight coverage: pass through with default 0.5 sigmoid */
            attended_output[i] = brain_output[i] * 0.5f;
        }
    }

    return 0;
}

/* ============================================================================
 * Training
 * ============================================================================ */

float nimcp_oa_train_attention(nimcp_output_attention_t* oa,
    const float* brain_output, uint32_t output_dim,
    const char* task_label,
    const float* target, uint32_t target_dim,
    float lr)
{
    if (!oa || !brain_output || !task_label || !target) {
        return -1.0f;
    }
    if (output_dim == 0 || target_dim == 0) {
        return -1.0f;
    }

    int task_idx = get_task_index(oa, task_label);
    if (task_idx < 0) {
        return -1.0f;
    }

    float* w = oa->attention_weights + (size_t)task_idx * oa->total_dim;
    uint32_t dim = output_dim < oa->total_dim ? output_dim : oa->total_dim;
    uint32_t cmp_dim = dim < target_dim ? dim : target_dim;

    float total_loss = 0.0f;

    for (uint32_t i = 0; i < cmp_dim; i++) {
        float score = sigmoidf(w[i] * brain_output[i]);
        float attended = brain_output[i] * score;
        float error = attended - target[i];
        total_loss += error * error;

        /* Gradient: dL/dW_i = 2 * error * output_i * output_i * sigmoid'(w*o) */
        float sig_prime = score * (1.0f - score);
        float grad = 2.0f * error * brain_output[i] * brain_output[i] * sig_prime;

        /* Clamp gradient for stability */
        if (grad > 10.0f) grad = 10.0f;
        if (grad < -10.0f) grad = -10.0f;

        w[i] -= lr * grad;
    }

    total_loss /= (float)(cmp_dim > 0 ? cmp_dim : 1);
    return total_loss;
}
