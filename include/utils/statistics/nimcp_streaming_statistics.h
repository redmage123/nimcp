/**
 * @file nimcp_streaming_statistics.h
 * @brief Streaming/Online Statistics Algorithms for NIMCP
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Memory-efficient streaming algorithms for statistical computation on
 *       unbounded data streams with O(1) space complexity
 *
 * WHY:  Neural computation generates continuous data streams that cannot be
 *       stored in memory. These algorithms enable real-time statistical
 *       monitoring, anomaly detection, and adaptive learning.
 *
 * HOW:  Implements numerically stable online algorithms (Welford, P-squared,
 *       t-digest, HyperLogLog) with GPU acceleration for batch updates
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Online Learning in Neural Systems:
 *   +------------------------------------------------------------------------+
 *   |  Biological neural systems continuously adapt to streaming input:     |
 *   |                                                                        |
 *   |  Synaptic Plasticity:                                                  |
 *   |    STDP computes timing statistics online                              |
 *   |    Running averages for homeostatic regulation                         |
 *   |                                                                        |
 *   |  Population Coding:                                                    |
 *   |    Real-time firing rate estimation                                    |
 *   |    Online covariance for noise correlations                            |
 *   |                                                                        |
 *   |  Predictive Coding:                                                    |
 *   |    Streaming prediction error computation                              |
 *   |    Online variance estimation for precision weighting                  |
 *   +------------------------------------------------------------------------+
 *
 * ALGORITHMS IMPLEMENTED:
 *
 * 1. Welford's Algorithm - Online mean/variance (O(1) space, O(n) time)
 * 2. P-squared Algorithm - Streaming quantile estimation
 * 3. t-digest - Accurate quantile estimation with merging
 * 4. Reservoir Sampling - Uniform random sample from stream
 * 5. Count-Min Sketch - Frequency estimation (sublinear space)
 * 6. HyperLogLog - Cardinality estimation (sublinear space)
 * 7. EWMA/EWMV - Exponentially weighted moving statistics
 * 8. Incremental PCA - Online principal component analysis
 * 9. Online Linear Regression - Streaming least squares
 *
 * FEATURES:
 * - O(1) memory for most operations (streaming constraint)
 * - GPU acceleration for batch updates
 * - Thread-safe accumulators with atomic operations
 * - Mergeable structures for parallel processing
 * - Full exception handling with NIMCP_THROW_TO_IMMUNE
 * - Logging integration
 *
 * PERFORMANCE:
 * - Single update: ~10ns (CPU), ~1ns amortized (GPU batch)
 * - Batch update (N=1000): ~2us (CPU), ~50us (GPU including transfer)
 * - Merge operation: O(1) for most structures
 *
 * THREAD SAFETY:
 * - All accumulators support atomic updates via nimcp_atomic
 * - Lock-free batch updates where possible
 * - Explicit locking for complex merges
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_STREAMING_STATISTICS_H
#define NIMCP_STREAMING_STATISTICS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <float.h>

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Magic number for structure validation */
#define NIMCP_STREAM_STATS_MAGIC         0x53545253  /* 'STRS' */
#define NIMCP_STREAM_QUANTILE_MAGIC      0x51554E54  /* 'QUNT' */
#define NIMCP_STREAM_COV_MAGIC           0x434F5653  /* 'COVS' */
#define NIMCP_STREAM_PCA_MAGIC           0x50434153  /* 'PCAS' */
#define NIMCP_STREAM_LINREG_MAGIC        0x4C494E52  /* 'LINR' */
#define NIMCP_RESERVOIR_MAGIC            0x52455356  /* 'RESV' */
#define NIMCP_CMS_MAGIC                  0x434D534B  /* 'CMSK' */
#define NIMCP_HLL_MAGIC                  0x484C4C43  /* 'HLLC' */
#define NIMCP_EWMA_MAGIC                 0x45574D41  /* 'EWMA' */

/** Default t-digest compression parameter */
#define NIMCP_TDIGEST_DEFAULT_COMPRESSION 100

/** Default HyperLogLog precision (2^p registers) */
#define NIMCP_HLL_DEFAULT_PRECISION 14

/** Default Count-Min Sketch parameters */
#define NIMCP_CMS_DEFAULT_WIDTH 2048
#define NIMCP_CMS_DEFAULT_DEPTH 5

/** Maximum PCA components for streaming */
#define NIMCP_STREAM_PCA_MAX_COMPONENTS 256

/** Default reservoir size */
#define NIMCP_RESERVOIR_DEFAULT_SIZE 1000

//=============================================================================
// Error Codes
//=============================================================================

/**
 * @brief Result codes for streaming statistics operations
 */
