/**
 * @file nimcp_semantic_compression.h
 * @brief Semantic compression for efficient neural data transmission
 *
 * WHAT: Compresses neural data semantically using learned primitives dictionary
 * WHY:  Enable efficient brain-to-brain synchronization with minimal bandwidth
 * HOW:  Dictionary learning, sparse coding, optional lossy compression
 *
 * BIOLOGICAL INSPIRATION:
 * - Hippocampal memory compression via sparse distributed representations
 * - Neocortical hierarchical feature extraction for dimensionality reduction
 * - Efficient coding principle: neurons represent information with minimal redundancy
 *
 * KEY CONCEPTS:
 * 1. SEMANTIC PRIMITIVES: Learned basis vectors that capture neural patterns
 * 2. SPARSE CODING: Neural data represented as sparse combinations of primitives
 * 3. LOSSY COMPRESSION: Quality-controlled approximation for higher compression
 * 4. DICTIONARY LEARNING: Online adaptation to neural data statistics
 *
 * COMPRESSION PIPELINE:
 * ┌────────────────────────────────────────────────────────────┐
 * │  Neural Data → Sparse Coding → Quantization → Encoding     │
 * │       ↓              ↓              ↓             ↓         │
 * │  Dictionary    Primitive IDs   Coefficients   Compressed   │
 * └────────────────────────────────────────────────────────────┘
 *
 * DECOMPRESSION PIPELINE:
 * ┌────────────────────────────────────────────────────────────┐
 * │  Compressed → Decoding → Dequantization → Reconstruction   │
 * │       ↓          ↓            ↓                ↓            │
 * │  Bitstream   IDs+Coefs    Float Values    Neural Data      │
 * └────────────────────────────────────────────────────────────┘
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#ifndef NIMCP_SEMANTIC_COMPRESSION_H
#define NIMCP_SEMANTIC_COMPRESSION_H

#include "utils/validation/nimcp_common.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Opaque semantic compressor handle
 */
typedef struct semantic_compressor semantic_compressor_t;

/**
 * @brief Configuration for semantic compression system
 */
typedef struct {
    /* Dictionary parameters */
    uint32_t dictionary_size;           /**< Number of semantic primitives */
    uint32_t primitive_vector_size;     /**< Dimension of each primitive */

    /* Compression parameters */
    uint32_t max_compression_ratio;     /**< Target compression ratio (e.g., 10 = 10:1) */
    uint32_t max_active_primitives;     /**< Max primitives per encoding (sparsity) */
    bool enable_lossy;                  /**< Allow lossy compression */
    float quality_threshold;            /**< Min quality for lossy (0.0-1.0) */

    /* Dictionary learning parameters */
    float learning_rate;                /**< Dictionary adaptation rate */
    uint32_t learning_iterations;       /**< Iterations per learning update */
    bool enable_online_learning;        /**< Adapt dictionary during compression */

    /* Encoding parameters */
    uint8_t quantization_bits;          /**< Bits per coefficient (0=float) */
    bool enable_entropy_coding;         /**< Use entropy coding on top */

    /* Bio-async parameters */
    bool enable_bio_async;              /**< Enable bio-async integration */
    uint32_t broadcast_interval_ms;     /**< Stats broadcast interval */
} semantic_compression_config_t;

/**
 * @brief Semantic primitive (basis vector)
 */
typedef struct {
    uint32_t primitive_id;              /**< Unique primitive ID */
    char name[64];                      /**< Human-readable name */
    float* vector;                      /**< Primitive vector data */
    uint32_t vector_size;               /**< Vector dimensionality */
    float frequency;                    /**< Usage frequency (for pruning) */
    float mean_activation;              /**< Average activation strength */
} semantic_primitive_t;

/**
 * @brief Compression statistics
 */
typedef struct {
    uint64_t total_compressions;        /**< Total compression operations */
    uint64_t total_decompressions;      /**< Total decompression operations */
    uint64_t total_bytes_in;            /**< Total input bytes */
    uint64_t total_bytes_out;           /**< Total compressed bytes */
    float avg_compression_ratio;        /**< Average compression ratio */
    float avg_reconstruction_error;     /**< Average reconstruction error */
    float avg_sparsity;                 /**< Average active primitives */
    uint64_t dictionary_updates;        /**< Number of dictionary updates */
    uint64_t bio_broadcasts_sent;       /**< Bio-async broadcasts sent */
} semantic_compression_stats_t;

/**
 * @brief Compression metadata (stored with compressed data)
 */
