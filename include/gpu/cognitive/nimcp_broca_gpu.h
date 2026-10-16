/**
 * @file nimcp_broca_gpu.h
 * @brief GPU-Accelerated Broca's Region API
 *
 * WHAT: C API for GPU-accelerated Broca's region operations
 * WHY:  GPU acceleration for language production hot paths
 * HOW:  Wraps CUDA kernels for parallel lexical, phonological, and motor processing
 *
 * BIOLOGICAL BASIS:
 * =================
 * Broca's region (BA44/45) processes language production through:
 * - Lexical access: Parallel word retrieval from mental lexicon
 * - Phonological encoding: Word-to-phoneme conversion
 * - Motor planning: Articulatory command generation
 *
 * GPU ACCELERATION RATIONALE:
 * ===========================
 * Language production involves parallel processing of:
 * - Multiple lexical candidates during word selection
 * - Phoneme sequences for coarticulation planning
 * - Motor command batches for articulator control
 *
 * USAGE:
 * ======
 * @code
 * nimcp_gpu_context_t* ctx = nimcp_gpu_context_create(0);
 * broca_gpu_context_t* broca_gpu = broca_gpu_create(ctx, &config);
 *
 * // Batch lexical lookup
 * broca_gpu_batch_lexical_lookup(broca_gpu, word_ids, num_words, results);
 *
 * // Parallel phoneme encoding
 * broca_gpu_encode_phonemes(broca_gpu, words, num_words, phoneme_buffer, &phoneme_count);
 *
 * // GPU motor command generation
 * broca_gpu_generate_motor_commands(broca_gpu, phonemes, num_phonemes, commands, &cmd_count);
 *
 * broca_gpu_destroy(broca_gpu);
 * nimcp_gpu_context_destroy(ctx);
 * @endcode
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_BROCA_GPU_H
#define NIMCP_BROCA_GPU_H

// Include GPU context BEFORE extern "C" block - it brings in CUDA headers
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// Forward Declarations
//=============================================================================

/** @brief Opaque GPU Broca context */
typedef struct broca_gpu_context broca_gpu_context_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief GPU Broca configuration
 */
typedef struct {
    uint32_t max_lexicon_size;       /**< Maximum lexicon entries on GPU */
    uint32_t max_batch_size;         /**< Maximum batch size for operations */
    uint32_t max_phonemes_per_word;  /**< Max phonemes per word (default: 16) */
    uint32_t max_articulators;       /**< Number of articulators (default: 6) */
    uint32_t working_memory_slots;   /**< GPU working memory slots */
    bool enable_coarticulation;      /**< Enable GPU coarticulation */
    bool enable_async_transfer;      /**< Enable async CPU-GPU transfers */
    float activation_decay_rate;     /**< WM activation decay per ms */
} broca_gpu_config_t;

/**
 * @brief Get default GPU Broca configuration
 */
NIMCP_EXPORT broca_gpu_config_t broca_gpu_default_config(void);

//=============================================================================
// Lexical Entry Structures (GPU-compatible)
//=============================================================================

/**
 * @brief GPU-optimized lexical entry (padded for coalesced access)
 */
typedef struct {
    uint32_t word_id;                /**< Unique word identifier */
    uint8_t phonemes[16];            /**< Phoneme sequence (fixed size for GPU) */
    uint32_t phoneme_count;          /**< Number of phonemes */
    uint8_t pos;                     /**< Part of speech */
    float frequency;                 /**< Usage frequency (0-1) */
    float activation;                /**< Current activation level */
    uint32_t _padding;               /**< Padding for 64-byte alignment */
} broca_gpu_lexical_entry_t;

/**
 * @brief GPU motor command (optimized layout)
 */
typedef struct {
    uint8_t articulator;             /**< Target articulator ID */
    uint8_t phoneme;                 /**< Associated phoneme */
    uint16_t _padding;               /**< Padding */
    float position;                  /**< Target position [0, 1] */
    float velocity;                  /**< Movement velocity */
    float timestamp_ms;              /**< Execution timestamp */
} broca_gpu_motor_command_t;

/**
 * @brief Batch lookup result
 */