typedef enum nimcp_stream_stats_result {
    NIMCP_STREAM_OK = 0,                    /**< Success */
    NIMCP_STREAM_ERROR_NULL = -1,           /**< NULL pointer argument */
    NIMCP_STREAM_ERROR_INVALID = -2,        /**< Invalid structure (bad magic) */
    NIMCP_STREAM_ERROR_MEMORY = -3,         /**< Memory allocation failed */
    NIMCP_STREAM_ERROR_PARAMS = -4,         /**< Invalid parameters */
    NIMCP_STREAM_ERROR_EMPTY = -5,          /**< No data in accumulator */
    NIMCP_STREAM_ERROR_MISMATCH = -6,       /**< Dimension mismatch in merge */
    NIMCP_STREAM_ERROR_OVERFLOW = -7,       /**< Numeric overflow */
    NIMCP_STREAM_ERROR_GPU = -8,            /**< GPU operation failed */
    NIMCP_STREAM_ERROR_NOT_INIT = -9        /**< Module not initialized */
} nimcp_stream_stats_result_t;

//=============================================================================
// Forward Declarations
//=============================================================================

/* Opaque structure pointers for public API */
typedef struct nimcp_stream_stats_s*       nimcp_stream_stats_t;
typedef struct nimcp_stream_quantile_s*    nimcp_stream_quantile_t;
typedef struct nimcp_stream_cov_s*         nimcp_stream_cov_t;
typedef struct nimcp_stream_cov_matrix_s*  nimcp_stream_cov_matrix_t;
typedef struct nimcp_stream_pca_s*         nimcp_stream_pca_t;
typedef struct nimcp_stream_linreg_s*      nimcp_stream_linreg_t;
typedef struct nimcp_reservoir_s*          nimcp_reservoir_t;
typedef struct nimcp_cms_s*                nimcp_cms_t;
typedef struct nimcp_hll_s*                nimcp_hll_t;
typedef struct nimcp_ewma_s*               nimcp_ewma_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Configuration for streaming statistics module
 */
typedef struct nimcp_stream_stats_config {
    bool enable_gpu;                    /**< Enable GPU acceleration */
    uint32_t gpu_batch_threshold;       /**< Minimum batch size for GPU */
    bool enable_thread_safety;          /**< Enable atomic operations */
    uint32_t tdigest_compression;       /**< t-digest compression parameter */
    uint32_t hll_precision;             /**< HyperLogLog precision (p) */
    uint32_t cms_width;                 /**< Count-Min Sketch width */
    uint32_t cms_depth;                 /**< Count-Min Sketch depth */
    uint32_t reservoir_size;            /**< Default reservoir size */
    uint32_t random_seed;               /**< Seed for random sampling (0=random) */
} nimcp_stream_stats_config_t;

/**
 * @brief Configuration for quantile estimator
 */
typedef struct nimcp_stream_quantile_config {
    enum {
        NIMCP_QUANTILE_PSQUARED,         /**< P-squared algorithm */
        NIMCP_QUANTILE_TDIGEST           /**< t-digest algorithm */
    } algorithm;
    uint32_t compression;                /**< Compression for t-digest */
    float target_quantile;               /**< Target quantile (0.5 for median) */
} nimcp_stream_quantile_config_t;

/**
 * @brief Configuration for streaming PCA
 */
typedef struct nimcp_stream_pca_config {
    uint32_t n_components;               /**< Number of principal components */
    uint32_t n_features;                 /**< Number of input features */
    float forgetting_factor;             /**< Forgetting factor (0-1, 1=no forget) */
    bool whiten;                         /**< Whiten output */
} nimcp_stream_pca_config_t;

//=============================================================================
// Module Initialization
//=============================================================================

/**
 * @brief Get default configuration for streaming statistics
 * @return Default configuration
 */
NIMCP_EXPORT nimcp_stream_stats_config_t nimcp_stream_stats_default_config(void);

/**
 * @brief Initialize the streaming statistics module
 * @param config Configuration (NULL for defaults)
 * @return NIMCP_STREAM_OK on success
 *
 * WHAT: Initialize streaming statistics subsystem
 * WHY:  Sets up GPU context, RNG, atomic operations
 * HOW:  One-time setup, thread-safe initialization
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_stats_init(
    const nimcp_stream_stats_config_t* config
);

/**
 * @brief Shutdown the streaming statistics module
 *
 * WHAT: Clean up streaming statistics subsystem
 * WHY:  Release GPU resources, cached allocations
 * HOW:  Free internal state, sync GPU
 */
NIMCP_EXPORT void nimcp_stream_stats_shutdown(void);

/**
 * @brief Check if streaming statistics module is initialized
 * @return true if initialized
 */
NIMCP_EXPORT bool nimcp_stream_stats_is_initialized(void);

//=============================================================================
// Online Descriptive Statistics (Welford's Algorithm)
//=============================================================================
/** @defgroup stream_stats Online Descriptive Statistics
 * @brief Numerically stable streaming mean, variance, min, max
 *
 * Uses Welford's online algorithm for numerically stable computation
 * of mean and variance in a single pass with O(1) space.
 *
 * Chan et al. (1983) parallel algorithm enables efficient merging of
 * accumulators computed on different data partitions.
 *
 * @{
 */

/**
 * @brief Create a new streaming statistics accumulator
 * @return New accumulator handle, NULL on failure
 *
 * WHAT: Allocate and initialize streaming stats accumulator
 * WHY:  Track running statistics on unbounded data stream
 * HOW:  Welford's algorithm for numerical stability
 *
 * Memory: ~96 bytes per accumulator
 * Thread-safe: Yes (atomic updates)
 */