typedef struct {
    uint32_t original_size;             /**< Original data size in floats */
    uint32_t compressed_size;           /**< Compressed size in bytes */
    uint32_t num_active_primitives;     /**< Number of active primitives */
    float compression_ratio;            /**< Achieved compression ratio */
    float quality_score;                /**< Quality metric (0.0-1.0) */
    bool is_lossy;                      /**< Whether lossy compression used */
    uint64_t timestamp_us;              /**< Compression timestamp */
} semantic_compression_metadata_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Create semantic compressor
 *
 * WHAT: Initializes semantic compression system with dictionary
 * WHY:  Must allocate resources before compression/decompression
 * HOW:  Allocates dictionary, initializes with random or learned primitives
 *
 * @param config Configuration parameters
 * @return Compressor handle or NULL on failure
 *
 * ERRORS:
 * - NULL if config invalid or memory allocation fails
 */
semantic_compressor_t* semantic_compressor_create(
    const semantic_compression_config_t* config
);

/**
 * @brief Destroy semantic compressor
 *
 * WHAT: Frees all compressor resources
 * WHY:  Prevent memory leaks
 * HOW:  Frees dictionary, buffers, internal state
 *
 * @param comp Compressor to destroy (NULL-safe)
 */
void semantic_compressor_destroy(semantic_compressor_t* comp);

/**
 * @brief Reset compressor state
 *
 * WHAT: Clears statistics and resets dictionary to initial state
 * WHY:  Start fresh without full reinitialization
 * HOW:  Clears stats, optionally reinitializes dictionary
 *
 * @param comp Compressor handle
 * @param reset_dictionary Whether to reset dictionary or keep learned primitives
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t semantic_compressor_reset(
    semantic_compressor_t* comp,
    bool reset_dictionary
);

/* ============================================================================
 * Dictionary Management Functions
 * ============================================================================ */

/**
 * @brief Add semantic primitive to dictionary
 *
 * WHAT: Manually add a primitive to the dictionary
 * WHY:  Seed dictionary with known neural patterns
 * HOW:  Validates primitive, adds to dictionary at specified ID
 *
 * @param comp Compressor handle
 * @param prim Primitive to add (vector is copied)
 * @return NIMCP_SUCCESS or error code
 *
 * ERRORS:
 * - NIMCP_INVALID_PARAM if primitive invalid
 * - NIMCP_ALREADY_EXISTS if ID already used
 * - NIMCP_BUFFER_FULL if dictionary full
 */
nimcp_result_t semantic_add_primitive(
    semantic_compressor_t* comp,
    const semantic_primitive_t* prim
);

/**
 * @brief Get primitive from dictionary
 *
 * WHAT: Retrieve primitive by ID
 * WHY:  Inspect or export learned primitives
 * HOW:  Looks up primitive, copies to output structure
 *
 * @param comp Compressor handle
 * @param primitive_id Primitive ID to retrieve
 * @param out_prim Output primitive (vector is NOT copied, points to internal)
 * @return NIMCP_SUCCESS or NIMCP_NOT_FOUND
 */
nimcp_result_t semantic_get_primitive(
    semantic_compressor_t* comp,
    uint32_t primitive_id,
    semantic_primitive_t* out_prim
);

/**
 * @brief Learn primitives from training data
 *
 * WHAT: Adapt dictionary to represent training samples efficiently
 * WHY:  Learn optimal basis for specific neural data distribution
 * HOW:  K-means clustering or sparse coding on training samples
 *
 * @param comp Compressor handle
 * @param data Array of training sample pointers
 * @param num_samples Number of training samples
 * @param sample_size Size of each sample (must match config)
 * @return NIMCP_SUCCESS or error code
 *
 * ALGORITHM:
 * - Uses online K-SVD or similar dictionary learning
 * - Iteratively updates primitives to minimize reconstruction error
 * - Enforces sparsity constraints
 *
 * NOTE: Can be slow for large datasets, run offline or async
 */
nimcp_result_t semantic_learn_primitives(
    semantic_compressor_t* comp,
    const float** data,
    uint32_t num_samples,
    uint32_t sample_size
);

/**
 * @brief Update dictionary online during compression
 *
 * WHAT: Incrementally adapt dictionary based on recent data
 * WHY:  Track non-stationary neural distributions
 * HOW:  Mini-batch gradient descent on recent samples
 *
 * @param comp Compressor handle
 * @param neural_data Recent neural data sample
 * @param data_size Size of data in floats
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t semantic_update_dictionary(
    semantic_compressor_t* comp,
    const float* neural_data,
    uint32_t data_size
);

/* ============================================================================
 * Compression Functions
 * ============================================================================ */

