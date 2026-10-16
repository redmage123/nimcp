/**
 * @file nimcp_jepa_latent.h
 * @brief JEPA Latent Space Representation Module
 * @version 1.0.0
 * @date 2025-12-26
 *
 * WHAT: Dense embedding representation for Joint Embedding Predictive Architecture
 * WHY:  JEPA predicts in latent/embedding space rather than pixel/token space,
 *       enabling more efficient learning focused on task-relevant semantics.
 * HOW:  Provides normalized embeddings with uncertainty estimates that integrate
 *       with NIMCP's FEP precision-weighted prediction error framework.
 *
 * THEORETICAL FOUNDATION:
 * =======================
 *
 * JEPA CORE PRINCIPLE (LeCun, 2022):
 * ----------------------------------
 * Instead of predicting raw observations (pixels, tokens), JEPA predicts
 * abstract representations in a learned embedding space:
 *
 *   Traditional: Encoder → Decoder → Reconstruct Input
 *   JEPA:        Encoder → Predictor → Predict Embeddings
 *
 * This abstraction allows the model to focus on task-relevant features
 * while ignoring surface-level variability.
 *
 * PRECISION INTEGRATION:
 * ----------------------
 * Each latent dimension carries an uncertainty estimate (variance), which
 * maps directly to FEP precision weights:
 *
 *   π_i = 1 / σ²_i   (precision = inverse variance)
 *
 * Prediction errors are precision-weighted:
 *
 *   ε_weighted = π × (z_predicted - z_target)
 *
 * BIOLOGICAL BASIS:
 * -----------------
 * Latent embeddings model neural population codes:
 * - Distributed representations across many neurons
 * - Activity patterns encode semantic content
 * - Precision maps to synaptic gain / attention
 *
 * REFERENCES:
 * - LeCun, Y. (2022) "A Path Towards Autonomous Machine Intelligence"
 * - Chen et al. (2025) "VL-JEPA: Joint Embedding Predictive Architecture"
 * - Bardes et al. (2024) "V-JEPA 2: Self-Supervised Video Models"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_JEPA_LATENT_H
#define NIMCP_JEPA_LATENT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/platform/nimcp_tier_optimization.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Bio-async module ID for JEPA latent system */
#define BIO_MODULE_JEPA_LATENT                  0x0E00

/** @brief Maximum latent dimensionality */
#define JEPA_LATENT_MAX_DIM                     1024

/** @brief Minimum latent dimensionality */
#define JEPA_LATENT_MIN_DIM                     16

/** @brief Default latent dimensionality (tier-dependent) */
#ifndef NIMCP_JEPA_LATENT_DIM
    #define NIMCP_JEPA_LATENT_DIM               256
#endif

/** @brief Default precision for new latents */
#define JEPA_LATENT_DEFAULT_PRECISION           1.0f

/** @brief Minimum precision to prevent numerical instability */
#define JEPA_LATENT_MIN_PRECISION               0.001f

/** @brief Maximum precision to prevent over-confidence */
#define JEPA_LATENT_MAX_PRECISION               1000.0f

/** @brief Similarity threshold for "same" embeddings */
#define JEPA_LATENT_SIMILARITY_THRESHOLD        0.95f

/** @brief Small epsilon for numerical stability */
#define JEPA_LATENT_EPSILON                     1e-8f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Modality of the latent representation
 *
 * WHAT: Identifies the source modality of the embedding
 * WHY:  Multimodal JEPA needs to track embedding origins
 */
typedef enum {
    JEPA_MODALITY_UNKNOWN = 0,      /**< Unknown or unspecified modality */
    JEPA_MODALITY_VISUAL,           /**< Visual (image/video) embedding */
    JEPA_MODALITY_SPEECH,           /**< Speech/audio embedding */
    JEPA_MODALITY_TEXT,             /**< Text/language embedding */
    JEPA_MODALITY_MOTOR,            /**< Motor/action embedding */
    JEPA_MODALITY_MULTIMODAL,       /**< Fused multimodal embedding */
    JEPA_MODALITY_COUNT             /**< Number of modalities */
} jepa_modality_t;

/**
 * @brief Normalization type for embeddings
 */
typedef enum {
    JEPA_NORM_NONE = 0,             /**< No normalization */
    JEPA_NORM_L2,                   /**< L2 (unit sphere) normalization */
    JEPA_NORM_LAYERNORM,            /**< Layer normalization (zero mean, unit var) */
    JEPA_NORM_BATCHNORM             /**< Batch normalization (requires stats) */
} jepa_norm_type_t;