NIMCP_EXPORT nimcp_stream_stats_t nimcp_stream_stats_create(void);

/**
 * @brief Destroy a streaming statistics accumulator
 * @param stats Accumulator to destroy (NULL safe)
 */
NIMCP_EXPORT void nimcp_stream_stats_destroy(nimcp_stream_stats_t stats);

/**
 * @brief Add a single value to the accumulator
 * @param stats Accumulator handle
 * @param value Value to add
 * @return NIMCP_STREAM_OK on success
 *
 * Time: O(1) constant time update
 * Thread-safe: Yes (uses atomic compare-and-swap)
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_stats_update(
    nimcp_stream_stats_t stats,
    double value
);

/**
 * @brief Add a batch of values to the accumulator
 * @param stats Accumulator handle
 * @param values Array of values
 * @param count Number of values
 * @return NIMCP_STREAM_OK on success
 *
 * More efficient than individual updates due to amortized locking.
 * Uses GPU acceleration if enabled and count > threshold.
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_stats_update_batch(
    nimcp_stream_stats_t stats,
    const float* values,
    uint32_t count
);

/**
 * @brief Merge two accumulators (parallel-friendly)
 * @param dest Destination accumulator (receives merged result)
 * @param src Source accumulator (unchanged)
 * @return NIMCP_STREAM_OK on success
 *
 * Enables parallel computation: partition data, compute partial stats,
 * then merge results with exact correctness.
 *
 * Uses Chan et al. parallel combination formula.
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_stats_merge(
    nimcp_stream_stats_t dest,
    const nimcp_stream_stats_t src
);

/**
 * @brief Get current mean estimate
 * @param stats Accumulator handle
 * @return Mean value, or NAN if empty or invalid
 */
NIMCP_EXPORT double nimcp_stream_stats_get_mean(const nimcp_stream_stats_t stats);

/**
 * @brief Get current sample variance estimate
 * @param stats Accumulator handle
 * @return Sample variance (n-1 denominator), or NAN if n<2
 */
NIMCP_EXPORT double nimcp_stream_stats_get_variance(const nimcp_stream_stats_t stats);

/**
 * @brief Get current population variance estimate
 * @param stats Accumulator handle
 * @return Population variance (n denominator), or NAN if empty
 */
NIMCP_EXPORT double nimcp_stream_stats_get_variance_population(
    const nimcp_stream_stats_t stats
);

/**
 * @brief Get current sample standard deviation
 * @param stats Accumulator handle
 * @return Sample std dev, or NAN if n<2
 */
NIMCP_EXPORT double nimcp_stream_stats_get_std(const nimcp_stream_stats_t stats);

/**
 * @brief Get current minimum value
 * @param stats Accumulator handle
 * @return Minimum seen so far, or +INFINITY if empty
 */
NIMCP_EXPORT double nimcp_stream_stats_get_min(const nimcp_stream_stats_t stats);

/**
 * @brief Get current maximum value
 * @param stats Accumulator handle
 * @return Maximum seen so far, or -INFINITY if empty
 */
NIMCP_EXPORT double nimcp_stream_stats_get_max(const nimcp_stream_stats_t stats);

/**
 * @brief Get current count of observations
 * @param stats Accumulator handle
 * @return Number of values added, or 0 if invalid
 */
NIMCP_EXPORT uint64_t nimcp_stream_stats_get_count(const nimcp_stream_stats_t stats);

/**
 * @brief Get current sum of all values
 * @param stats Accumulator handle
 * @return Sum of all values, or 0 if empty
 */
NIMCP_EXPORT double nimcp_stream_stats_get_sum(const nimcp_stream_stats_t stats);

/**
 * @brief Get current skewness estimate
 * @param stats Accumulator handle
 * @return Skewness (third standardized moment), or NAN if n<3
 */
NIMCP_EXPORT double nimcp_stream_stats_get_skewness(const nimcp_stream_stats_t stats);

/**
 * @brief Get current kurtosis estimate
 * @param stats Accumulator handle
 * @return Excess kurtosis (fourth moment - 3), or NAN if n<4
 */
NIMCP_EXPORT double nimcp_stream_stats_get_kurtosis(const nimcp_stream_stats_t stats);

/**
 * @brief Reset accumulator to initial state
 * @param stats Accumulator handle
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_stats_reset(
    nimcp_stream_stats_t stats
);

/** @} */ // end stream_stats group

//=============================================================================
// Online Quantile Estimation
//=============================================================================
/** @defgroup stream_quantile Online Quantile Estimation
 * @brief Streaming quantile/percentile estimation algorithms
 *
 * Implements two algorithms:
 *
 * 1. P-squared algorithm (Jain & Chlamtac, 1985):
 *    - O(1) space, O(1) update
 *    - Estimates single quantile (e.g., median)
 *    - Fast but less accurate for extreme quantiles
 *
 * 2. t-digest (Dunning & Ertl, 2019):
 *    - O(compression) space, O(log compression) update
 *    - Estimates any quantile accurately
 *    - Excellent for extreme quantiles (p < 0.01 or p > 0.99)
 *    - Mergeable for parallel computation
 *
 * @{
 */

