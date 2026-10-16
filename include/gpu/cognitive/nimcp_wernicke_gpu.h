/**
 * @file nimcp_wernicke_gpu.h
 * @brief GPU-Accelerated Wernicke's Region API
 *
 * WHAT: C API for GPU-accelerated Wernicke's region operations
 * WHY:  GPU acceleration for language comprehension hot paths
 * HOW:  Wraps CUDA kernels for parallel phoneme recognition, lexical access, semantic activation
 *
 * BIOLOGICAL BASIS:
 * =================
 * Wernicke's region (posterior STG, BA22) processes language comprehension through:
 * - Phoneme recognition: Parallel pattern matching against phoneme templates
 * - Lexical access: Cohort-based parallel word recognition
 * - Semantic activation: Spreading activation across concept network
 * - Context integration: Parallel context embedding updates
 *
 * GPU ACCELERATION RATIONALE:
 * ===========================
 * Language comprehension involves parallel processing of:
 * - Multiple phoneme candidates during recognition
 * - Word cohort narrowing from phoneme sequences
 * - Semantic spreading across thousands of concepts
 * - Attention-weighted precision computation
 *
 * USAGE:
 * ======
 * @code
 * nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(0);
 * wernicke_gpu_context_t* wernicke_gpu = wernicke_gpu_create(ctx, &config);
 *
 * // Batch phoneme recognition
 * wernicke_gpu_recognize_phonemes(wernicke_gpu, spectral_features, num_frames, phonemes);
 *
 * // Parallel cohort word recognition
 * wernicke_gpu_recognize_words(wernicke_gpu, phonemes, num_phonemes, candidates, &count);
 *
 * // GPU semantic spreading activation
 * wernicke_gpu_spread_activation(wernicke_gpu, seed_concepts, num_seeds, activations);
 *
 * wernicke_gpu_destroy(wernicke_gpu);
 * nimcp_gpu_context_destroy(ctx);
 * @endcode
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2026-01-05
 */

#ifndef NIMCP_WERNICKE_GPU_H
#define NIMCP_WERNICKE_GPU_H

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>

/*=============================================================================
 * Forward Declarations
 *=============================================================================*/

/** @brief Opaque GPU Wernicke context */
typedef struct wernicke_gpu_context wernicke_gpu_context_t;

/*=============================================================================
 * Constants
 *=============================================================================*/

/** @brief Maximum phoneme embedding dimension */
#define WERNICKE_GPU_MAX_PHONEME_DIM       64

/** @brief Maximum word embedding dimension */
#define WERNICKE_GPU_MAX_WORD_DIM          256

/** @brief Maximum semantic embedding dimension */
#define WERNICKE_GPU_MAX_SEMANTIC_DIM      512

/** @brief Default number of phoneme categories */
#define WERNICKE_GPU_DEFAULT_NUM_PHONEMES  44

/*=============================================================================
 * Configuration
 *=============================================================================*/

/**
 * @brief GPU Wernicke configuration
 */
typedef struct {
    /* Phoneme recognition */
    uint32_t num_phoneme_categories;     /**< Number of phoneme categories (default: 44) */
    uint32_t phoneme_embedding_dim;      /**< Phoneme embedding dimension */
    uint32_t max_spectral_frames;        /**< Max spectral frames per batch */

    /* Lexical access */
    uint32_t max_lexicon_size;           /**< Maximum lexicon entries on GPU */
    uint32_t max_cohort_size;            /**< Maximum word cohort size */
    uint32_t word_embedding_dim;         /**< Word embedding dimension */
    uint32_t max_phonemes_per_word;      /**< Max phonemes per word (default: 16) */

    /* Semantic activation */
    uint32_t max_concepts;               /**< Maximum concepts in semantic network */
    uint32_t semantic_embedding_dim;     /**< Semantic embedding dimension */
    uint32_t spreading_iterations;       /**< Spreading activation iterations */
    float spreading_decay;               /**< Activation decay per iteration */

    /* Attention/Precision */
    bool enable_attention;               /**< Enable GPU attention computation */
    uint32_t attention_heads;            /**< Number of attention heads */

    /* Working memory */
    uint32_t working_memory_slots;       /**< GPU phonological WM slots */
    float wm_decay_rate;                 /**< WM decay rate per update */

    /* Transfer */
    bool enable_async_transfer;          /**< Enable async CPU-GPU transfers */

    /* Batch processing */
    uint32_t max_batch_size;             /**< Maximum batch size */
} wernicke_gpu_config_t;

/**
 * @brief Get default GPU Wernicke configuration
 */