/**
 * @brief Similarity metric for comparing latents
 */
typedef enum {
    JEPA_SIM_COSINE = 0,            /**< Cosine similarity (default) */
    JEPA_SIM_DOT_PRODUCT,           /**< Dot product similarity */
    JEPA_SIM_EUCLIDEAN,             /**< Negative Euclidean distance */
    JEPA_SIM_PRECISION_WEIGHTED     /**< Precision-weighted similarity */
} jepa_similarity_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief JEPA latent representation
 *
 * WHAT: Dense embedding vector in joint latent space
 * WHY:  Core representation for JEPA prediction and comparison
 * HOW:  Stores embedding with per-dimension uncertainty
 *
 * Memory Layout:
 *   embedding[latent_dim] - The actual embedding values
 *   variance[latent_dim]  - Per-dimension uncertainty (optional)
 */
typedef struct jepa_latent {
    /* Embedding data */
    float* embedding;               /**< Latent vector [latent_dim] */
    uint32_t latent_dim;            /**< Embedding dimensionality */

    /* Uncertainty tracking (optional) */
    float* variance;                /**< Per-dimension variance σ² [latent_dim], NULL if disabled */
    float precision;                /**< Overall precision π = 1/mean(σ²) */

    /* Metadata */
    jepa_modality_t modality;       /**< Source modality */
    uint64_t timestamp_ms;          /**< When this latent was computed */
    uint32_t sequence_position;     /**< Position in sequence (for temporal) */

    /* Normalization state */
    bool is_normalized;             /**< Whether embedding is normalized */
    jepa_norm_type_t norm_type;     /**< Type of normalization applied */

    /* Reference counting for memory management */
    uint32_t ref_count;             /**< Reference count for CoW semantics */
} jepa_latent_t;

/**
 * @brief Configuration for JEPA latent creation
 */
typedef struct {
    uint32_t latent_dim;            /**< Embedding dimensionality */
    bool enable_variance;           /**< Track per-dimension variance */
    jepa_norm_type_t norm_type;     /**< Default normalization */
    float initial_precision;        /**< Initial precision value */
    jepa_modality_t modality;       /**< Source modality */
} jepa_latent_config_t;

/**
 * @brief Statistics for latent operations
 */
typedef struct {
    uint64_t latents_created;       /**< Total latents created */
    uint64_t latents_destroyed;     /**< Total latents destroyed */
    uint64_t normalizations;        /**< Normalization operations */
    uint64_t similarity_ops;        /**< Similarity computations */
    uint64_t interpolations;        /**< Interpolation operations */
    float avg_latent_norm;          /**< Average L2 norm of embeddings */
    float avg_precision;            /**< Average precision */
} jepa_latent_stats_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default latent configuration
 *
 * WHAT: Provide sensible defaults for latent creation
 * WHY:  Easy initialization with tier-appropriate parameters
 *
 * @param config Output configuration
 * @return NIMCP_SUCCESS on success, error code on failure
 */
int jepa_latent_default_config(jepa_latent_config_t* config);

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Create a new JEPA latent representation
 *
 * WHAT: Allocate and initialize a latent embedding
 * WHY:  Core building block for JEPA operations
 * HOW:  Allocate memory, initialize to zeros, set metadata
 *
 * @param config Configuration (NULL for defaults)
 * @return New latent or NULL on failure
 *
 * @note Caller must call jepa_latent_destroy() when done
 */
jepa_latent_t* jepa_latent_create(const jepa_latent_config_t* config);

/**
 * @brief Create a latent with specific dimensionality
 *
 * WHAT: Convenience function for common case
 * WHY:  Most uses just need to specify dimension
 *
 * @param latent_dim Embedding dimensionality
 * @return New latent or NULL on failure
 */
jepa_latent_t* jepa_latent_create_dim(uint32_t latent_dim);

/**
 * @brief Clone a latent representation
 *
 * WHAT: Create deep copy of a latent
 * WHY:  Need independent copies for manipulation
 *
 * @param src Source latent
 * @return Cloned latent or NULL on failure
 */
jepa_latent_t* jepa_latent_clone(const jepa_latent_t* src);

/**
 * @brief Destroy a latent representation
 *
 * WHAT: Free all memory associated with latent
 * WHY:  Proper cleanup
 *
 * @param latent Latent to destroy (NULL safe)
 */
void jepa_latent_destroy(jepa_latent_t* latent);

/**
 * @brief Reset latent to zero state
 *
 * WHAT: Clear embedding values while preserving allocation
 * WHY:  Reuse memory without reallocation
 *
 * @param latent Latent to reset
 * @return NIMCP_SUCCESS on success
 */