/**
 * @brief Create a streaming quantile estimator
 * @param config Configuration (NULL for P-squared median)
 * @return New estimator handle, NULL on failure
 *
 * WHAT: Create streaming quantile estimator
 * WHY:  Estimate quantiles without storing all data
 * HOW:  P-squared or t-digest algorithm
 */
NIMCP_EXPORT nimcp_stream_quantile_t nimcp_stream_quantile_create(
    const nimcp_stream_quantile_config_t* config
);

/**
 * @brief Destroy a quantile estimator
 * @param quantile Estimator to destroy (NULL safe)
 */
NIMCP_EXPORT void nimcp_stream_quantile_destroy(nimcp_stream_quantile_t quantile);

/**
 * @brief Add a value to the quantile estimator
 * @param quantile Estimator handle
 * @param value Value to add
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_quantile_update(
    nimcp_stream_quantile_t quantile,
    double value
);

/**
 * @brief Add batch of values to quantile estimator
 * @param quantile Estimator handle
 * @param values Array of values
 * @param count Number of values
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_quantile_update_batch(
    nimcp_stream_quantile_t quantile,
    const float* values,
    uint32_t count
);

/**
 * @brief Get quantile estimate
 * @param quantile Estimator handle
 * @param p Quantile to estimate (0.0 to 1.0)
 * @return Quantile estimate, or NAN on error
 *
 * For P-squared: only returns configured target quantile (ignores p)
 * For t-digest: returns estimate for any p
 */
NIMCP_EXPORT double nimcp_stream_quantile_get(
    const nimcp_stream_quantile_t quantile,
    double p
);

/**
 * @brief Merge two quantile estimators
 * @param dest Destination estimator (receives merged result)
 * @param src Source estimator (unchanged)
 * @return NIMCP_STREAM_OK on success
 *
 * Note: Only t-digest supports merging. P-squared merge approximates.
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_quantile_merge(
    nimcp_stream_quantile_t dest,
    const nimcp_stream_quantile_t src
);

/**
 * @brief Reset quantile estimator
 * @param quantile Estimator handle
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_quantile_reset(
    nimcp_stream_quantile_t quantile
);

/**
 * @brief Specialized median estimator (convenience wrapper)
 * @return New median estimator (P-squared with p=0.5)
 */
NIMCP_EXPORT nimcp_stream_quantile_t nimcp_stream_median_create(void);

/**
 * @brief Get median estimate (convenience function)
 * @param quantile Estimator handle
 * @return Median estimate
 */
NIMCP_EXPORT double nimcp_stream_median(const nimcp_stream_quantile_t quantile);

/** @} */ // end stream_quantile group

//=============================================================================
// Online Covariance/Correlation
//=============================================================================
/** @defgroup stream_cov Online Covariance and Correlation
 * @brief Streaming bivariate and multivariate covariance
 *
 * Uses parallel-friendly online algorithm that enables:
 * - Single-pass covariance computation
 * - Numerically stable updates
 * - Merging of partial results
 *
 * @{
 */

/**
 * @brief Create bivariate covariance accumulator
 * @return New accumulator handle, NULL on failure
 *
 * Tracks covariance and correlation between two variables X and Y.
 */
NIMCP_EXPORT nimcp_stream_cov_t nimcp_stream_cov_create(void);

/**
 * @brief Destroy covariance accumulator
 * @param cov Accumulator to destroy (NULL safe)
 */
NIMCP_EXPORT void nimcp_stream_cov_destroy(nimcp_stream_cov_t cov);

/**
 * @brief Add (x, y) pair to covariance accumulator
 * @param cov Accumulator handle
 * @param x First variable value
 * @param y Second variable value
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_cov_update(
    nimcp_stream_cov_t cov,
    double x,
    double y
);

/**
 * @brief Add batch of (x, y) pairs
 * @param cov Accumulator handle
 * @param x Array of X values
 * @param y Array of Y values
 * @param count Number of pairs
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_cov_update_batch(
    nimcp_stream_cov_t cov,
    const float* x,
    const float* y,
    uint32_t count
);

/**
 * @brief Get current sample covariance estimate
 * @param cov Accumulator handle
 * @return Sample covariance, or NAN if n<2
 */
NIMCP_EXPORT double nimcp_stream_cov_get(const nimcp_stream_cov_t cov);

/**
 * @brief Get current Pearson correlation coefficient
 * @param cov Accumulator handle
 * @return Correlation in [-1, 1], or NAN if insufficient data
 */
NIMCP_EXPORT double nimcp_stream_corr_get(const nimcp_stream_cov_t cov);

/**
 * @brief Merge two covariance accumulators
 * @param dest Destination accumulator
 * @param src Source accumulator
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_cov_merge(
    nimcp_stream_cov_t dest,
    const nimcp_stream_cov_t src
);

/**
 * @brief Reset covariance accumulator
 * @param cov Accumulator handle
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_cov_reset(nimcp_stream_cov_t cov);

/**
 * @brief Create multi-dimensional covariance matrix accumulator
 * @param n_dims Number of dimensions
 * @return New accumulator handle, NULL on failure
 *
 * Memory: O(n_dims^2) for covariance matrix
 */
