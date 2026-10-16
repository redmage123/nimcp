/**
 * @file nimcp_semantic_compression.h
 * @brief Semantic compression for efficient neural data transmission
 *
 * WHAT: Compress neural signals by preserving semantic meaning
 * WHY:  Reduce bandwidth while maintaining information fidelity
 * HOW:  Encode meaning, not raw data -> semantic primitives -> delta coding
 *
 * BIOLOGICAL INSPIRATION:
 * Based on how the brain compresses information for efficient transmission:
 * - Sparse coding: Only encode changes and important features
 * - Semantic priming: Reuse known patterns and meanings
 * - Predictive coding: Transmit only prediction errors
 * - Vector quantization: Map continuous signals to discrete meanings
 *
 * USAGE EXAMPLE:
 * ```c
 * // Configure compressor
 * nimcp_compression_config_t config = {
 *     .max_primitives = 256,
 *     .quality_level = 0.95f,
 *     .enable_delta = true,
 *     .vector_dimension = 64
 * };
 *
 * // Create compressor
 * nimcp_semantic_compressor_t* comp = nimcp_semantic_compressor_create(&config);
 *
 * // Compress neural signal
 * float signal[1024] = {...};
 * nimcp_compressed_signal_t* compressed =
 *     nimcp_semantic_compressor_compress(comp, signal, 1024);
 *
 * // Check compression ratio
 * float ratio = nimcp_semantic_compressor_get_ratio(comp);
 * printf("Compression ratio: %.2fx\n", ratio);
 *
 * // Decompress
 * nimcp_decompressed_signal_t* decompressed =
 *     nimcp_semantic_compressor_decompress(comp, compressed);
 *
 * // Check semantic loss
 * printf("Semantic loss: %.4f\n", decompressed->semantic_loss);
 *
 * // Cleanup
 * nimcp_compressed_signal_destroy(compressed);
 * nimcp_decompressed_signal_destroy(decompressed);
 * nimcp_semantic_compressor_destroy(comp);
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#ifndef NIMCP_SEMANTIC_COMPRESSION_H
#define NIMCP_SEMANTIC_COMPRESSION_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "async/nimcp_bio_async.h"
#include "utils/validation/nimcp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct nimcp_semantic_compressor_struct nimcp_semantic_compressor_t;
typedef struct nimcp_compressed_signal_struct nimcp_compressed_signal_t;
typedef struct nimcp_decompressed_signal_struct nimcp_decompressed_signal_t;

//=============================================================================
// Constants
//=============================================================================

#define SEMANTIC_MAX_PRIMITIVES 1024
#define SEMANTIC_MAX_VECTOR_DIM 256
#define SEMANTIC_MIN_QUALITY 0.1f
#define SEMANTIC_MAX_QUALITY 1.0f

//=============================================================================
// Configuration and Data Structures
//=============================================================================

/**
 * @brief Semantic primitive - basic unit of meaning
 *
 * WHAT: Represents a fundamental pattern or concept in neural signals
 * WHY:  Allows efficient encoding by mapping to discrete meanings
 * HOW:  Vector quantization of signal patterns
 */
typedef struct {
    uint32_t primitive_id;              /**< Unique identifier */
    float* meaning_vector;              /**< Vector representation [vector_dim] */
    uint32_t vector_dim;                /**< Dimension of meaning vector */
    float confidence;                   /**< Confidence in this primitive (0-1) */
    uint32_t usage_count;               /**< How often this primitive is used */
    uint64_t last_used;                 /**< Timestamp of last usage */
} nimcp_semantic_primitive_t;

/**
 * @brief Compressed signal representation
 *
 * WHAT: Compressed neural signal using semantic primitives
 * WHY:  Compact representation for transmission
 * HOW:  Sequence of primitive IDs plus delta values
 */
struct nimcp_compressed_signal_struct {
    uint32_t* primitive_ids;            /**< Sequence of primitive IDs */
    uint32_t num_primitives;            /**< Number of primitives */

    float* deltas;                      /**< Delta values for temporal compression */
    uint32_t num_deltas;                /**< Number of delta values */

    float* residuals;                   /**< Residual errors for quality */
    uint32_t num_residuals;             /**< Number of residuals */