NIMCP_EXPORT wernicke_gpu_config_t wernicke_gpu_default_config(void);

/*=============================================================================
 * GPU-Compatible Structures
 *=============================================================================*/

/**
 * @brief GPU spectral frame (for phoneme recognition)
 */
typedef struct {
    float mel_bands[40];                 /**< Mel-frequency bands */
    float mfcc[13];                      /**< MFCC coefficients */
    float delta[13];                     /**< Delta coefficients */
    float delta_delta[13];               /**< Delta-delta coefficients */
    float energy;                        /**< Frame energy */
    float pitch;                         /**< Pitch estimate (F0) */
    float voicing;                       /**< Voicing probability */
    uint8_t _padding[4];                 /**< Alignment padding */
} wernicke_gpu_spectral_frame_t;

/**
 * @brief GPU phoneme recognition result
 */
typedef struct {
    uint8_t phoneme_id;                  /**< Recognized phoneme ID */
    uint8_t _padding[3];                 /**< Alignment */
    float confidence;                    /**< Recognition confidence [0-1] */
    float* posterior;                    /**< Posterior over all phonemes (optional) */
} wernicke_gpu_phoneme_result_t;

/**
 * @brief GPU lexical entry (word in lexicon)
 */
typedef struct {
    uint32_t word_id;                    /**< Unique word identifier */
    uint8_t phonemes[16];                /**< Phoneme sequence (fixed for GPU) */
    uint32_t phoneme_count;              /**< Number of phonemes */
    float frequency;                     /**< Word frequency [0-1] */
    float activation;                    /**< Current activation level */
    uint32_t concept_id;                 /**< Primary concept link */
    uint32_t _padding;                   /**< Alignment */
} wernicke_gpu_lexical_entry_t;

/**
 * @brief GPU word recognition candidate
 */
typedef struct {
    uint32_t word_id;                    /**< Word identifier */
    float cohort_probability;            /**< Probability in cohort */
    float uniqueness_point;              /**< Uniqueness point reached [0-1] */
    uint8_t matched_phonemes;            /**< Phonemes matched so far */
    bool recognition_complete;           /**< Word fully recognized */
    uint8_t _padding[2];                 /**< Alignment */
} wernicke_gpu_word_candidate_t;

/**
 * @brief GPU concept entry (for semantic network)
 */
typedef struct {
    uint32_t concept_id;                 /**< Unique concept identifier */
    float activation;                    /**< Current activation level */
    float* embedding;                    /**< Semantic embedding (optional) */
    uint32_t* neighbors;                 /**< Connected concept IDs */
    float* edge_weights;                 /**< Connection weights */
    uint32_t num_neighbors;              /**< Number of connections */
} wernicke_gpu_concept_t;

/**
 * @brief GPU semantic activation result
 */
typedef struct {
    uint32_t concept_id;                 /**< Concept identifier */
    float activation;                    /**< Final activation level */
    float spreading_contribution;        /**< Activation from spreading */
} wernicke_gpu_activation_result_t;

/*=============================================================================
 * Lifecycle Functions
 *=============================================================================*/

/**
 * @brief Create GPU Wernicke context
 *
 * @param gpu_ctx GPU context (required)
 * @param config Configuration (NULL for defaults)
 * @return New GPU Wernicke context, or NULL on failure
 */
NIMCP_EXPORT wernicke_gpu_context_t* wernicke_gpu_create(
    nimcp_gpu_context_t* gpu_ctx,
    const wernicke_gpu_config_t* config
);

/**
 * @brief Destroy GPU Wernicke context
 *
 * @param ctx GPU Wernicke context to destroy
 */
NIMCP_EXPORT void wernicke_gpu_destroy(wernicke_gpu_context_t* ctx);

/**
 * @brief Synchronize GPU operations
 *
 * @param ctx GPU Wernicke context
 * @return true on success
 */
NIMCP_EXPORT bool wernicke_gpu_synchronize(wernicke_gpu_context_t* ctx);

/*=============================================================================
 * Phoneme Embedding Management
 *=============================================================================*/

/**
 * @brief Upload phoneme embeddings to GPU
 *
 * @param ctx GPU Wernicke context
 * @param embeddings Phoneme embedding matrix [num_phonemes x embed_dim]
 * @param num_phonemes Number of phoneme categories
 * @param embed_dim Embedding dimension
 * @return true on success
 */
NIMCP_EXPORT bool wernicke_gpu_upload_phoneme_embeddings(
    wernicke_gpu_context_t* ctx,
    const float* embeddings,
    uint32_t num_phonemes,
    uint32_t embed_dim
);