NIMCP_EXPORT nimcp_stream_cov_matrix_t nimcp_stream_cov_matrix_create(uint32_t n_dims);

/**
 * @brief Destroy covariance matrix accumulator
 * @param cov Accumulator to destroy (NULL safe)
 */
NIMCP_EXPORT void nimcp_stream_cov_matrix_destroy(nimcp_stream_cov_matrix_t cov);

/**
 * @brief Add observation vector to covariance matrix accumulator
 * @param cov Accumulator handle
 * @param values Observation vector (length = n_dims)
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_cov_matrix_update(
    nimcp_stream_cov_matrix_t cov,
    const float* values
);

/**
 * @brief Get covariance matrix
 * @param cov Accumulator handle
 * @param out_matrix Output matrix (n_dims x n_dims, row-major)
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_cov_matrix_get(
    const nimcp_stream_cov_matrix_t cov,
    float* out_matrix
);

/**
 * @brief Get correlation matrix
 * @param cov Accumulator handle
 * @param out_matrix Output matrix (n_dims x n_dims, row-major)
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_corr_matrix_get(
    const nimcp_stream_cov_matrix_t cov,
    float* out_matrix
);

/** @} */ // end stream_cov group

//=============================================================================
// Online PCA (Incremental)
//=============================================================================
/** @defgroup stream_pca Incremental PCA
 * @brief Online principal component analysis
 *
 * Implements incremental PCA that can:
 * - Learn principal components from streaming data
 * - Update components as new data arrives
 * - Transform new data using current components
 *
 * Based on IPCA (Incremental PCA) algorithm.
 *
 * @{
 */

/**
 * @brief Create incremental PCA
 * @param config Configuration
 * @return New PCA handle, NULL on failure
 */
NIMCP_EXPORT nimcp_stream_pca_t nimcp_stream_pca_create(
    const nimcp_stream_pca_config_t* config
);

/**
 * @brief Destroy incremental PCA
 * @param pca PCA to destroy (NULL safe)
 */
NIMCP_EXPORT void nimcp_stream_pca_destroy(nimcp_stream_pca_t pca);

/**
 * @brief Partial fit - update PCA with new batch of data
 * @param pca PCA handle
 * @param data Data matrix (n_samples x n_features, row-major)
 * @param n_samples Number of samples in batch
 * @return NIMCP_STREAM_OK on success
 *
 * Updates principal components using new data batch.
 * Call multiple times as new data arrives.
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_pca_partial_fit(
    nimcp_stream_pca_t pca,
    const float* data,
    uint32_t n_samples
);

/**
 * @brief Transform data using current principal components
 * @param pca PCA handle
 * @param data Input data (n_samples x n_features, row-major)
 * @param n_samples Number of samples
 * @param transformed Output (n_samples x n_components, row-major)
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_pca_transform(
    const nimcp_stream_pca_t pca,
    const float* data,
    uint32_t n_samples,
    float* transformed
);

/**
 * @brief Get current principal components
 * @param pca PCA handle
 * @param components Output (n_components x n_features, row-major)
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_pca_get_components(
    const nimcp_stream_pca_t pca,
    float* components
);

/**
 * @brief Get explained variance for each component
 * @param pca PCA handle
 * @param variances Output array (length = n_components)
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_pca_get_explained_variance(
    const nimcp_stream_pca_t pca,
    float* variances
);

/**
 * @brief Get number of samples seen
 * @param pca PCA handle
 * @return Number of samples processed
 */
NIMCP_EXPORT uint64_t nimcp_stream_pca_get_n_samples(const nimcp_stream_pca_t pca);

/**
 * @brief Reset PCA to initial state
 * @param pca PCA handle
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_pca_reset(nimcp_stream_pca_t pca);

/** @} */ // end stream_pca group

//=============================================================================
// Online Regression
//=============================================================================
/** @defgroup stream_linreg Online Linear Regression
 * @brief Streaming least squares regression
 *
 * Implements online linear regression using:
 * - Sherman-Morrison formula for incremental updates
 * - Recursive least squares (RLS) with forgetting factor
 *
 * @{
 */

/**
 * @brief Create online linear regression
 * @param n_features Number of input features
 * @param forgetting_factor Forgetting factor (0-1, 1=no forgetting)
 * @return New regression handle, NULL on failure
 *
 * The forgetting factor allows adaptation to non-stationary data:
 * - lambda = 1.0: Standard least squares (equal weight to all data)
 * - lambda < 1.0: Recent data weighted more heavily
 * - Typical values: 0.95 to 0.99
 */
NIMCP_EXPORT nimcp_stream_linreg_t nimcp_stream_linreg_create(
    uint32_t n_features,
    float forgetting_factor
);

/**
 * @brief Destroy online linear regression
 * @param reg Regression to destroy (NULL safe)
 */