int jepa_latent_reset(jepa_latent_t* latent);

/* ============================================================================
 * Data Access API
 * ============================================================================ */

/**
 * @brief Set embedding values from array
 *
 * @param latent Target latent
 * @param values Source array [latent_dim]
 * @param dim Number of values
 * @return NIMCP_SUCCESS on success
 */
int jepa_latent_set_embedding(jepa_latent_t* latent, const float* values, uint32_t dim);

/**
 * @brief Get embedding values to array
 *
 * @param latent Source latent
 * @param values Output array [latent_dim]
 * @param max_dim Maximum values to copy
 * @return Number of values copied, -1 on error
 */
int jepa_latent_get_embedding(const jepa_latent_t* latent, float* values, uint32_t max_dim);

/**
 * @brief Set variance values
 *
 * @param latent Target latent
 * @param variance Variance array [latent_dim]
 * @param dim Number of values
 * @return NIMCP_SUCCESS on success
 */
int jepa_latent_set_variance(jepa_latent_t* latent, const float* variance, uint32_t dim);

/**
 * @brief Update precision from variance
 *
 * WHAT: Compute overall precision from per-dimension variances
 * WHY:  Precision is inverse of mean variance
 * HOW:  π = 1 / mean(σ²)
 *
 * @param latent Latent with variance set
 * @return NIMCP_SUCCESS on success
 */
int jepa_latent_update_precision(jepa_latent_t* latent);

/* ============================================================================
 * Normalization API
 * ============================================================================ */

/**
 * @brief Normalize embedding to unit length (L2)
 *
 * WHAT: Scale embedding so ||z|| = 1
 * WHY:  Cosine similarity requires unit vectors
 * HOW:  z = z / ||z||
 *
 * @param latent Latent to normalize
 * @return NIMCP_SUCCESS on success
 */
int jepa_latent_normalize(jepa_latent_t* latent);

/**
 * @brief Apply layer normalization
 *
 * WHAT: Normalize to zero mean, unit variance
 * WHY:  Stabilizes training, reduces covariate shift
 * HOW:  z = (z - mean(z)) / std(z)
 *
 * @param latent Latent to normalize
 * @return NIMCP_SUCCESS on success
 */
int jepa_latent_layer_normalize(jepa_latent_t* latent);

/**
 * @brief Get L2 norm of embedding
 *
 * @param latent Source latent
 * @return L2 norm, -1.0f on error
 */
float jepa_latent_norm(const jepa_latent_t* latent);

/* ============================================================================
 * Similarity API
 * ============================================================================ */

/**
 * @brief Compute cosine similarity between two latents
 *
 * WHAT: cos(θ) = (a · b) / (||a|| × ||b||)
 * WHY:  Standard similarity metric for embeddings
 *
 * @param a First latent
 * @param b Second latent
 * @return Cosine similarity [-1, 1], NAN on error
 */
float jepa_latent_cosine_similarity(const jepa_latent_t* a, const jepa_latent_t* b);

/**
 * @brief Compute similarity using specified metric
 *
 * @param a First latent
 * @param b Second latent
 * @param metric Similarity metric to use
 * @return Similarity value, NAN on error
 */
float jepa_latent_similarity(const jepa_latent_t* a, const jepa_latent_t* b,
                              jepa_similarity_t metric);

/**
 * @brief Compute precision-weighted similarity
 *
 * WHAT: Similarity weighted by mutual precision
 * WHY:  Uncertain dimensions should contribute less
 * HOW:  Σ(π_i × a_i × b_i) / Σ(π_i)
 *
 * @param a First latent (with variance)
 * @param b Second latent (with variance)
 * @return Precision-weighted similarity
 */
float jepa_latent_precision_similarity(const jepa_latent_t* a, const jepa_latent_t* b);

/**
 * @brief Compute Euclidean distance between latents
 *
 * @param a First latent
 * @param b Second latent
 * @return Euclidean distance, -1.0f on error
 */
float jepa_latent_distance(const jepa_latent_t* a, const jepa_latent_t* b);

/* ============================================================================
 * Interpolation API
 * ============================================================================ */

/**
 * @brief Linear interpolation between two latents
 *
 * WHAT: result = (1-α) × a + α × b
 * WHY:  Smooth transitions in latent space
 *
 * @param a First latent
 * @param b Second latent
 * @param alpha Interpolation factor [0, 1]
 * @param result Output latent (must be pre-allocated)
 * @return NIMCP_SUCCESS on success
 */