/**
 * @brief Compress neural data semantically
 *
 * WHAT: Compresses neural data using learned primitives
 * WHY:  Reduce bandwidth for brain-to-brain transmission
 * HOW:  Sparse coding → quantization → entropy coding
 *
 * @param comp Compressor handle
 * @param neural_data Input neural data (float array)
 * @param data_size Number of floats in input
 * @param compressed Output buffer for compressed data
 * @param compressed_size Input: buffer size, Output: actual compressed size
 * @return NIMCP_SUCCESS or error code
 *
 * ALGORITHM:
 * 1. Find sparse representation: data ≈ Σ coef[i] * primitive[i]
 * 2. Quantize coefficients if enabled
 * 3. Entropy code IDs and coefficients
 * 4. Write metadata header
 *
 * ERRORS:
 * - NIMCP_INVALID_PARAM if data_size doesn't match config
 * - NIMCP_BUFFER_TOO_SMALL if output buffer too small
 *
 * PERFORMANCE: O(dictionary_size * data_size * max_active_primitives)
 */
nimcp_result_t semantic_compress(
    semantic_compressor_t* comp,
    const float* neural_data,
    uint32_t data_size,
    uint8_t* compressed,
    uint32_t* compressed_size
);

/**
 * @brief Compress with quality control
 *
 * WHAT: Compress while maintaining minimum quality threshold
 * WHY:  Allow quality-bandwidth tradeoff
 * HOW:  Iteratively adjusts sparsity until quality met
 *
 * @param comp Compressor handle
 * @param neural_data Input neural data
 * @param data_size Number of floats
 * @param min_quality Minimum acceptable quality (0.0-1.0)
 * @param compressed Output buffer
 * @param compressed_size Input: buffer size, Output: actual size
 * @param out_metadata Output compression metadata
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t semantic_compress_with_quality(
    semantic_compressor_t* comp,
    const float* neural_data,
    uint32_t data_size,
    float min_quality,
    uint8_t* compressed,
    uint32_t* compressed_size,
    semantic_compression_metadata_t* out_metadata
);

/**
 * @brief Decompress neural data
 *
 * WHAT: Reconstructs neural data from compressed representation
 * WHY:  Restore data at receiver for neural processing
 * HOW:  Entropy decode → dequantize → linear combination of primitives
 *
 * @param comp Compressor handle
 * @param compressed Compressed data buffer
 * @param comp_size Size of compressed data in bytes
 * @param neural_data Output buffer for decompressed data
 * @param data_size Input: buffer size, Output: actual size in floats
 * @return NIMCP_SUCCESS or error code
 *
 * ALGORITHM:
 * 1. Read metadata header
 * 2. Entropy decode primitive IDs and coefficients
 * 3. Reconstruct: data = Σ coef[i] * primitive[i]
 * 4. Apply post-processing if needed
 *
 * ERRORS:
 * - NIMCP_INVALID_MSG if compressed data corrupt
 * - NIMCP_BUFFER_TOO_SMALL if output buffer too small
 * - NIMCP_VERSION_MISMATCH if dictionary version mismatch
 */
nimcp_result_t semantic_decompress(
    semantic_compressor_t* comp,
    const uint8_t* compressed,
    uint32_t comp_size,
    float* neural_data,
    uint32_t* data_size
);

/**
 * @brief Decompress with metadata extraction
 *
 * WHAT: Decompresses and returns compression metadata
 * WHY:  Need quality/ratio info at receiver
 * HOW:  Parses metadata header during decompression
 *
 * @param comp Compressor handle
 * @param compressed Compressed data
 * @param comp_size Compressed size
 * @param neural_data Output buffer
 * @param data_size Output size
 * @param out_metadata Output metadata
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t semantic_decompress_with_metadata(
    semantic_compressor_t* comp,
    const uint8_t* compressed,
    uint32_t comp_size,
    float* neural_data,
    uint32_t* data_size,
    semantic_compression_metadata_t* out_metadata
);

/* ============================================================================
 * Quality Metrics
 * ============================================================================ */

/**
 * @brief Get current compression ratio
 *
 * WHAT: Returns ratio of input:output size
 * WHY:  Monitor compression efficiency
 * HOW:  Calculates from recent compression operations
 *
 * @param comp Compressor handle
 * @return Compression ratio (e.g., 10.0 = 10:1 compression)
 */
float semantic_get_compression_ratio(semantic_compressor_t* comp);