NIMCP_EXPORT void nimcp_stream_linreg_destroy(nimcp_stream_linreg_t reg);

/**
 * @brief Update regression with new observation
 * @param reg Regression handle
 * @param x Feature vector (length = n_features)
 * @param y Target value
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_linreg_update(
    nimcp_stream_linreg_t reg,
    const float* x,
    float y
);

/**
 * @brief Update regression with batch of observations
 * @param reg Regression handle
 * @param X Feature matrix (n_samples x n_features, row-major)
 * @param y Target vector (n_samples)
 * @param n_samples Number of samples
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_linreg_update_batch(
    nimcp_stream_linreg_t reg,
    const float* X,
    const float* y,
    uint32_t n_samples
);

/**
 * @brief Predict using current coefficients
 * @param reg Regression handle
 * @param x Feature vector
 * @return Predicted value
 */
NIMCP_EXPORT float nimcp_stream_linreg_predict(
    const nimcp_stream_linreg_t reg,
    const float* x
);

/**
 * @brief Predict batch
 * @param reg Regression handle
 * @param X Feature matrix (n_samples x n_features)
 * @param n_samples Number of samples
 * @param predictions Output predictions (n_samples)
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_linreg_predict_batch(
    const nimcp_stream_linreg_t reg,
    const float* X,
    uint32_t n_samples,
    float* predictions
);

/**
 * @brief Get current regression coefficients
 * @param reg Regression handle
 * @param coefficients Output array (length = n_features + 1, intercept last)
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_linreg_get_coefficients(
    const nimcp_stream_linreg_t reg,
    float* coefficients
);

/**
 * @brief Get current R-squared (coefficient of determination)
 * @param reg Regression handle
 * @return R-squared value, or NAN if insufficient data
 */
NIMCP_EXPORT float nimcp_stream_linreg_get_r_squared(const nimcp_stream_linreg_t reg);

/**
 * @brief Reset regression to initial state
 * @param reg Regression handle
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_linreg_reset(
    nimcp_stream_linreg_t reg
);

/** @} */ // end stream_linreg group

//=============================================================================
// Reservoir Sampling
//=============================================================================
/** @defgroup reservoir Reservoir Sampling
 * @brief Uniform random sampling from data streams
 *
 * Algorithm R (Vitter, 1985) maintains a uniform random sample of k items
 * from a stream of unknown length n. Each item has exactly k/n probability
 * of being in the final sample.
 *
 * @{
 */

/**
 * @brief Create reservoir sampler
 * @param reservoir_size Number of items to sample
 * @return New reservoir handle, NULL on failure
 */
NIMCP_EXPORT nimcp_reservoir_t nimcp_reservoir_create(uint32_t reservoir_size);

/**
 * @brief Destroy reservoir sampler
 * @param reservoir Reservoir to destroy (NULL safe)
 */
NIMCP_EXPORT void nimcp_reservoir_destroy(nimcp_reservoir_t reservoir);

/**
 * @brief Add item to stream
 * @param reservoir Reservoir handle
 * @param value Item value
 * @return NIMCP_STREAM_OK on success
 *
 * Uses Algorithm R for O(1) amortized time per item.
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_reservoir_update(
    nimcp_reservoir_t reservoir,
    float value
);

/**
 * @brief Add batch of items
 * @param reservoir Reservoir handle
 * @param values Array of values
 * @param count Number of values
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_reservoir_update_batch(
    nimcp_reservoir_t reservoir,
    const float* values,
    uint32_t count
);

/**
 * @brief Get current sample
 * @param reservoir Reservoir handle
 * @param sample Output array (length = reservoir_size)
 * @param actual_size Output: actual number of items (may be < reservoir_size)
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_reservoir_get_sample(
    const nimcp_reservoir_t reservoir,
    float* sample,
    uint32_t* actual_size
);

/**
 * @brief Get number of items seen in stream
 * @param reservoir Reservoir handle
 * @return Total items seen
 */
NIMCP_EXPORT uint64_t nimcp_reservoir_get_stream_size(const nimcp_reservoir_t reservoir);

/**
 * @brief Reset reservoir
 * @param reservoir Reservoir handle
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_reservoir_reset(nimcp_reservoir_t reservoir);

/**
 * @brief Create weighted reservoir sampler
 * @param reservoir_size Number of items to sample
 * @return New weighted reservoir handle, NULL on failure
 *
 * Uses Algorithm A-Res (Efraimidis & Spirakis, 2006) for weighted sampling.
 */
NIMCP_EXPORT nimcp_reservoir_t nimcp_reservoir_weighted_create(uint32_t reservoir_size);

/**
 * @brief Add weighted item to stream
 * @param reservoir Reservoir handle (must be weighted)
 * @param value Item value
 * @param weight Item weight (positive)
 * @return NIMCP_STREAM_OK on success
 *
 * Items are sampled with probability proportional to weight.
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_reservoir_weighted_update(
    nimcp_reservoir_t reservoir,
    float value,
    float weight
);

/** @} */ // end reservoir group