int jepa_latent_interpolate(const jepa_latent_t* a, const jepa_latent_t* b,
                             float alpha, jepa_latent_t* result);

/**
 * @brief Spherical linear interpolation (SLERP)
 *
 * WHAT: Interpolate along great circle on unit sphere
 * WHY:  Better for normalized embeddings
 *
 * @param a First latent (normalized)
 * @param b Second latent (normalized)
 * @param alpha Interpolation factor [0, 1]
 * @param result Output latent
 * @return NIMCP_SUCCESS on success
 */
int jepa_latent_slerp(const jepa_latent_t* a, const jepa_latent_t* b,
                       float alpha, jepa_latent_t* result);

/* ============================================================================
 * Arithmetic API
 * ============================================================================ */

/**
 * @brief Add two latents element-wise
 *
 * @param a First latent
 * @param b Second latent
 * @param result Output latent (can be same as a or b)
 * @return NIMCP_SUCCESS on success
 */
int jepa_latent_add(const jepa_latent_t* a, const jepa_latent_t* b,
                     jepa_latent_t* result);

/**
 * @brief Subtract two latents element-wise
 *
 * WHAT: result = a - b
 * WHY:  Compute difference/residual in latent space
 *
 * @param a First latent
 * @param b Second latent
 * @param result Output latent
 * @return NIMCP_SUCCESS on success
 */
int jepa_latent_subtract(const jepa_latent_t* a, const jepa_latent_t* b,
                          jepa_latent_t* result);

/**
 * @brief Scale latent by scalar
 *
 * @param latent Latent to scale (in-place)
 * @param scale Scale factor
 * @return NIMCP_SUCCESS on success
 */
int jepa_latent_scale(jepa_latent_t* latent, float scale);

/**
 * @brief Compute dot product of two latents
 *
 * @param a First latent
 * @param b Second latent
 * @return Dot product, NAN on error
 */
float jepa_latent_dot(const jepa_latent_t* a, const jepa_latent_t* b);

/* ============================================================================
 * Projection API
 * ============================================================================ */

/**
 * @brief Project latent to different dimensionality
 *
 * WHAT: Linear projection to new dimension
 * WHY:  Align different encoder outputs to same space
 *
 * @param src Source latent
 * @param projection Projection matrix [target_dim × src_dim]
 * @param bias Bias vector [target_dim], can be NULL
 * @param target_dim Target dimensionality
 * @param result Output latent
 * @return NIMCP_SUCCESS on success
 */
int jepa_latent_project(const jepa_latent_t* src,
                         const float* projection,
                         const float* bias,
                         uint32_t target_dim,
                         jepa_latent_t* result);

/* ============================================================================
 * Pooling API
 * ============================================================================ */

/**
 * @brief Average pool multiple latents into one
 *
 * WHAT: result = mean(latents)
 * WHY:  Aggregate multiple local embeddings into global
 *
 * @param latents Array of latent pointers
 * @param num_latents Number of latents
 * @param result Output pooled latent
 * @return NIMCP_SUCCESS on success
 */
int jepa_latent_mean_pool(const jepa_latent_t** latents, uint32_t num_latents,
                           jepa_latent_t* result);

/**
 * @brief Max pool multiple latents
 *
 * WHAT: result[i] = max(latent[j][i] for all j)
 * WHY:  Alternative pooling preserving strongest activations
 *
 * @param latents Array of latent pointers
 * @param num_latents Number of latents
 * @param result Output pooled latent
 * @return NIMCP_SUCCESS on success
 */
int jepa_latent_max_pool(const jepa_latent_t** latents, uint32_t num_latents,
                          jepa_latent_t* result);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get global latent statistics
 *
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
int jepa_latent_get_stats(jepa_latent_stats_t* stats);

/**
 * @brief Reset global statistics
 *
 * @return NIMCP_SUCCESS on success
 */
int jepa_latent_reset_stats(void);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert modality to string
 *
 * @param modality Modality enum
 * @return Human-readable string
 */
const char* jepa_modality_to_string(jepa_modality_t modality);

/**
 * @brief Convert normalization type to string
 *
 * @param norm_type Normalization type enum
 * @return Human-readable string
 */
const char* jepa_norm_type_to_string(jepa_norm_type_t norm_type);

/**
 * @brief Convert similarity metric to string
 *
 * @param metric Similarity metric enum
 * @return Human-readable string
 */
const char* jepa_similarity_to_string(jepa_similarity_t metric);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_JEPA_LATENT_H */
