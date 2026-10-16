//=============================================================================
// nimcp_positional_encoding.h - Positional Encoding System
//=============================================================================
/**
 * @file nimcp_positional_encoding.h
 * @brief Positional Encoding for Sequence and Spatial Data
 *
 * WHAT: Multiple positional encoding methods for neural sequence processing
 * WHY:  Neural networks lack inherent position awareness; explicit encoding needed
 * HOW:  Sinusoidal, learned, rotary (RoPE), and ALiBi encoding implementations
 *
 * THEORETICAL FOUNDATIONS:
 * - Vaswani et al. (2017): Attention Is All You Need - sinusoidal encoding
 * - Su et al. (2021): RoFormer - Rotary Position Embedding (RoPE)
 * - Press et al. (2022): ALiBi - Attention with Linear Biases
 * - Shaw et al. (2018): Self-Attention with Relative Position Representations
 *
 * ENCODING TYPES:
 * 1. SINUSOIDAL: Fixed sin/cos patterns (no training, extrapolates well)
 * 2. LEARNED: Trainable embeddings (task-specific, needs training)
 * 3. ROTARY (RoPE): Rotation-based relative encoding (best for long sequences)
 * 4. ALIBI: Linear attention bias (simplest, most efficient)
 * 5. RELATIVE: Relative position embeddings (Shaw et al.)
 *
 * BIO-ASYNC INTEGRATION:
 * - Encoding operations emit BIO_MSG_ENCODING_COMPUTE messages
 * - Cache updates via BIO_MSG_ENCODING_CACHE_UPDATE
 * - Uses DOPAMINE channel for computation completion signals
 *
 * SECURITY:
 * - Input validation via NIMCP_VALIDATE_PARAM macros
 * - Bounds checking on position indices
 * - Memory allocation tracking via nimcp_malloc/nimcp_free
 *
 * CODING STANDARDS:
 * - Guard clauses (no nested ifs)
 * - Helper functions (<50 lines)
 * - WHAT-WHY-HOW documentation
 * - Single Responsibility Principle
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 * @version 1.0.0
 */

#ifndef NIMCP_POSITIONAL_ENCODING_H
#define NIMCP_POSITIONAL_ENCODING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum sequence length supported */
#define NIMCP_POS_MAX_SEQ_LENGTH 32768

/** Maximum embedding dimension */
#define NIMCP_POS_MAX_DIM 4096

/** Default base for RoPE encoding (10000.0 per original paper) */
#define NIMCP_ROPE_DEFAULT_BASE 10000.0f

/** Default number of heads for ALiBi */
#define NIMCP_ALIBI_DEFAULT_HEADS 8

//=============================================================================
// Error Codes (3200-3299 range within Brain/Network Errors)
//=============================================================================

#define NIMCP_POS_SUCCESS              0
#define NIMCP_POS_ERROR_NULL_PARAM     3200
#define NIMCP_POS_ERROR_INVALID_DIM    3201
#define NIMCP_POS_ERROR_INVALID_POS    3202
#define NIMCP_POS_ERROR_ALLOC_FAILED   3203
#define NIMCP_POS_ERROR_INVALID_TYPE   3204
#define NIMCP_POS_ERROR_NOT_INIT       3205
#define NIMCP_POS_ERROR_CACHE_MISS     3206

//=============================================================================
// Encoding Types
//=============================================================================

/**
 * @brief Positional encoding method types
 *
 * WHAT: Enumeration of available encoding methods
 * WHY:  Allow runtime selection of encoding strategy
 * HOW:  Each type has different properties and use cases
 */
typedef enum {
    /** Sinusoidal encoding (Vaswani 2017) - fixed, no training */
    NIMCP_POS_SINUSOIDAL = 0,

    /** Learned position embeddings - trainable */
    NIMCP_POS_LEARNED = 1,

    /** Rotary Position Embedding (Su 2021) - relative, rotation-based */
    NIMCP_POS_ROTARY = 2,

    /** ALiBi (Press 2022) - linear attention bias */
    NIMCP_POS_ALIBI = 3,

    /** Relative position embeddings (Shaw 2018) */
    NIMCP_POS_RELATIVE = 4,

    /** Count of encoding types */
    NIMCP_POS_TYPE_COUNT = 5
} nimcp_pos_encoding_type_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Base configuration for all encoding types
 *
 * WHAT: Common parameters shared by all encoding methods
 * WHY:  Reduce duplication, consistent interface
 * HOW:  Embedded in type-specific configs
 */