//=============================================================================
// Count-Min Sketch
//=============================================================================
/** @defgroup cms Count-Min Sketch
 * @brief Sublinear space frequency estimation
 *
 * Count-Min Sketch (Cormode & Muthukrishnan, 2005) provides:
 * - O(width * depth) space
 * - O(depth) update and query time
 * - Point query: estimate count(i) with error at most epsilon * ||a||_1
 *   with probability at least 1 - delta
 *   where width = ceil(e/epsilon), depth = ceil(ln(1/delta))
 *
 * @{
 */

/**
 * @brief Create Count-Min Sketch
 * @param width Number of counters per row
 * @param depth Number of hash functions
 * @return New CMS handle, NULL on failure
 *
 * Memory: width * depth * sizeof(uint32_t) bytes
 *
 * Typical parameters:
 * - epsilon = 0.001 (0.1% error) -> width = 2719
 * - delta = 0.01 (99% confidence) -> depth = 5
 */
NIMCP_EXPORT nimcp_cms_t nimcp_cms_create(uint32_t width, uint32_t depth);

/**
 * @brief Destroy Count-Min Sketch
 * @param cms Sketch to destroy (NULL safe)
 */
NIMCP_EXPORT void nimcp_cms_destroy(nimcp_cms_t cms);

/**
 * @brief Add item with count to sketch
 * @param cms Sketch handle
 * @param item Item identifier (hashed internally)
 * @param count Count to add (typically 1)
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_cms_update(
    nimcp_cms_t cms,
    uint64_t item,
    int32_t count
);

/**
 * @brief Add item by string key
 * @param cms Sketch handle
 * @param key String key (hashed internally)
 * @param count Count to add
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_cms_update_string(
    nimcp_cms_t cms,
    const char* key,
    int32_t count
);

/**
 * @brief Query frequency estimate
 * @param cms Sketch handle
 * @param item Item identifier
 * @return Estimated count (may overestimate, never underestimates)
 */
NIMCP_EXPORT uint64_t nimcp_cms_query(const nimcp_cms_t cms, uint64_t item);

/**
 * @brief Query by string key
 * @param cms Sketch handle
 * @param key String key
 * @return Estimated count
 */
NIMCP_EXPORT uint64_t nimcp_cms_query_string(const nimcp_cms_t cms, const char* key);

/**
 * @brief Merge two sketches
 * @param dest Destination sketch (receives merged result)
 * @param src Source sketch (unchanged)
 * @return NIMCP_STREAM_OK on success
 *
 * Requires same width and depth.
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_cms_merge(
    nimcp_cms_t dest,
    const nimcp_cms_t src
);

/**
 * @brief Get total count of all items
 * @param cms Sketch handle
 * @return Total count added to sketch
 */
NIMCP_EXPORT uint64_t nimcp_cms_get_total_count(const nimcp_cms_t cms);

/**
 * @brief Reset sketch to zero
 * @param cms Sketch handle
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_cms_reset(nimcp_cms_t cms);

/** @} */ // end cms group

//=============================================================================
// HyperLogLog
//=============================================================================
/** @defgroup hll HyperLogLog
 * @brief Sublinear space cardinality estimation
 *
 * HyperLogLog (Flajolet et al., 2007) provides:
 * - O(2^p) space (typically ~1KB for good accuracy)
 * - O(1) update time
 * - Relative error approximately 1.04 / sqrt(2^p)
 *
 * @{
 */

/**
 * @brief Create HyperLogLog counter
 * @param precision Precision parameter p (4 to 18)
 * @return New HLL handle, NULL on failure
 *
 * Memory: 2^p bytes
 * Relative error: ~1.04 / sqrt(2^p)
 *
 * Typical values:
 * - p = 10: 1KB, ~3.25% error
 * - p = 12: 4KB, ~1.625% error
 * - p = 14: 16KB, ~0.8% error (default)
 * - p = 16: 64KB, ~0.4% error
 */
NIMCP_EXPORT nimcp_hll_t nimcp_hll_create(uint32_t precision);

/**
 * @brief Destroy HyperLogLog counter
 * @param hll Counter to destroy (NULL safe)
 */
NIMCP_EXPORT void nimcp_hll_destroy(nimcp_hll_t hll);

/**
 * @brief Add item to HLL
 * @param hll Counter handle
 * @param item Item identifier (hashed internally)
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_hll_add(nimcp_hll_t hll, uint64_t item);

/**
 * @brief Add item by string key
 * @param hll Counter handle
 * @param key String key
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_hll_add_string(
    nimcp_hll_t hll,
    const char* key
);

/**
 * @brief Add batch of items
 * @param hll Counter handle
 * @param items Array of item identifiers
 * @param count Number of items
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_hll_add_batch(
    nimcp_hll_t hll,
    const uint64_t* items,
    uint32_t count
);

/**
 * @brief Get cardinality estimate
 * @param hll Counter handle
 * @return Estimated number of distinct items
 */
NIMCP_EXPORT uint64_t nimcp_hll_count(const nimcp_hll_t hll);

