//=============================================================================
// nimcp_zscore_normalizer.h - Z-Score Normalization
//=============================================================================

#ifndef NIMCP_ZSCORE_NORMALIZER_H
#define NIMCP_ZSCORE_NORMALIZER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_zscore_normalizer.h
 * @brief Statistical z-score normalization for neural signals
 *
 * WHAT: Standardize signals to zero mean and unit variance
 * WHY:  Remove scale differences, enable stable learning
 * HOW:  Running mean/variance via Welford's algorithm
 */

typedef struct zscore_normalizer zscore_normalizer_t;

// Z-score statistics
typedef struct {
    float mean;           /**< Current mean */
    float variance;       /**< Current variance */
    float stddev;         /**< Standard deviation */
    size_t sample_count;  /**< Number of samples seen */
    float min_value;      /**< Minimum value observed */
    float max_value;      /**< Maximum value observed */
} zscore_stats_t;

//=============================================================================
// LIFECYCLE
//=============================================================================

/**
 * @brief Create z-score normalizer
 *
 * WHAT: Allocate normalizer with running statistics
 * WHY:  Enable online normalization without storing all samples
 * HOW:  Initialize Welford's algorithm state
 *
 * @param num_channels Number of parallel channels
 * @param window_size Window for statistics (0 = infinite)
 * @param outlier_clip Number of std devs for clipping (0 = no clipping)
 * @return Normalizer or NULL on failure
 */
zscore_normalizer_t* zscore_normalizer_create(
    size_t num_channels,
    size_t window_size,
    float outlier_clip
);

/**
 * @brief Destroy normalizer
 * @param normalizer Normalizer to destroy (can be NULL)
 */
void zscore_normalizer_destroy(zscore_normalizer_t* normalizer);

//=============================================================================
// NORMALIZATION
//=============================================================================

/**
 * @brief Fit normalizer to new sample
 *
 * WHAT: Update statistics with new observation
 * WHY:  Adapt to changing signal statistics
 * HOW:  Welford's online variance algorithm
 *
 * @param normalizer Normalizer to update
 * @param channel Channel index
 * @param value New sample value
 * @return true on success
 */
bool zscore_normalizer_fit(
    zscore_normalizer_t* normalizer,
    size_t channel,
    float value
);

/**
 * @brief Transform value to z-score
 *
 * WHAT: Normalize value: z = (x - mean) / stddev
 * WHY:  Convert to standard normal distribution
 * HOW:  Apply current statistics
 *
 * @param normalizer Normalizer to use
 * @param channel Channel index
 * @param value Value to normalize
 * @return Normalized z-score
 */
float zscore_normalizer_transform(
    const zscore_normalizer_t* normalizer,
    size_t channel,
    float value
);

/**
 * @brief Fit and transform in one operation
 *
 * WHAT: Update statistics then normalize
 * WHY:  Efficient online normalization
 * HOW:  Combine fit + transform
 *
 * @param normalizer Normalizer to use
 * @param channel Channel index
 * @param value Value to fit and transform
 * @return Normalized z-score
 */
float zscore_normalizer_fit_transform(
    zscore_normalizer_t* normalizer,
    size_t channel,
    float value
);

/**
 * @brief Inverse transform z-score to original scale
 *
 * WHAT: Convert z-score back: x = z * stddev + mean
 * WHY:  Reconstruct original values
 * HOW:  Apply inverse transformation
 *
 * @param normalizer Normalizer to use
 * @param channel Channel index
 * @param zscore Z-score to inverse transform
 * @return Value in original scale
 */
float zscore_normalizer_inverse_transform(
    const zscore_normalizer_t* normalizer,
    size_t channel,
    float zscore
);

//=============================================================================
// BATCH OPERATIONS
//=============================================================================

/**
 * @brief Fit to batch of samples
 * @return Number of samples fitted
 */
size_t zscore_normalizer_fit_batch(
    zscore_normalizer_t* normalizer,
    size_t channel,
    const float* values,
    size_t count
);

/**
 * @brief Transform batch of values
 * @return Number of values transformed
 */
size_t zscore_normalizer_transform_batch(
    const zscore_normalizer_t* normalizer,
    size_t channel,
    const float* values,
    float* outputs,
    size_t count
);

//=============================================================================
// QUERY OPERATIONS
//=============================================================================

/**
 * @brief Get channel statistics
 * @return true on success
 */
bool zscore_normalizer_get_stats(
    const zscore_normalizer_t* normalizer,
    size_t channel,
    zscore_stats_t* stats
);

/**
 * @brief Get current mean
 * @return Mean value, or 0.0 if invalid
 */
float zscore_normalizer_mean(
    const zscore_normalizer_t* normalizer,
    size_t channel
);

/**
 * @brief Get current standard deviation
 * @return Standard deviation, or 1.0 if invalid
 */
float zscore_normalizer_stddev(
    const zscore_normalizer_t* normalizer,
    size_t channel
);

/**
 * @brief Get number of channels
 * @return Number of channels
 */
size_t zscore_normalizer_num_channels(const zscore_normalizer_t* normalizer);

//=============================================================================
// MANAGEMENT
//=============================================================================

/**
 * @brief Reset channel statistics
 * @param normalizer Normalizer to reset
 * @param channel Channel index
 * @return true on success
 */
bool zscore_normalizer_reset_channel(
    zscore_normalizer_t* normalizer,
    size_t channel
);

/**
 * @brief Reset all channels
 * @param normalizer Normalizer to reset
 */
void zscore_normalizer_reset_all(zscore_normalizer_t* normalizer);

//=============================================================================
// TENSOR-BASED OPERATIONS
//=============================================================================

/* Forward declaration for tensor type */
#ifndef NIMCP_TENSOR_T_DEFINED
#define NIMCP_TENSOR_T_DEFINED
struct nimcp_tensor_s;
typedef struct nimcp_tensor_s nimcp_tensor_t;
#endif

/**
 * @brief Transform tensor in-place using z-score normalization
 *
 * More efficient than scalar operations for large tensors.
 *
 * @param normalizer Z-score normalizer instance
 * @param channel Channel index for statistics
 * @param tensor Tensor to normalize (modified in place)
 * @return true on success
 */
bool zscore_normalizer_transform_tensor(
    const zscore_normalizer_t* normalizer,
    size_t channel,
    nimcp_tensor_t* tensor
);

/**
 * @brief Inverse transform tensor in-place
 *
 * @param normalizer Z-score normalizer instance
 * @param channel Channel index for statistics
 * @param tensor Tensor to denormalize (modified in place)
 * @return true on success
 */
bool zscore_normalizer_inverse_transform_tensor(
    const zscore_normalizer_t* normalizer,
    size_t channel,
    nimcp_tensor_t* tensor
);

/**
 * @brief Fit normalizer using tensor data
 *
 * Efficiently computes mean/variance from tensor using vectorized operations.
 *
 * @param normalizer Z-score normalizer instance
 * @param channel Channel index to update
 * @param tensor Tensor containing values to fit
 * @return Number of elements processed
 */
size_t zscore_normalizer_fit_tensor(
    zscore_normalizer_t* normalizer,
    size_t channel,
    const nimcp_tensor_t* tensor
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_ZSCORE_NORMALIZER_H