typedef struct {
    uint32_t max_seq_length;     /**< Maximum sequence length to support */
    uint32_t embedding_dim;       /**< Dimension of embeddings */
    bool cache_enabled;           /**< Pre-compute and cache encodings */
    bool thread_safe;             /**< Enable thread-safe operations */
} nimcp_pos_base_config_t;

/**
 * @brief Sinusoidal encoding configuration
 *
 * WHAT: Parameters for sin/cos positional encoding
 * WHY:  Allow customization of frequency scaling
 * HOW:  PE(pos, 2i) = sin(pos / 10000^(2i/d))
 *       PE(pos, 2i+1) = cos(pos / 10000^(2i/d))
 */
typedef struct {
    nimcp_pos_base_config_t base;
    float frequency_base;         /**< Base for frequency scaling (default: 10000) */
    float frequency_scale;        /**< Additional frequency scaling factor */
} nimcp_pos_sinusoidal_config_t;

/**
 * @brief Learned embedding configuration
 *
 * WHAT: Parameters for trainable position embeddings
 * WHY:  Allow task-specific position representations
 * HOW:  Lookup table indexed by position
 */
typedef struct {
    nimcp_pos_base_config_t base;
    float init_std;               /**< Standard deviation for initialization */
    float learning_rate;          /**< Learning rate for position embeddings */
    float weight_decay;           /**< L2 regularization factor */
} nimcp_pos_learned_config_t;

/**
 * @brief RoPE (Rotary Position Embedding) configuration
 *
 * WHAT: Parameters for rotation-based relative encoding
 * WHY:  Best for long sequences, relative position aware
 * HOW:  Apply rotation matrix based on position to query/key pairs
 */
typedef struct {
    nimcp_pos_base_config_t base;
    float rope_base;              /**< Base for rotation frequencies (default: 10000) */
    float rope_scaling;           /**< NTK-aware scaling for extrapolation */
    uint32_t rope_dim;            /**< Dimension to apply RoPE (0 = all) */
    bool use_ntk_scaling;         /**< Enable NTK-aware interpolation */
    float ntk_factor;             /**< NTK scaling factor for long sequences */
} nimcp_pos_rope_config_t;

/**
 * @brief ALiBi (Attention with Linear Biases) configuration
 *
 * WHAT: Parameters for linear attention bias encoding
 * WHY:  Simplest and most efficient for long sequences
 * HOW:  Add linear bias m * (i - j) to attention scores
 */
typedef struct {
    nimcp_pos_base_config_t base;
    uint32_t num_heads;           /**< Number of attention heads */
    float slope_base;             /**< Base for geometric slope calculation */
    bool use_symmetric;           /**< Symmetric (bidirectional) bias */
} nimcp_pos_alibi_config_t;

/**
 * @brief Relative position embedding configuration
 *
 * WHAT: Parameters for relative position representations
 * WHY:  Learn relative rather than absolute positions
 * HOW:  Separate key/value embeddings indexed by relative position
 */
typedef struct {
    nimcp_pos_base_config_t base;
    uint32_t max_relative_pos;    /**< Maximum relative position to consider */
    bool use_clipping;            /**< Clip positions beyond max_relative_pos */
    float init_std;               /**< Initialization standard deviation */
} nimcp_pos_relative_config_t;

/**
 * @brief Unified configuration for any encoding type
 *
 * WHAT: Union wrapper for type-specific configs
 * WHY:  Allow generic encoding creation
 * HOW:  Tagged union pattern
 */
typedef struct {
    nimcp_pos_encoding_type_t type;
    union {
        nimcp_pos_sinusoidal_config_t sinusoidal;
        nimcp_pos_learned_config_t learned;
        nimcp_pos_rope_config_t rope;
        nimcp_pos_alibi_config_t alibi;
        nimcp_pos_relative_config_t relative;
    } config;
} nimcp_pos_config_t;

//=============================================================================
// Opaque Handle Types
//=============================================================================

/** Opaque handle for positional encoding instance */
typedef struct nimcp_pos_encoder_s nimcp_pos_encoder_t;

/** Opaque handle for encoding cache */
typedef struct nimcp_pos_cache_s nimcp_pos_cache_t;

//=============================================================================
// Statistics Structure
//=============================================================================

/**
 * @brief Statistics for positional encoding operations
 *
 * WHAT: Performance and usage metrics
 * WHY:  Enable monitoring and optimization
 * HOW:  Track counts, timings, cache performance
 */