/**
 * @brief Merge two HLL counters
 * @param dest Destination counter (receives union)
 * @param src Source counter (unchanged)
 * @return NIMCP_STREAM_OK on success
 *
 * Requires same precision. Result estimates |A union B|.
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_hll_merge(
    nimcp_hll_t dest,
    const nimcp_hll_t src
);

/**
 * @brief Reset HLL counter
 * @param hll Counter handle
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_hll_reset(nimcp_hll_t hll);

/** @} */ // end hll group

//=============================================================================
// Exponentially Weighted Moving Statistics
//=============================================================================
/** @defgroup ewma Exponentially Weighted Moving Statistics
 * @brief EWMA and EWMV for time-series smoothing
 *
 * Exponentially weighted moving average/variance give more weight
 * to recent observations, useful for:
 * - Trend detection
 * - Anomaly detection
 * - Adaptive filtering
 *
 * EWMA_t = alpha * x_t + (1 - alpha) * EWMA_{t-1}
 *
 * @{
 */

/**
 * @brief Create EWMA accumulator
 * @param alpha Smoothing factor (0 < alpha <= 1)
 * @return New EWMA handle, NULL on failure
 *
 * Common alpha values:
 * - alpha = 2/(N+1) where N is equivalent window size
 * - alpha = 0.1: ~19-sample effective window
 * - alpha = 0.05: ~39-sample effective window
 */
NIMCP_EXPORT nimcp_ewma_t nimcp_ewma_create(float alpha);

/**
 * @brief Destroy EWMA accumulator
 * @param ewma Accumulator to destroy (NULL safe)
 */
NIMCP_EXPORT void nimcp_ewma_destroy(nimcp_ewma_t ewma);

/**
 * @brief Add value to EWMA
 * @param ewma Accumulator handle
 * @param value Value to add
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_ewma_update(
    nimcp_ewma_t ewma,
    double value
);

/**
 * @brief Get current EWMA value
 * @param ewma Accumulator handle
 * @return Current EWMA, or NAN if no data
 */
NIMCP_EXPORT double nimcp_ewma_get(const nimcp_ewma_t ewma);

/**
 * @brief Reset EWMA to initial state
 * @param ewma Accumulator handle
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_ewma_reset(nimcp_ewma_t ewma);

/**
 * @brief Create EWMV (exponentially weighted moving variance) accumulator
 * @param alpha Smoothing factor
 * @return New EWMV handle, NULL on failure
 *
 * Tracks both EWMA and exponentially weighted moving variance.
 */
NIMCP_EXPORT nimcp_ewma_t nimcp_ewmv_create(float alpha);

/**
 * @brief Get current EWMV (variance) value
 * @param ewma Accumulator handle (must be EWMV type)
 * @return Current EWMV, or NAN if insufficient data
 */
NIMCP_EXPORT double nimcp_ewmv_get(const nimcp_ewma_t ewma);

/**
 * @brief Get current EWMS (standard deviation)
 * @param ewma Accumulator handle (must be EWMV type)
 * @return Current sqrt(EWMV), or NAN if insufficient data
 */
NIMCP_EXPORT double nimcp_ewms_get(const nimcp_ewma_t ewma);

/** @} */ // end ewma group

//=============================================================================
// GPU Acceleration
//=============================================================================
/** @defgroup stream_gpu GPU Acceleration
 * @brief GPU-accelerated batch operations
 *
 * These functions use CUDA for batch processing when available.
 * Falls back to CPU implementation if GPU not available.
 *
 * @{
 */

/**
 * @brief Check if GPU acceleration is available
 * @return true if GPU available and initialized
 */
NIMCP_EXPORT bool nimcp_stream_stats_gpu_available(void);

/**
 * @brief GPU-accelerated batch statistics
 * @param values Device pointer to values array
 * @param count Number of values
 * @param d_mean Output: mean (device pointer or NULL)
 * @param d_variance Output: variance (device pointer or NULL)
 * @param d_min Output: min (device pointer or NULL)
 * @param d_max Output: max (device pointer or NULL)
 * @return NIMCP_STREAM_OK on success
 *
 * All pointers must be device memory.
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_stats_gpu_batch(
    const float* values,
    uint32_t count,
    float* d_mean,
    float* d_variance,
    float* d_min,
    float* d_max
);

/**
 * @brief GPU-accelerated covariance batch
 * @param x Device pointer to X values
 * @param y Device pointer to Y values
 * @param count Number of pairs
 * @param d_covariance Output: covariance (device pointer)
 * @return NIMCP_STREAM_OK on success
 */
NIMCP_EXPORT nimcp_stream_stats_result_t nimcp_stream_cov_gpu_batch(
    const float* x,
    const float* y,
    uint32_t count,
    float* d_covariance
);

/** @} */ // end stream_gpu group

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error message for result code
 * @param result Result code
 * @return Human-readable error message
 */
NIMCP_EXPORT const char* nimcp_stream_stats_error_string(
    nimcp_stream_stats_result_t result
);

/**
 * @brief Hash function for string keys (FNV-1a)
 * @param key String to hash
 * @return 64-bit hash value
 */
NIMCP_EXPORT uint64_t nimcp_stream_hash_string(const char* key);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_STREAMING_STATISTICS_H
