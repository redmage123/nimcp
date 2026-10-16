/**
 * @file nimcp_analogical_transfer.h
 * @brief Analogical Transfer Engine — structural similarity search for novel problems.
 *
 * WHAT: When facing a novel problem, search memory for structurally similar
 *       past experiences and import the solution strategy.
 * WHY:  Enables "This is LIKE something I've seen before" reasoning — a core
 *       human cognitive ability that accelerates learning in new domains.
 * HOW:  Stores problem-solution pairs as feature vectors, uses cosine similarity
 *       to find analogies, and blends analogical solutions with brain output.
 *
 * Thread-safe: No (single-threaded cognitive module).
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#ifndef NIMCP_ANALOGICAL_TRANSFER_H
#define NIMCP_ANALOGICAL_TRANSFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define ANALOGICAL_MAX_FEATURES   256
#define ANALOGICAL_MAX_SOLUTION   256
#define ANALOGICAL_MAX_LABEL       64

/* ============================================================================
 * Configuration
 * ============================================================================ */

typedef struct {
    uint32_t max_analogies;          /**< Max stored patterns (default 100) */
    float    similarity_threshold;   /**< Min cosine similarity to consider (default 0.6) */
    float    transfer_weight;        /**< Blend weight for analogical solution (default 0.3) */
} nimcp_analogical_config_t;

/* ============================================================================
 * Stored Pattern
 * ============================================================================ */

typedef struct {
    float    features[ANALOGICAL_MAX_FEATURES];   /**< Problem feature vector */
    uint32_t feat_dim;                             /**< Actual feature dimensionality */
    float    solution[ANALOGICAL_MAX_SOLUTION];   /**< Solution vector */
    uint32_t sol_dim;                              /**< Actual solution dimensionality */
    char     label[ANALOGICAL_MAX_LABEL];          /**< Human-readable label */
    float    success_score;                        /**< How well this solution worked [0-1] */
} nimcp_analogical_pattern_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

typedef struct nimcp_analogical_transfer nimcp_analogical_transfer_t;

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/**
 * @brief Get default configuration.
 * @return Config with max_analogies=100, similarity_threshold=0.6, transfer_weight=0.3.
 */
nimcp_analogical_config_t nimcp_analogical_config_default(void);

/**
 * @brief Create an analogical transfer engine.
 * @param config Configuration (NULL for defaults).
 * @return Handle, or NULL on failure.
 */
nimcp_analogical_transfer_t* nimcp_analogical_create(const nimcp_analogical_config_t* config);

/**
 * @brief Destroy engine and free all resources. NULL-safe.
 */
void nimcp_analogical_destroy(nimcp_analogical_transfer_t* handle);

/* ============================================================================
 * Pattern Storage
 * ============================================================================ */

/**
 * @brief Store a successful problem-solution pair.
 *
 * If buffer is full, the pattern with the lowest success_score is evicted.
 *
 * @param handle       Engine handle.
 * @param features     Problem feature vector.
 * @param feat_dim     Feature dimensionality (max ANALOGICAL_MAX_FEATURES).
 * @param solution     Solution vector.
 * @param sol_dim      Solution dimensionality (max ANALOGICAL_MAX_SOLUTION).
 * @param label        Human-readable label (max 63 chars).
 * @param success_score How well this solution worked [0-1].
 * @return 0 on success, -1 on error.
 */
int nimcp_analogical_store_pattern(nimcp_analogical_transfer_t* handle,
                                   const float* features, uint32_t feat_dim,
                                   const float* solution, uint32_t sol_dim,
                                   const char* label, float success_score);

/* ============================================================================
 * Analogy Search
 * ============================================================================ */

/**
 * @brief Search stored patterns for the best structural analogy.
 *
 * Computes cosine similarity between query_features and all stored patterns.
 * Returns the best match above similarity_threshold.
 *
 * @param handle            Engine handle.
 * @param query_features    Query feature vector.
 * @param feat_dim          Query dimensionality.
 * @param best_solution_out Output: best matching solution (caller-allocated, ANALOGICAL_MAX_SOLUTION).
 * @param sol_dim           Output: solution dimensionality.
 * @param best_label_out    Output: label of best match (caller-allocated, ANALOGICAL_MAX_LABEL).
 * @return Cosine similarity of best match (0.0 if none found above threshold).
 */
float nimcp_analogical_find_analogy(nimcp_analogical_transfer_t* handle,
                                    const float* query_features, uint32_t feat_dim,
                                    float* best_solution_out, uint32_t* sol_dim,
                                    char* best_label_out);

/* ============================================================================
 * Transfer Application
 * ============================================================================ */

/**
 * @brief Find best analogy and blend with brain output.
 *
 * blended = (1 - transfer_weight) * brain_output + transfer_weight * analogical_solution
 *
 * If no analogy is found above threshold, blended_output = brain_output (unmodified).
 *
 * @param handle          Engine handle.
 * @param query_features  Problem features for analogy search.
 * @param feat_dim        Feature dimensionality.
 * @param brain_output    Current brain output vector.
 * @param output_dim      Output dimensionality.
 * @param blended_output  Output: blended result (caller-allocated, output_dim floats).
 * @return Number of analogies found above threshold (0 or 1).
 */
int nimcp_analogical_apply_transfer(nimcp_analogical_transfer_t* handle,
                                    const float* query_features, uint32_t feat_dim,
                                    const float* brain_output, uint32_t output_dim,
                                    float* blended_output);

/* ============================================================================
 * Outcome Recording
 * ============================================================================ */

/**
 * @brief Update success_score for a stored pattern (reinforcement).
 *
 * Uses exponential moving average: score = 0.8 * old + 0.2 * success.
 *
 * @param handle  Engine handle.
 * @param label   Label of pattern to update.
 * @param success New success value [0-1].
 * @return 0 on success, -1 if label not found.
 */
int nimcp_analogical_record_outcome(nimcp_analogical_transfer_t* handle,
                                    const char* label, float success);

/* ============================================================================
 * Query
 * ============================================================================ */

/**
 * @brief Get the number of stored patterns.
 */
uint32_t nimcp_analogical_get_pattern_count(const nimcp_analogical_transfer_t* handle);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ANALOGICAL_TRANSFER_H */