typedef struct {
    uint64_t total_encodings;          /**< Total encoding operations */
    uint64_t cache_hits;               /**< Cache hit count */
    uint64_t cache_misses;             /**< Cache miss count */
    float avg_encoding_time_us;        /**< Average encoding time (microseconds) */
    float peak_encoding_time_us;       /**< Peak encoding time */
    size_t memory_usage_bytes;         /**< Current memory usage */
    size_t cache_size_bytes;           /**< Cache memory usage */
} nimcp_pos_stats_t;

//=============================================================================
// Default Configuration Functions
//=============================================================================

/**
 * @brief Get default sinusoidal encoding configuration
 *
 * WHAT: Return sensible defaults for sinusoidal encoding
 * WHY:  Good starting point matching Transformer paper
 * HOW:  max_seq=8192, dim=512, base=10000
 *
 * @return Default sinusoidal configuration
 */
nimcp_pos_sinusoidal_config_t nimcp_pos_sinusoidal_default_config(void);

/**
 * @brief Get default learned embedding configuration
 *
 * WHAT: Return sensible defaults for learned embeddings
 * WHY:  Good starting point for most tasks
 * HOW:  max_seq=2048, dim=512, init_std=0.02
 *
 * @return Default learned configuration
 */
nimcp_pos_learned_config_t nimcp_pos_learned_default_config(void);

/**
 * @brief Get default RoPE configuration
 *
 * WHAT: Return sensible defaults for rotary encoding
 * WHY:  Good starting point matching RoFormer paper
 * HOW:  max_seq=8192, dim=512, base=10000
 *
 * @return Default RoPE configuration
 */
nimcp_pos_rope_config_t nimcp_pos_rope_default_config(void);

/**
 * @brief Get default ALiBi configuration
 *
 * WHAT: Return sensible defaults for ALiBi
 * WHY:  Good starting point for efficient attention
 * HOW:  max_seq=16384, num_heads=8
 *
 * @return Default ALiBi configuration
 */
nimcp_pos_alibi_config_t nimcp_pos_alibi_default_config(void);

/**
 * @brief Get default relative position configuration
 *
 * WHAT: Return sensible defaults for relative embeddings
 * WHY:  Good starting point for local attention
 * HOW:  max_seq=4096, max_relative=128
 *
 * @return Default relative configuration
 */
nimcp_pos_relative_config_t nimcp_pos_relative_default_config(void);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create positional encoder instance
 *
 * WHAT: Initialize encoder with specified configuration
 * WHY:  Factory function for any encoding type
 * HOW:  Allocate structures, pre-compute caches if enabled
 *
 * @param config Unified configuration specifying encoding type
 * @return Encoder handle, or NULL on failure
 */
nimcp_pos_encoder_t* nimcp_pos_encoder_create(const nimcp_pos_config_t* config);

/**
 * @brief Destroy positional encoder instance
 *
 * WHAT: Free all resources associated with encoder
 * WHY:  Prevent memory leaks
 * HOW:  Free caches, embeddings, and encoder structure
 *
 * @param encoder Encoder to destroy (NULL-safe)
 */
void nimcp_pos_encoder_destroy(nimcp_pos_encoder_t* encoder);

//=============================================================================
// Encoding Functions
//=============================================================================

/**
 * @brief Encode single position
 *
 * WHAT: Get positional encoding vector for one position
 * WHY:  Basic building block for sequence encoding
 * HOW:  Compute or lookup encoding based on type
 *
 * @param encoder Encoder instance
 * @param position Position index (0-based)
 * @param output Output buffer (must be embedding_dim floats)
 * @return NIMCP_POS_SUCCESS or error code
 */
int nimcp_pos_encode_position(
    nimcp_pos_encoder_t* encoder,
    uint32_t position,
    float* output
);

/**
 * @brief Encode sequence of positions
 *
 * WHAT: Get positional encodings for entire sequence
 * WHY:  Efficient batch encoding
 * HOW:  Batch computation or cache lookup
 *
 * @param encoder Encoder instance
 * @param start_pos Starting position
 * @param seq_length Number of positions to encode
 * @param output Output buffer (seq_length * embedding_dim floats)
 * @return NIMCP_POS_SUCCESS or error code
 */
int nimcp_pos_encode_sequence(
    nimcp_pos_encoder_t* encoder,
    uint32_t start_pos,
    uint32_t seq_length,
    float* output
);

/**
 * @brief Apply positional encoding to input tensor
 *
 * WHAT: Add or concatenate position encodings to input
 * WHY:  Common operation for transformer input processing
 * HOW:  input[i] = input[i] + PE(i) (additive mode)
 *
 * @param encoder Encoder instance
 * @param input Input tensor (seq_length * embedding_dim)
 * @param seq_length Sequence length
 * @param output Output tensor (can be same as input for in-place)
 * @param additive If true, add to input; if false, concatenate
 * @return NIMCP_POS_SUCCESS or error code
 */