/*=============================================================================
 * Parallel Phoneme Recognition
 *=============================================================================*/

/**
 * @brief Batch phoneme recognition (GPU-accelerated)
 *
 * WHAT: Recognize phonemes from spectral features in parallel
 * WHY:  Real-time phoneme recognition requires parallel processing
 * HOW:  GPU kernel computes similarity to phoneme templates
 *
 * @param ctx GPU Wernicke context
 * @param frames Array of spectral frames
 * @param num_frames Number of frames
 * @param results Output phoneme results (must be pre-allocated)
 * @return true on success
 */
NIMCP_EXPORT bool wernicke_gpu_recognize_phonemes(
    wernicke_gpu_context_t* ctx,
    const wernicke_gpu_spectral_frame_t* frames,
    uint32_t num_frames,
    wernicke_gpu_phoneme_result_t* results
);

/**
 * @brief Compute phoneme posteriors (GPU)
 *
 * WHAT: Compute full posterior distribution over phonemes
 * WHY:  Soft phoneme decisions for downstream processing
 * HOW:  GPU softmax over phoneme similarities
 *
 * @param ctx GPU Wernicke context
 * @param frames Spectral frames
 * @param num_frames Number of frames
 * @param posteriors Output posterior matrix [num_frames x num_phonemes]
 * @return true on success
 */
NIMCP_EXPORT bool wernicke_gpu_compute_posteriors(
    wernicke_gpu_context_t* ctx,
    const wernicke_gpu_spectral_frame_t* frames,
    uint32_t num_frames,
    float* posteriors
);

/*=============================================================================
 * Lexicon Management (GPU)
 *=============================================================================*/

/**
 * @brief Upload lexicon to GPU
 *
 * @param ctx GPU Wernicke context
 * @param entries Array of lexical entries
 * @param count Number of entries
 * @return true on success
 */
NIMCP_EXPORT bool wernicke_gpu_upload_lexicon(
    wernicke_gpu_context_t* ctx,
    const wernicke_gpu_lexical_entry_t* entries,
    uint32_t count
);

/**
 * @brief Clear GPU lexicon
 *
 * @param ctx GPU Wernicke context
 * @return true on success
 */
NIMCP_EXPORT bool wernicke_gpu_clear_lexicon(wernicke_gpu_context_t* ctx);

/**
 * @brief Get lexicon size on GPU
 *
 * @param ctx GPU Wernicke context
 * @return Number of entries
 */
NIMCP_EXPORT uint32_t wernicke_gpu_get_lexicon_size(const wernicke_gpu_context_t* ctx);

/*=============================================================================
 * Parallel Word Recognition (Cohort Model)
 *=============================================================================*/

/**
 * @brief Parallel word recognition (GPU-accelerated)
 *
 * WHAT: Recognize words from phoneme sequence using cohort model
 * WHY:  Parallel evaluation of all cohort candidates
 * HOW:  GPU kernel matches phonemes against lexicon in parallel
 *
 * @param ctx GPU Wernicke context
 * @param phonemes Input phoneme sequence
 * @param num_phonemes Number of phonemes
 * @param candidates Output word candidates (pre-allocated)
 * @param max_candidates Maximum candidates to return
 * @param num_candidates Output: actual candidates found
 * @return true on success
 */
NIMCP_EXPORT bool wernicke_gpu_recognize_words(
    wernicke_gpu_context_t* ctx,
    const uint8_t* phonemes,
    uint32_t num_phonemes,
    wernicke_gpu_word_candidate_t* candidates,
    uint32_t max_candidates,
    uint32_t* num_candidates
);

/**
 * @brief Update cohort activations (GPU)
 *
 * WHAT: Update word cohort with new phoneme evidence
 * WHY:  Incremental word recognition
 * HOW:  GPU kernel updates all cohort activations in parallel
 *
 * @param ctx GPU Wernicke context
 * @param new_phoneme New phoneme ID
 * @param phoneme_confidence Confidence of new phoneme
 * @return true on success
 */
NIMCP_EXPORT bool wernicke_gpu_update_cohort(
    wernicke_gpu_context_t* ctx,
    uint8_t new_phoneme,
    float phoneme_confidence
);

/**
 * @brief Get current cohort candidates (GPU)
 *
 * @param ctx GPU Wernicke context
 * @param candidates Output candidates
 * @param max_candidates Maximum to return
 * @param num_candidates Output: actual count
 * @return true on success
 */