    size_t original_size;               /**< Original signal size in bytes */
    size_t compressed_size;             /**< Compressed size in bytes */

    uint64_t timestamp;                 /**< When compressed */
    float quality_metric;               /**< Measured quality (0-1) */
};

/**
 * @brief Decompressed signal with metadata
 *
 * WHAT: Reconstructed signal with quality metrics
 * WHY:  Track fidelity loss during compression/decompression
 * HOW:  Compare reconstructed signal to original
 */
struct nimcp_decompressed_signal_struct {
    float* signal;                      /**< Reconstructed signal */
    size_t len;                         /**< Length of signal */

    float semantic_loss;                /**< Semantic loss metric (0-1) */
    float reconstruction_error;         /**< MSE between original and reconstructed */

    uint64_t timestamp;                 /**< When decompressed */
};

/**
 * @brief Compression configuration
 *
 * WHAT: Parameters controlling compression behavior
 * WHY:  Allow tuning compression vs. quality tradeoff
 * HOW:  Configure primitive dictionary size, quality threshold, etc.
 */
typedef struct {
    uint32_t max_primitives;            /**< Maximum number of primitives */
    uint32_t vector_dimension;          /**< Dimension of meaning vectors */

    float quality_level;                /**< Target quality (0-1) */
    bool enable_delta;                  /**< Enable delta coding */
    bool enable_residuals;              /**< Store residual errors */

    float primitive_learning_rate;      /**< Rate of primitive adaptation */
    uint32_t min_primitive_usage;       /**< Min usage before pruning */

    bool bio_async_enabled;             /**< Enable bio-async integration */
    nimcp_bio_channel_type_t bio_channel; /**< Bio-async channel to use */
} nimcp_compression_config_t;

/**
 * @brief Compression statistics
 *
 * WHAT: Performance and usage statistics
 * WHY:  Monitor compression efficiency
 * HOW:  Track ratios, quality, primitive usage
 */
typedef struct {
    uint64_t total_compressions;        /**< Total compression operations */
    uint64_t total_decompressions;      /**< Total decompression operations */

    float avg_compression_ratio;        /**< Average compression ratio */
    float avg_semantic_loss;            /**< Average semantic loss */
    float avg_reconstruction_error;     /**< Average reconstruction error */

    uint32_t active_primitives;         /**< Currently active primitives */
    uint32_t total_primitives_created;  /**< Total primitives ever created */
    uint32_t primitives_pruned;         /**< Primitives pruned for low usage */

    size_t total_bytes_in;              /**< Total bytes compressed */
    size_t total_bytes_out;             /**< Total bytes after compression */
} nimcp_compression_stats_t;

//=============================================================================
// Core API Functions
//=============================================================================

/**
 * WHAT: Create semantic compressor with configuration
 * WHY:  Initialize compression system for neural signals
 * HOW:  Allocate structures, initialize primitive dictionary
 *
 * @param config Compression configuration
 * @return Compressor handle or NULL on error
 */
nimcp_semantic_compressor_t* nimcp_semantic_compressor_create(
    const nimcp_compression_config_t* config);

/**
 * WHAT: Destroy semantic compressor
 * WHY:  Clean up resources
 * HOW:  Free all allocated memory
 *
 * @param compressor Compressor to destroy
 */
void nimcp_semantic_compressor_destroy(nimcp_semantic_compressor_t* compressor);

/**
 * WHAT: Compress neural signal using semantic primitives
 * WHY:  Reduce signal size while preserving meaning
 * HOW:  Match signal patterns to primitives, apply delta coding
 *
 * @param compressor Compressor handle
 * @param signal Input signal array
 * @param len Length of signal
 * @return Compressed signal or NULL on error
 */
nimcp_compressed_signal_t* nimcp_semantic_compressor_compress(
    nimcp_semantic_compressor_t* compressor,
    const float* signal,
    size_t len);

/**
 * WHAT: Decompress compressed signal
 * WHY:  Reconstruct original signal from semantic representation
 * HOW:  Look up primitives, apply deltas, add residuals
 *
 * @param compressor Compressor handle
 * @param compressed Compressed signal
 * @return Decompressed signal or NULL on error
 */