int nimcp_pos_apply_encoding(
    nimcp_pos_encoder_t* encoder,
    const float* input,
    uint32_t seq_length,
    float* output,
    bool additive
);

//=============================================================================
// RoPE-Specific Functions
//=============================================================================

/**
 * @brief Apply RoPE rotation to query/key pair
 *
 * WHAT: Rotate query and key vectors by position-dependent angles
 * WHY:  Core operation for rotary position embedding
 * HOW:  Apply 2D rotation matrices along embedding dimension
 *
 * @param encoder Encoder instance (must be NIMCP_POS_ROTARY type)
 * @param query Query vector (embedding_dim floats)
 * @param key Key vector (embedding_dim floats)
 * @param position Position index
 * @param query_out Rotated query output
 * @param key_out Rotated key output
 * @return NIMCP_POS_SUCCESS or error code
 */
int nimcp_pos_rope_apply(
    nimcp_pos_encoder_t* encoder,
    const float* query,
    const float* key,
    uint32_t position,
    float* query_out,
    float* key_out
);

/**
 * @brief Apply RoPE to batched queries/keys
 *
 * WHAT: Efficient batch RoPE for attention computation
 * WHY:  Batch processing for transformer forward pass
 * HOW:  Apply rotations to all positions in parallel
 *
 * @param encoder Encoder instance
 * @param queries Query tensor (seq_len * num_heads * head_dim)
 * @param keys Key tensor (seq_len * num_heads * head_dim)
 * @param seq_length Sequence length
 * @param num_heads Number of attention heads
 * @param queries_out Output queries
 * @param keys_out Output keys
 * @return NIMCP_POS_SUCCESS or error code
 */
int nimcp_pos_rope_apply_batch(
    nimcp_pos_encoder_t* encoder,
    const float* queries,
    const float* keys,
    uint32_t seq_length,
    uint32_t num_heads,
    float* queries_out,
    float* keys_out
);

//=============================================================================
// ALiBi-Specific Functions
//=============================================================================

/**
 * @brief Get ALiBi attention bias matrix
 *
 * WHAT: Compute linear bias matrix for attention scores
 * WHY:  Core operation for ALiBi-based attention
 * HOW:  bias[h][i][j] = -slope[h] * abs(i - j)
 *
 * @param encoder Encoder instance (must be NIMCP_POS_ALIBI type)
 * @param seq_length Sequence length
 * @param bias_out Output bias matrix (num_heads * seq_length * seq_length)
 * @return NIMCP_POS_SUCCESS or error code
 */
int nimcp_pos_alibi_get_bias(
    nimcp_pos_encoder_t* encoder,
    uint32_t seq_length,
    float* bias_out
);

/**
 * @brief Get ALiBi slopes for all heads
 *
 * WHAT: Get geometric slope values for each attention head
 * WHY:  Different heads use different distance decay rates
 * HOW:  slope[h] = 2^(-8 * (h+1) / num_heads)
 *
 * @param encoder Encoder instance
 * @param slopes_out Output slopes (num_heads floats)
 * @return NIMCP_POS_SUCCESS or error code
 */
int nimcp_pos_alibi_get_slopes(
    nimcp_pos_encoder_t* encoder,
    float* slopes_out
);

//=============================================================================
// Learned Embedding Functions
//=============================================================================

/**
 * @brief Get gradient for learned position embeddings
 *
 * WHAT: Compute gradients for backpropagation
 * WHY:  Enable training of position embeddings
 * HOW:  Gradient from upstream passed to embedding lookup
 *
 * @param encoder Encoder instance (must be NIMCP_POS_LEARNED type)
 * @param grad_output Gradient from downstream (seq_len * dim)
 * @param positions Position indices used in forward pass
 * @param seq_length Number of positions
 * @param grad_embeddings Output gradients for embeddings
 * @return NIMCP_POS_SUCCESS or error code
 */
int nimcp_pos_learned_backward(
    nimcp_pos_encoder_t* encoder,
    const float* grad_output,
    const uint32_t* positions,
    uint32_t seq_length,
    float* grad_embeddings
);

/**
 * @brief Update learned embeddings with gradients
 *
 * WHAT: Apply gradient descent update to embeddings
 * WHY:  Train position embeddings
 * HOW:  embed -= learning_rate * gradient
 *
 * @param encoder Encoder instance
 * @param gradients Gradients for all embeddings
 * @param learning_rate Learning rate (0 = use config default)
 * @return NIMCP_POS_SUCCESS or error code
 */