NIMCP_EXPORT bool wernicke_gpu_get_cohort(
    wernicke_gpu_context_t* ctx,
    wernicke_gpu_word_candidate_t* candidates,
    uint32_t max_candidates,
    uint32_t* num_candidates
);

/**
 * @brief Reset cohort (new word recognition)
 *
 * @param ctx GPU Wernicke context
 * @return true on success
 */
NIMCP_EXPORT bool wernicke_gpu_reset_cohort(wernicke_gpu_context_t* ctx);

/*=============================================================================
 * Semantic Network (GPU)
 *=============================================================================*/

/**
 * @brief Upload semantic network to GPU
 *
 * @param ctx GPU Wernicke context
 * @param concepts Array of concepts
 * @param num_concepts Number of concepts
 * @param adjacency_matrix Sparse adjacency [num_concepts x max_neighbors]
 * @param weights Edge weights matching adjacency
 * @return true on success
 */
NIMCP_EXPORT bool wernicke_gpu_upload_semantic_network(
    wernicke_gpu_context_t* ctx,
    const wernicke_gpu_concept_t* concepts,
    uint32_t num_concepts,
    const uint32_t* adjacency_matrix,
    const float* weights
);

/**
 * @brief Parallel spreading activation (GPU)
 *
 * WHAT: Spread activation through semantic network on GPU
 * WHY:  Parallel spreading across thousands of concepts
 * HOW:  GPU kernel iterates spreading with decay
 *
 * @param ctx GPU Wernicke context
 * @param seed_concepts Initial activated concepts
 * @param seed_activations Initial activation levels
 * @param num_seeds Number of seed concepts
 * @param results Output activation results
 * @param max_results Maximum results to return
 * @param num_results Output: actual results
 * @return true on success
 */
NIMCP_EXPORT bool wernicke_gpu_spread_activation(
    wernicke_gpu_context_t* ctx,
    const uint32_t* seed_concepts,
    const float* seed_activations,
    uint32_t num_seeds,
    wernicke_gpu_activation_result_t* results,
    uint32_t max_results,
    uint32_t* num_results
);

/**
 * @brief Get top-K activated concepts (GPU)
 *
 * @param ctx GPU Wernicke context
 * @param top_k Number of top concepts to return
 * @param results Output results
 * @param actual_count Output: actual count
 * @return true on success
 */
NIMCP_EXPORT bool wernicke_gpu_get_top_activated(
    wernicke_gpu_context_t* ctx,
    uint32_t top_k,
    wernicke_gpu_activation_result_t* results,
    uint32_t* actual_count
);

/**
 * @brief Compute semantic similarity (GPU)
 *
 * WHAT: Compute pairwise similarity between concepts
 * WHY:  Fast similarity computation for disambiguation
 * HOW:  GPU kernel computes cosine similarity
 *
 * @param ctx GPU Wernicke context
 * @param concept_a First concept ID
 * @param concept_b Second concept ID
 * @return Similarity score [0-1]
 */
NIMCP_EXPORT float wernicke_gpu_semantic_similarity(
    wernicke_gpu_context_t* ctx,
    uint32_t concept_a,
    uint32_t concept_b
);

/*=============================================================================
 * GPU Working Memory (Phonological Loop)
 *=============================================================================*/

/**
 * @brief Push phonemes to GPU working memory
 *
 * @param ctx GPU Wernicke context
 * @param phonemes Phonemes to push
 * @param count Number of phonemes
 * @return true on success
 */
NIMCP_EXPORT bool wernicke_gpu_wm_push(
    wernicke_gpu_context_t* ctx,
    const uint8_t* phonemes,
    uint32_t count
);

/**
 * @brief Get GPU working memory contents
 *
 * @param ctx GPU Wernicke context
 * @param phonemes Output phoneme buffer
 * @param activations Output activation buffer (optional)
 * @param max_count Maximum to retrieve
 * @param actual_count Output: actual count
 * @return true on success
 */
NIMCP_EXPORT bool wernicke_gpu_wm_get_contents(
    wernicke_gpu_context_t* ctx,
    uint8_t* phonemes,
    float* activations,
    uint32_t max_count,
    uint32_t* actual_count
);

/**
 * @brief Rehearse GPU working memory
 *
 * @param ctx GPU Wernicke context
 * @return true on success
 */
NIMCP_EXPORT bool wernicke_gpu_wm_rehearse(wernicke_gpu_context_t* ctx);

/**
 * @brief Apply decay to GPU working memory
 *
 * @param ctx GPU Wernicke context
 * @param decay_factor Decay multiplier
 * @param threshold Remove entries below this
 * @return true on success
 */