/**
 * @brief Get reconstruction quality metric
 *
 * WHAT: Returns normalized reconstruction quality
 * WHY:  Assess lossy compression accuracy
 * HOW:  1 - (||original - reconstructed|| / ||original||)
 *
 * @param comp Compressor handle
 * @return Quality score [0.0, 1.0] (1.0 = perfect reconstruction)
 */
float semantic_get_reconstruction_quality(semantic_compressor_t* comp);

/**
 * @brief Compute reconstruction error for sample
 *
 * WHAT: Compares original and reconstructed data
 * WHY:  Validate compression quality
 * HOW:  Compresses then decompresses, measures error
 *
 * @param comp Compressor handle
 * @param original Original neural data
 * @param data_size Size in floats
 * @param out_error Output error metrics (MSE, MAE, correlation)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t semantic_compute_reconstruction_error(
    semantic_compressor_t* comp,
    const float* original,
    uint32_t data_size,
    float* out_error
);

/**
 * @brief Get sparsity level
 *
 * WHAT: Returns average number of active primitives
 * WHY:  Monitor representation efficiency
 * HOW:  Averages active primitive count over recent compressions
 *
 * @param comp Compressor handle
 * @return Average sparsity (typically 5-20 for good compression)
 */
float semantic_get_sparsity(semantic_compressor_t* comp);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Register compressor with bio-async router
 *
 * WHAT: Integrates compressor into bio-async messaging system
 * WHY:  Enable async compression requests and broadcasts
 * HOW:  Registers message handlers for compression operations
 *
 * @param comp Compressor handle
 * @param router Bio-async router
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t semantic_compressor_register_bioasync(
    semantic_compressor_t* comp,
    bio_router_t* router
);

/**
 * @brief Broadcast compression statistics
 *
 * WHAT: Sends stats update via bio-async (dopamine channel)
 * WHY:  Notify system of compression performance
 * HOW:  Packages stats into BIO_MSG_NLP_COMPRESSION_COMPLETE
 *
 * @param comp Compressor handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t semantic_broadcast_stats(semantic_compressor_t* comp);

/**
 * @brief Handle incoming compression request
 *
 * WHAT: Bio-async handler for compression requests
 * WHY:  Support async compression operations
 * HOW:  Unpacks message, compresses, sends response
 *
 * @param comp Compressor handle
 * @param message Bio-async message
 * @param msg_size Message size
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t semantic_handle_compression_request(
    semantic_compressor_t* comp,
    const void* message,
    size_t msg_size
);

/* ============================================================================
 * Statistics and Utilities
 * ============================================================================ */

/**
 * @brief Get compression statistics
 *
 * WHAT: Retrieves accumulated statistics
 * WHY:  Monitor system performance
 * HOW:  Copies internal stats to output
 *
 * @param comp Compressor handle
 * @param out_stats Output statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t semantic_get_stats(
    semantic_compressor_t* comp,
    semantic_compression_stats_t* out_stats
);

/**
 * @brief Reset statistics counters
 *
 * WHAT: Clears accumulated statistics
 * WHY:  Start fresh monitoring period
 * HOW:  Zeros statistics structure
 *
 * @param comp Compressor handle
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t semantic_reset_stats(semantic_compressor_t* comp);

/**
 * @brief Get default configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Quick setup without manual tuning
 * HOW:  Fills config with biologically-inspired defaults
 *
 * @param out_config Output configuration structure
 */
void semantic_get_default_config(semantic_compression_config_t* out_config);

/**
 * @brief Validate configuration
 *
 * WHAT: Checks configuration parameters for validity
 * WHY:  Catch errors before initialization
 * HOW:  Validates ranges, consistency, resource limits
 *
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS if valid, error code otherwise
 */
nimcp_result_t semantic_validate_config(
    const semantic_compression_config_t* config
);

/**
 * @brief Save dictionary to file
 *
 * WHAT: Exports learned dictionary for reuse
 * WHY:  Preserve learned primitives across sessions
 * HOW:  Serializes primitives to binary file
 *
 * @param comp Compressor handle
 * @param filepath Path to save dictionary
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t semantic_save_dictionary(
    semantic_compressor_t* comp,
    const char* filepath
);

/**
 * @brief Load dictionary from file
 *
 * WHAT: Imports previously saved dictionary
 * WHY:  Reuse learned primitives
 * HOW:  Deserializes from binary file
 *
 * @param comp Compressor handle
 * @param filepath Path to dictionary file
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t semantic_load_dictionary(
    semantic_compressor_t* comp,
    const char* filepath
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SEMANTIC_COMPRESSION_H */