typedef struct {
    uint32_t word_id;
    bool found;
    uint32_t phoneme_count;
    uint8_t phonemes[16];
    float frequency;
} broca_gpu_lookup_result_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create GPU Broca context
 *
 * WHAT: Initialize GPU resources for Broca acceleration
 * WHY:  Allocate GPU memory for lexicon, working memory, buffers
 * HOW:  Create CUDA streams, allocate device memory
 *
 * @param gpu_ctx GPU context (required)
 * @param config Configuration (NULL for defaults)
 * @return New GPU Broca context, or NULL on failure
 */
NIMCP_EXPORT broca_gpu_context_t* broca_gpu_create(
    nimcp_gpu_context_t* gpu_ctx,
    const broca_gpu_config_t* config
);

/**
 * @brief Destroy GPU Broca context
 *
 * @param ctx GPU Broca context to destroy
 */
NIMCP_EXPORT void broca_gpu_destroy(broca_gpu_context_t* ctx);

/**
 * @brief Synchronize GPU operations
 *
 * @param ctx GPU Broca context
 * @return true on success
 */
NIMCP_EXPORT bool broca_gpu_synchronize(broca_gpu_context_t* ctx);

//=============================================================================
// Lexicon Management (GPU)
//=============================================================================

/**
 * @brief Upload lexicon entries to GPU
 *
 * WHAT: Transfer lexical entries from CPU to GPU memory
 * WHY:  Enable GPU-accelerated parallel lexical lookup
 * HOW:  Batch upload with optional async transfer
 *
 * @param ctx GPU Broca context
 * @param entries Array of lexical entries
 * @param count Number of entries
 * @return true on success
 */
NIMCP_EXPORT bool broca_gpu_upload_lexicon(
    broca_gpu_context_t* ctx,
    const broca_gpu_lexical_entry_t* entries,
    uint32_t count
);

/**
 * @brief Clear GPU lexicon
 *
 * @param ctx GPU Broca context
 * @return true on success
 */
NIMCP_EXPORT bool broca_gpu_clear_lexicon(broca_gpu_context_t* ctx);

/**
 * @brief Get lexicon size on GPU
 *
 * @param ctx GPU Broca context
 * @return Number of entries in GPU lexicon
 */
NIMCP_EXPORT uint32_t broca_gpu_get_lexicon_size(const broca_gpu_context_t* ctx);

//=============================================================================
// Parallel Lexical Lookup
//=============================================================================

/**
 * @brief Batch lexical lookup (GPU-accelerated)
 *
 * WHAT: Look up multiple words in parallel on GPU
 * WHY:  Parallel lexical access for utterance planning
 * HOW:  GPU kernel searches lexicon for each word_id simultaneously
 *
 * @param ctx GPU Broca context
 * @param word_ids Array of word IDs to look up
 * @param count Number of words
 * @param results Output array of results (must be pre-allocated)
 * @return true on success
 */
NIMCP_EXPORT bool broca_gpu_batch_lexical_lookup(
    broca_gpu_context_t* ctx,
    const uint32_t* word_ids,
    uint32_t count,
    broca_gpu_lookup_result_t* results
);

/**
 * @brief Parallel lexical search by activation (GPU)
 *
 * WHAT: Find top-N words by activation level
 * WHY:  Competitive lexical selection in production
 * HOW:  GPU parallel reduction to find highest activations
 *
 * @param ctx GPU Broca context
 * @param top_n Number of top entries to return
 * @param results Output array (must hold top_n entries)
 * @param actual_count Output: actual number returned
 * @return true on success
 */
NIMCP_EXPORT bool broca_gpu_find_top_activated(
    broca_gpu_context_t* ctx,
    uint32_t top_n,
    broca_gpu_lexical_entry_t* results,
    uint32_t* actual_count
);

/**
 * @brief Update lexical activations (GPU)
 *
 * WHAT: Apply activation boost/decay to lexicon entries
 * WHY:  Priming and recency effects in word selection
 * HOW:  GPU kernel updates all activations in parallel
 *
 * @param ctx GPU Broca context
 * @param word_ids Words to boost (NULL to decay all)
 * @param count Number of words to boost
 * @param boost_amount Activation boost for specified words
 * @param decay_rate Decay multiplier for all words (e.g., 0.95)
 * @return true on success
 */
NIMCP_EXPORT bool broca_gpu_update_activations(
    broca_gpu_context_t* ctx,
    const uint32_t* word_ids,
    uint32_t count,
    float boost_amount,
    float decay_rate
);

//=============================================================================
// Parallel Phonological Encoding
//=============================================================================

