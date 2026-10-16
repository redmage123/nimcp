/**
 * @file nimcp_output_attention.h
 * @brief Output Attention Head — learned per-task attention weights that
 *        focus on relevant output neurons for each task type.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#ifndef NIMCP_OUTPUT_ATTENTION_H
#define NIMCP_OUTPUT_ATTENTION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NIMCP_OA_MAX_TASKS   64
#define NIMCP_OA_MAX_HEAD_DIM 4096

/* ============================================================================
 * Configuration
 * ============================================================================ */

typedef struct {
    uint32_t num_heads;            /**< Number of attention heads (default 4) */
    uint32_t head_dim;             /**< Neurons per head (default 1024) */
    uint32_t task_embedding_dim;   /**< Task embedding size (default 64) */
} nimcp_oa_config_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

typedef struct nimcp_output_attention nimcp_output_attention_t;

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/**
 * @brief Create an output attention module.
 * @param config Configuration. NULL uses defaults.
 * @return Handle, or NULL on failure.
 */
nimcp_output_attention_t* nimcp_oa_create(const nimcp_oa_config_t* config);

/**
 * @brief Destroy an output attention module. NULL-safe.
 */
void nimcp_oa_destroy(nimcp_output_attention_t* oa);

/**
 * @brief Return default configuration.
 */
nimcp_oa_config_t nimcp_oa_config_default(void);

/* ============================================================================
 * Attention
 * ============================================================================ */

/**
 * @brief Apply learned attention to brain output for a given task.
 *
 * Computes attention_score[i] = sigmoid(W[task_type] . output[i]),
 * then attended_output[i] = output[i] * attention_score[i].
 *
 * @param oa              Handle.
 * @param brain_output    Raw brain output vector.
 * @param output_dim      Dimension of brain_output.
 * @param task_label      Task name (hashed to task index).
 * @param attended_output Output buffer (same size as brain_output).
 * @return 0 on success, -1 on failure.
 */
int nimcp_oa_attend(nimcp_output_attention_t* oa,
    const float* brain_output, uint32_t output_dim,
    const char* task_label,
    float* attended_output);

/**
 * @brief Train attention weights to focus on dimensions that reduce loss.
 *
 * Gradient: dL/dW = (attended - target) * output * sigmoid'(score)
 *
 * @param oa           Handle.
 * @param brain_output Raw brain output.
 * @param output_dim   Dimension of output.
 * @param task_label   Task name.
 * @param target       Target output vector.
 * @param target_dim   Dimension of target.
 * @param lr           Learning rate.
 * @return Loss value (>= 0), or -1.0f on failure.
 */
float nimcp_oa_train_attention(nimcp_output_attention_t* oa,
    const float* brain_output, uint32_t output_dim,
    const char* task_label,
    const float* target, uint32_t target_dim,
    float lr);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OUTPUT_ATTENTION_H */