NIMCP_EXPORT bool wernicke_gpu_wm_apply_decay(
    wernicke_gpu_context_t* ctx,
    float decay_factor,
    float threshold
);

/**
 * @brief Clear GPU working memory
 *
 * @param ctx GPU Wernicke context
 * @return true on success
 */
NIMCP_EXPORT bool wernicke_gpu_wm_clear(wernicke_gpu_context_t* ctx);

/*=============================================================================
 * Full Comprehension Pipeline (GPU)
 *=============================================================================*/

/**
 * @brief Run full comprehension pipeline on GPU
 *
 * WHAT: Execute spectral->phoneme->word->semantic pipeline on GPU
 * WHY:  Maximum parallelization of language comprehension
 * HOW:  Fused GPU kernels minimize memory transfers
 *
 * @param ctx GPU Wernicke context
 * @param frames Input spectral frames
 * @param num_frames Number of frames
 * @param word_candidates Output word candidates
 * @param max_word_candidates Maximum word candidates
 * @param num_word_candidates Output: actual word candidates
 * @param semantic_activations Output semantic activations
 * @param max_semantic_activations Maximum semantic activations
 * @param num_semantic_activations Output: actual activations
 * @return true on success
 */
NIMCP_EXPORT bool wernicke_gpu_comprehend(
    wernicke_gpu_context_t* ctx,
    const wernicke_gpu_spectral_frame_t* frames,
    uint32_t num_frames,
    wernicke_gpu_word_candidate_t* word_candidates,
    uint32_t max_word_candidates,
    uint32_t* num_word_candidates,
    wernicke_gpu_activation_result_t* semantic_activations,
    uint32_t max_semantic_activations,
    uint32_t* num_semantic_activations
);

/*=============================================================================
 * Statistics and Diagnostics
 *=============================================================================*/

/**
 * @brief GPU Wernicke statistics
 */
typedef struct {
    uint64_t phoneme_recognitions;       /**< Total phoneme recognition calls */
    uint64_t word_recognitions;          /**< Total word recognition calls */
    uint64_t spreading_activations;      /**< Total spreading activation calls */
    uint64_t wm_operations;              /**< Working memory operations */
    float avg_phoneme_time_us;           /**< Average phoneme recognition time */
    float avg_word_time_us;              /**< Average word recognition time */
    float avg_spreading_time_us;         /**< Average spreading activation time */
    size_t gpu_memory_used;              /**< GPU memory in use */
    uint32_t current_cohort_size;        /**< Current word cohort size */
    uint32_t active_concepts;            /**< Currently active concepts */
} wernicke_gpu_stats_t;

/**
 * @brief Get GPU Wernicke statistics
 *
 * @param ctx GPU Wernicke context
 * @param stats Output statistics
 * @return true on success
 */
NIMCP_EXPORT bool wernicke_gpu_get_stats(
    const wernicke_gpu_context_t* ctx,
    wernicke_gpu_stats_t* stats
);

/**
 * @brief Reset GPU Wernicke statistics
 *
 * @param ctx GPU Wernicke context
 */
NIMCP_EXPORT void wernicke_gpu_reset_stats(wernicke_gpu_context_t* ctx);

/*=============================================================================
 * CPU Fallback Equivalents (for testing)
 *=============================================================================*/

/**
 * @brief CPU reference implementation of phoneme recognition
 */
NIMCP_EXPORT bool wernicke_cpu_recognize_phonemes(
    const wernicke_gpu_spectral_frame_t* frames,
    uint32_t num_frames,
    const float* phoneme_embeddings,
    uint32_t num_phonemes,
    uint32_t embed_dim,
    wernicke_gpu_phoneme_result_t* results
);

/**
 * @brief CPU reference implementation of word recognition
 */
NIMCP_EXPORT bool wernicke_cpu_recognize_words(
    const wernicke_gpu_lexical_entry_t* lexicon,
    uint32_t lexicon_size,
    const uint8_t* phonemes,
    uint32_t num_phonemes,
    wernicke_gpu_word_candidate_t* candidates,
    uint32_t max_candidates,
    uint32_t* num_candidates
);

/**
 * @brief CPU reference implementation of spreading activation
 */
NIMCP_EXPORT bool wernicke_cpu_spread_activation(
    const float* adjacency_weights,
    uint32_t num_concepts,
    uint32_t max_neighbors,
    const uint32_t* seed_concepts,
    const float* seed_activations,
    uint32_t num_seeds,
    uint32_t iterations,
    float decay,
    float* output_activations
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WERNICKE_GPU_H */