/**
 * @brief Encode words to phonemes (GPU-accelerated)
 *
 * WHAT: Convert batch of words to phoneme sequences
 * WHY:  Parallel phonological encoding for utterance
 * HOW:  GPU kernel looks up and concatenates phonemes
 *
 * @param ctx GPU Broca context
 * @param word_ids Array of word IDs to encode
 * @param word_count Number of words
 * @param phoneme_buffer Output buffer for phonemes
 * @param buffer_size Size of phoneme buffer
 * @param phoneme_count Output: total phonemes generated
 * @param word_boundaries Output: boundary indices (optional, can be NULL)
 * @return true on success
 */
NIMCP_EXPORT bool broca_gpu_encode_phonemes(
    broca_gpu_context_t* ctx,
    const uint32_t* word_ids,
    uint32_t word_count,
    uint8_t* phoneme_buffer,
    uint32_t buffer_size,
    uint32_t* phoneme_count,
    uint32_t* word_boundaries
);

/**
 * @brief Apply coarticulation effects (GPU)
 *
 * WHAT: Modify phoneme features for coarticulation
 * WHY:  Natural speech requires phoneme blending
 * HOW:  GPU kernel applies neighbor-dependent adjustments
 *
 * @param ctx GPU Broca context
 * @param phonemes Input/output phoneme array
 * @param phoneme_count Number of phonemes
 * @param coarticulation_strength Blending factor [0-1]
 * @return true on success
 */
NIMCP_EXPORT bool broca_gpu_apply_coarticulation(
    broca_gpu_context_t* ctx,
    uint8_t* phonemes,
    uint32_t phoneme_count,
    float coarticulation_strength
);

//=============================================================================
// Parallel Motor Command Generation
//=============================================================================

/**
 * @brief Generate motor commands (GPU-accelerated)
 *
 * WHAT: Convert phonemes to articulator commands in parallel
 * WHY:  Fast motor planning for speech production
 * HOW:  GPU kernel computes commands for all phonemes simultaneously
 *
 * @param ctx GPU Broca context
 * @param phonemes Input phoneme sequence
 * @param phoneme_count Number of phonemes
 * @param commands Output motor command array
 * @param max_commands Maximum commands (buffer size)
 * @param command_count Output: actual commands generated
 * @param base_timestamp Starting timestamp (ms)
 * @return true on success
 */
NIMCP_EXPORT bool broca_gpu_generate_motor_commands(
    broca_gpu_context_t* ctx,
    const uint8_t* phonemes,
    uint32_t phoneme_count,
    broca_gpu_motor_command_t* commands,
    uint32_t max_commands,
    uint32_t* command_count,
    float base_timestamp
);

/**
 * @brief Batch motor command timing adjustment (GPU)
 *
 * WHAT: Adjust motor command timing for speech rate
 * WHY:  Support variable speaking rates
 * HOW:  GPU kernel scales all timestamps
 *
 * @param ctx GPU Broca context
 * @param commands Motor command array (modified in-place)
 * @param command_count Number of commands
 * @param rate_multiplier Timing scale factor (1.0 = normal)
 * @return true on success
 */
NIMCP_EXPORT bool broca_gpu_adjust_timing(
    broca_gpu_context_t* ctx,
    broca_gpu_motor_command_t* commands,
    uint32_t command_count,
    float rate_multiplier
);

//=============================================================================
// GPU Working Memory
//=============================================================================

/**
 * @brief Push words to GPU working memory
 *
 * WHAT: Add words to GPU-side working memory buffer
 * WHY:  Fast rehearsal and manipulation on GPU
 * HOW:  Circular buffer with activation decay
 *
 * @param ctx GPU Broca context
 * @param word_ids Words to push
 * @param count Number of words
 * @param initial_activation Initial activation level
 * @return true on success
 */
NIMCP_EXPORT bool broca_gpu_wm_push(
    broca_gpu_context_t* ctx,
    const uint32_t* word_ids,
    uint32_t count,
    float initial_activation
);

/**
 * @brief Get GPU working memory contents
 *
 * @param ctx GPU Broca context
 * @param word_ids Output word ID array
 * @param activations Output activation array (optional)
 * @param max_count Maximum entries to retrieve
 * @param actual_count Output: actual entries
 * @return true on success
 */
NIMCP_EXPORT bool broca_gpu_wm_get_contents(
    broca_gpu_context_t* ctx,
    uint32_t* word_ids,
    float* activations,
    uint32_t max_count,
    uint32_t* actual_count
);