nimcp_decompressed_signal_t* nimcp_semantic_compressor_decompress(
    nimcp_semantic_compressor_t* compressor,
    const nimcp_compressed_signal_t* compressed);

/**
 * WHAT: Get current compression ratio
 * WHY:  Monitor compression efficiency
 * HOW:  Calculate ratio of compressed to original size
 *
 * @param compressor Compressor handle
 * @return Compression ratio (>1.0 means compression)
 */
float nimcp_semantic_compressor_get_ratio(
    const nimcp_semantic_compressor_t* compressor);

/**
 * WHAT: Add new semantic primitive to dictionary
 * WHY:  Expand compression vocabulary with new patterns
 * HOW:  Store vector as new primitive, assign ID
 *
 * @param compressor Compressor handle
 * @param meaning_vector Vector representation of pattern
 * @param vector_dim Dimension of vector
 * @return Primitive ID or 0 on error
 */
uint32_t nimcp_semantic_compressor_add_primitive(
    nimcp_semantic_compressor_t* compressor,
    const float* meaning_vector,
    uint32_t vector_dim);

/**
 * WHAT: Get compression statistics
 * WHY:  Monitor performance and usage patterns
 * HOW:  Return accumulated statistics
 *
 * @param compressor Compressor handle
 * @param stats Output statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_semantic_compressor_get_stats(
    const nimcp_semantic_compressor_t* compressor,
    nimcp_compression_stats_t* stats);

/**
 * WHAT: Get default compression configuration
 * WHY:  Provide sensible defaults for most use cases
 * HOW:  Return pre-configured config structure
 *
 * @return Default configuration
 */
nimcp_compression_config_t nimcp_semantic_compressor_default_config(void);

/**
 * WHAT: Reset primitive dictionary
 * WHY:  Clear learned primitives and start fresh
 * HOW:  Free all primitives, reinitialize dictionary
 *
 * @param compressor Compressor handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_semantic_compressor_reset_primitives(
    nimcp_semantic_compressor_t* compressor);

/**
 * WHAT: Prune unused primitives
 * WHY:  Remove rarely-used primitives to save memory
 * HOW:  Remove primitives with usage below threshold
 *
 * @param compressor Compressor handle
 * @param min_usage Minimum usage count to keep
 * @return Number of primitives pruned
 */
uint32_t nimcp_semantic_compressor_prune_primitives(
    nimcp_semantic_compressor_t* compressor,
    uint32_t min_usage);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * WHAT: Destroy compressed signal
 * WHY:  Free resources
 * HOW:  Free all allocated arrays
 *
 * @param compressed Compressed signal to destroy
 */
void nimcp_compressed_signal_destroy(nimcp_compressed_signal_t* compressed);

/**
 * WHAT: Destroy decompressed signal
 * WHY:  Free resources
 * HOW:  Free signal array and structure
 *
 * @param decompressed Decompressed signal to destroy
 */
void nimcp_decompressed_signal_destroy(nimcp_decompressed_signal_t* decompressed);

/**
 * WHAT: Calculate semantic similarity between signals
 * WHY:  Measure quality of compression
 * HOW:  Compare semantic content (not raw values)
 *
 * @param signal1 First signal
 * @param len1 Length of first signal
 * @param signal2 Second signal
 * @param len2 Length of second signal
 * @return Similarity score (0-1, 1=identical)
 */
float nimcp_semantic_similarity(
    const float* signal1, size_t len1,
    const float* signal2, size_t len2);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * WHAT: Process bio-async inbox for compression module
 * WHY:  Enable distributed compression operations
 * HOW:  Check for incoming compress/decompress requests
 *
 * @param compressor Compressor handle
 * @return Number of messages processed
 */
uint32_t nimcp_semantic_compressor_process_inbox(
    nimcp_semantic_compressor_t* compressor);

/**
 * WHAT: Register compressor with bio-async system
 * WHY:  Enable async compression requests
 * HOW:  Subscribe to compression message types
 *
 * @param compressor Compressor handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_semantic_compressor_register_bio_async(
    nimcp_semantic_compressor_t* compressor);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SEMANTIC_COMPRESSION_H */