int nimcp_pos_learned_update(
    nimcp_pos_encoder_t* encoder,
    const float* gradients,
    float learning_rate
);

//=============================================================================
// Cache Management
//=============================================================================

/**
 * @brief Pre-compute encodings up to specified length
 *
 * WHAT: Fill cache with pre-computed encodings
 * WHY:  Avoid recomputation during inference
 * HOW:  Compute and store encodings for positions [0, length)
 *
 * @param encoder Encoder instance
 * @param length Number of positions to cache
 * @return NIMCP_POS_SUCCESS or error code
 */
int nimcp_pos_cache_precompute(
    nimcp_pos_encoder_t* encoder,
    uint32_t length
);

/**
 * @brief Clear encoding cache
 *
 * WHAT: Free cached encodings
 * WHY:  Reclaim memory or prepare for different sequence
 * HOW:  Free cache data, reset counters
 *
 * @param encoder Encoder instance
 * @return NIMCP_POS_SUCCESS or error code
 */
int nimcp_pos_cache_clear(nimcp_pos_encoder_t* encoder);

/**
 * @brief Get cache statistics
 *
 * WHAT: Query cache hit/miss rates
 * WHY:  Monitor cache effectiveness
 * HOW:  Return cached metrics
 *
 * @param encoder Encoder instance
 * @param hit_rate Output hit rate (0-1)
 * @param size_bytes Output cache size
 * @return NIMCP_POS_SUCCESS or error code
 */
int nimcp_pos_cache_stats(
    nimcp_pos_encoder_t* encoder,
    float* hit_rate,
    size_t* size_bytes
);

//=============================================================================
// Statistics and Diagnostics
//=============================================================================

/**
 * @brief Get encoder statistics
 *
 * WHAT: Query performance metrics
 * WHY:  Enable monitoring and optimization
 * HOW:  Return accumulated statistics
 *
 * @param encoder Encoder instance
 * @param stats Output statistics structure
 * @return NIMCP_POS_SUCCESS or error code
 */
int nimcp_pos_get_stats(
    nimcp_pos_encoder_t* encoder,
    nimcp_pos_stats_t* stats
);

/**
 * @brief Reset encoder statistics
 *
 * WHAT: Clear accumulated statistics
 * WHY:  Fresh measurement period
 * HOW:  Zero all counters
 *
 * @param encoder Encoder instance
 * @return NIMCP_POS_SUCCESS or error code
 */
int nimcp_pos_reset_stats(nimcp_pos_encoder_t* encoder);

/**
 * @brief Get encoder type
 *
 * WHAT: Query encoding type of instance
 * WHY:  Runtime type checking
 * HOW:  Return stored type enum
 *
 * @param encoder Encoder instance
 * @return Encoding type, or -1 if invalid encoder
 */
nimcp_pos_encoding_type_t nimcp_pos_get_type(nimcp_pos_encoder_t* encoder);

/**
 * @brief Get embedding dimension
 *
 * WHAT: Query dimension of position embeddings
 * WHY:  Need for buffer allocation
 * HOW:  Return stored dimension
 *
 * @param encoder Encoder instance
 * @return Embedding dimension, or 0 if invalid encoder
 */
uint32_t nimcp_pos_get_dim(nimcp_pos_encoder_t* encoder);

/**
 * @brief Get maximum sequence length
 *
 * WHAT: Query maximum supported sequence length
 * WHY:  Bounds checking
 * HOW:  Return stored max length
 *
 * @param encoder Encoder instance
 * @return Maximum sequence length, or 0 if invalid encoder
 */
uint32_t nimcp_pos_get_max_length(nimcp_pos_encoder_t* encoder);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Convert encoding type to string
 *
 * WHAT: Get human-readable name for encoding type
 * WHY:  Logging and diagnostics
 * HOW:  Switch on enum value
 *
 * @param type Encoding type
 * @return String name (static, do not free)
 */
const char* nimcp_pos_type_to_string(nimcp_pos_encoding_type_t type);

/**
 * @brief Validate configuration
 *
 * WHAT: Check configuration for errors
 * WHY:  Early detection of invalid parameters
 * HOW:  Validate ranges and constraints
 *
 * @param config Configuration to validate
 * @return NIMCP_POS_SUCCESS if valid, error code otherwise
 */
int nimcp_pos_validate_config(const nimcp_pos_config_t* config);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_POSITIONAL_ENCODING_H