/**
 * @brief Apply decay to GPU working memory
 *
 * WHAT: Decay activations in GPU working memory
 * WHY:  Model temporal decay of verbal working memory
 * HOW:  GPU kernel multiplies all activations by decay factor
 *
 * @param ctx GPU Broca context
 * @param decay_factor Decay multiplier (e.g., 0.95)
 * @param threshold Remove entries below this activation
 * @return true on success
 */
NIMCP_EXPORT bool broca_gpu_wm_apply_decay(
    broca_gpu_context_t* ctx,
    float decay_factor,
    float threshold
);

/**
 * @brief Clear GPU working memory
 *
 * @param ctx GPU Broca context
 * @return true on success
 */
NIMCP_EXPORT bool broca_gpu_wm_clear(broca_gpu_context_t* ctx);

//=============================================================================
// Full Pipeline (GPU)
//=============================================================================

/**
 * @brief Run full production pipeline on GPU
 *
 * WHAT: Execute complete word->phoneme->motor pipeline on GPU
 * WHY:  Maximum parallelization of language production
 * HOW:  Fused GPU kernels minimize memory transfers
 *
 * @param ctx GPU Broca context
 * @param word_ids Input word sequence
 * @param word_count Number of words
 * @param commands Output motor command array
 * @param max_commands Maximum commands (buffer size)
 * @param command_count Output: actual commands generated
 * @param base_timestamp Starting timestamp (ms)
 * @return true on success
 */
NIMCP_EXPORT bool broca_gpu_produce_utterance(
    broca_gpu_context_t* ctx,
    const uint32_t* word_ids,
    uint32_t word_count,
    broca_gpu_motor_command_t* commands,
    uint32_t max_commands,
    uint32_t* command_count,
    float base_timestamp
);

//=============================================================================
// Statistics and Diagnostics
//=============================================================================

/**
 * @brief GPU Broca statistics
 */
typedef struct {
    uint64_t lexical_lookups;        /**< Total lookup calls */
    uint64_t phonemes_encoded;       /**< Total phonemes processed */
    uint64_t motor_commands;         /**< Total commands generated */
    uint64_t wm_operations;          /**< Working memory operations */
    float avg_lookup_time_us;        /**< Average lookup time */
    float avg_encode_time_us;        /**< Average encode time */
    float avg_motor_time_us;         /**< Average motor gen time */
    size_t gpu_memory_used;          /**< GPU memory in use */
} broca_gpu_stats_t;

/**
 * @brief Get GPU Broca statistics
 *
 * @param ctx GPU Broca context
 * @param stats Output statistics structure
 * @return true on success
 */
NIMCP_EXPORT bool broca_gpu_get_stats(
    const broca_gpu_context_t* ctx,
    broca_gpu_stats_t* stats
);

/**
 * @brief Reset GPU Broca statistics
 *
 * @param ctx GPU Broca context
 */
NIMCP_EXPORT void broca_gpu_reset_stats(broca_gpu_context_t* ctx);

//=============================================================================
// CPU Fallback Equivalents (for testing)
//=============================================================================

/**
 * @brief CPU reference implementation of batch lexical lookup
 *
 * WHAT: CPU equivalent for GPU/CPU equivalence testing
 * WHY:  Verify GPU results match CPU
 */
NIMCP_EXPORT bool broca_cpu_batch_lexical_lookup(
    const broca_gpu_lexical_entry_t* lexicon,
    uint32_t lexicon_size,
    const uint32_t* word_ids,
    uint32_t count,
    broca_gpu_lookup_result_t* results
);

/**
 * @brief CPU reference implementation of phoneme encoding
 */
NIMCP_EXPORT bool broca_cpu_encode_phonemes(
    const broca_gpu_lexical_entry_t* lexicon,
    uint32_t lexicon_size,
    const uint32_t* word_ids,
    uint32_t word_count,
    uint8_t* phoneme_buffer,
    uint32_t buffer_size,
    uint32_t* phoneme_count,
    uint32_t* word_boundaries
);

/**
 * @brief CPU reference implementation of motor command generation
 */
NIMCP_EXPORT bool broca_cpu_generate_motor_commands(
    const uint8_t* phonemes,
    uint32_t phoneme_count,
    broca_gpu_motor_command_t* commands,
    uint32_t max_commands,
    uint32_t* command_count,
    float base_timestamp,
    uint32_t num_articulators
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BROCA_GPU_H */
