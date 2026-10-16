//=============================================================================
// nimcp_statistics_gpu.h - GPU-Accelerated Statistics Module
//=============================================================================
/**
 * @file nimcp_statistics_gpu.h
 * @brief GPU-accelerated statistical computations for NIMCP
 *
 * WHAT: GPU acceleration for descriptive statistics, bootstrap methods,
 *       distribution operations, information theory, and matrix decompositions
 * WHY:  Achieve 20-100x speedup for batch statistical operations used in
 *       neural network training, data analysis, and scientific computing
 * HOW:  CUDA kernels with cuBLAS for matrix ops, cuRAND for sampling,
 *       cuSOLVER for decompositions, and optimized parallel reductions
 *
 * ARCHITECTURE:
 *
 *   +------------------------------------------------------------------+
 *   |                  GPU STATISTICS ACCELERATION                      |
 *   |                                                                  |
 *   |  +----------------+  +------------------+  +------------------+  |
 *   |  | Descriptive    |  | Bootstrap        |  | Distribution     |  |
 *   |  | (mean, var,    |  | Methods          |  | Operations       |  |
 *   |  |  cov, corr)    |  | (CI, resampling) |  | (sample, PDF)    |  |
 *   |  +----------------+  +------------------+  +------------------+  |
 *   |           |                  |                    |              |
 *   |  +----------------+  +------------------+  +------------------+  |
 *   |  | Information    |  | Matrix Ops       |  | Quantile         |  |
 *   |  | Theory (H, MI, |  | (eigen, SVD,     |  | Estimation       |  |
 *   |  |  KL divergence)|  |  inverse)        |  |                  |  |
 *   |  +----------------+  +------------------+  +------------------+  |
 *   +------------------------------------------------------------------+
 *
 * EXPECTED PERFORMANCE:
 *   - Batch mean/variance (1M samples): 50ms CPU -> 1ms GPU (50x)
 *   - Covariance matrix (1000x1000): 500ms CPU -> 10ms GPU (50x)
 *   - Bootstrap (10K resamples): 2s CPU -> 20ms GPU (100x)
 *   - Eigendecomposition (512x512): 100ms CPU -> 5ms GPU (20x)
 *
 * THREAD SAFETY:
 *   - State creation/destruction: NOT thread-safe
 *   - Computation operations: Thread-safe with different contexts
 *   - Same context from multiple threads: NOT thread-safe
 *
 * CPU FALLBACK:
 *   - All functions return error codes when CUDA unavailable
 *   - Use CPU statistics functions as fallback
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_STATISTICS_GPU_H
#define NIMCP_STATISTICS_GPU_H

#include "common/nimcp_export.h"
#include "gpu/context/nimcp_gpu_context.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define BIO_MODULE_STATISTICS_GPU           0x03B0

/** Maximum samples for single-pass operations */
#define STATS_GPU_MAX_SAMPLES               (1 << 24)  // 16M

/** Maximum variables for covariance/correlation */
#define STATS_GPU_MAX_VARIABLES             4096

/** Maximum bootstrap resamples */
#define STATS_GPU_MAX_BOOTSTRAP_RESAMPLES   100000

/** Maximum quantiles to compute at once */
#define STATS_GPU_MAX_QUANTILES             256

/** Default CUDA block size for statistics kernels */
#define STATS_GPU_BLOCK_SIZE                256

/** Reduction block size (power of 2) */
#define STATS_GPU_REDUCTION_BLOCK_SIZE      512

//=============================================================================
// Error Codes
//=============================================================================

#define STATS_GPU_ERR_BASE                  34000

#define STATS_GPU_ERR_OK                    0
#define STATS_GPU_ERR_NULL_CTX              (STATS_GPU_ERR_BASE + 1)
#define STATS_GPU_ERR_NULL_INPUT            (STATS_GPU_ERR_BASE + 2)
#define STATS_GPU_ERR_NULL_OUTPUT           (STATS_GPU_ERR_BASE + 3)
#define STATS_GPU_ERR_ALLOC                 (STATS_GPU_ERR_BASE + 4)
#define STATS_GPU_ERR_CUDA                  (STATS_GPU_ERR_BASE + 5)
#define STATS_GPU_ERR_CUBLAS                (STATS_GPU_ERR_BASE + 6)
#define STATS_GPU_ERR_CUSOLVER              (STATS_GPU_ERR_BASE + 7)
#define STATS_GPU_ERR_CURAND                (STATS_GPU_ERR_BASE + 8)
#define STATS_GPU_ERR_KERNEL_LAUNCH         (STATS_GPU_ERR_BASE + 9)
#define STATS_GPU_ERR_INVALID_PARAMS        (STATS_GPU_ERR_BASE + 10)
#define STATS_GPU_ERR_TOO_MANY_SAMPLES      (STATS_GPU_ERR_BASE + 11)
#define STATS_GPU_ERR_TOO_MANY_VARIABLES    (STATS_GPU_ERR_BASE + 12)
#define STATS_GPU_ERR_SINGULAR_MATRIX       (STATS_GPU_ERR_BASE + 13)
#define STATS_GPU_ERR_CONVERGENCE           (STATS_GPU_ERR_BASE + 14)
#define STATS_GPU_ERR_RNG_NOT_INIT          (STATS_GPU_ERR_BASE + 15)
#define STATS_GPU_ERR_DIMENSION_MISMATCH    (STATS_GPU_ERR_BASE + 16)

//=============================================================================
// Forward Declarations
//=============================================================================

/** GPU RNG state handle for statistical sampling */
typedef struct stats_gpu_rng_s stats_gpu_rng_t;

/** GPU statistics workspace for reusable allocations */
typedef struct stats_gpu_workspace_s stats_gpu_workspace_t;

//=============================================================================
// Distribution Types
//=============================================================================

/**
 * @brief Probability distribution types supported for GPU operations
 */
typedef enum {
    STATS_GPU_DIST_NORMAL,              /**< Normal (Gaussian) distribution */
    STATS_GPU_DIST_UNIFORM,             /**< Uniform distribution */
    STATS_GPU_DIST_EXPONENTIAL,         /**< Exponential distribution */
    STATS_GPU_DIST_POISSON,             /**< Poisson distribution */
    STATS_GPU_DIST_GAMMA,               /**< Gamma distribution */
    STATS_GPU_DIST_BETA,                /**< Beta distribution */
    STATS_GPU_DIST_LOGNORMAL,           /**< Log-normal distribution */
    STATS_GPU_DIST_CHI_SQUARED,         /**< Chi-squared distribution */
    STATS_GPU_DIST_STUDENT_T,           /**< Student's t-distribution */
    STATS_GPU_DIST_CAUCHY,              /**< Cauchy distribution */
    STATS_GPU_DIST_COUNT
} stats_gpu_distribution_t;

/**
 * @brief Variance computation mode (population vs sample)
 */
typedef enum {
    STATS_GPU_VAR_POPULATION,           /**< Population variance (divide by n) */
    STATS_GPU_VAR_SAMPLE,               /**< Sample variance (divide by n-1) */
} stats_gpu_variance_mode_t;

/**
 * @brief Bootstrap statistic type
 */
typedef enum {
    STATS_GPU_BOOTSTRAP_MEAN,           /**< Bootstrap the mean */
    STATS_GPU_BOOTSTRAP_VARIANCE,       /**< Bootstrap the variance */
    STATS_GPU_BOOTSTRAP_MEDIAN,         /**< Bootstrap the median */
    STATS_GPU_BOOTSTRAP_PERCENTILE,     /**< Bootstrap a percentile */
    STATS_GPU_BOOTSTRAP_CUSTOM,         /**< Custom statistic (user-defined) */
} stats_gpu_bootstrap_stat_t;

/**
 * @brief Confidence interval method
 */
typedef enum {
    STATS_GPU_CI_PERCENTILE,            /**< Percentile method */
    STATS_GPU_CI_BCa,                   /**< Bias-corrected and accelerated */
    STATS_GPU_CI_NORMAL,                /**< Normal approximation */
} stats_gpu_ci_method_t;

//=============================================================================
// Parameter Structures
//=============================================================================

/**
 * @brief Parameters for batch mean/variance computation
 */
typedef struct {
    uint32_t num_samples;               /**< Number of samples per variable */
    uint32_t num_variables;             /**< Number of variables (for batch) */
    stats_gpu_variance_mode_t var_mode; /**< Population or sample variance */
    bool compute_std;                   /**< Also compute standard deviation */
    bool compute_skewness;              /**< Also compute skewness */
    bool compute_kurtosis;              /**< Also compute kurtosis */
    bool use_welford;                   /**< Use Welford's algorithm (more stable) */
} stats_gpu_descriptive_params_t;

/**
 * @brief Parameters for covariance/correlation matrix computation
 */
typedef struct {
    uint32_t num_samples;               /**< Number of observations */
    uint32_t num_variables;             /**< Number of variables */
    stats_gpu_variance_mode_t var_mode; /**< Population or sample covariance */
    bool compute_correlation;           /**< Compute correlation (normalized cov) */
    bool row_major;                     /**< Data is row-major (samples x variables) */
} stats_gpu_covariance_params_t;

/**
 * @brief Parameters for quantile estimation
 */
typedef struct {
    uint32_t num_samples;               /**< Number of samples */
    const float* quantiles;             /**< Quantile values to compute [0,1] */
    uint32_t num_quantiles;             /**< Number of quantiles */
    bool interpolate;                   /**< Interpolate between adjacent values */
} stats_gpu_quantile_params_t;

/**
 * @brief Parameters for bootstrap resampling
 */
typedef struct {
    uint32_t num_samples;               /**< Original sample size */
    uint32_t num_resamples;             /**< Number of bootstrap resamples */
    stats_gpu_bootstrap_stat_t stat;    /**< Statistic to bootstrap */
    float percentile;                   /**< Percentile (if stat == PERCENTILE) */
    uint64_t seed;                      /**< RNG seed (0 for time-based) */
    bool stratified;                    /**< Use stratified resampling */
} stats_gpu_bootstrap_params_t;

/**
 * @brief Parameters for confidence interval computation
 */
typedef struct {
    float confidence_level;             /**< Confidence level (e.g., 0.95) */
    stats_gpu_ci_method_t method;       /**< CI computation method */
    uint32_t num_bootstrap_samples;     /**< Bootstrap samples for BCa */
} stats_gpu_ci_params_t;

/**
 * @brief Parameters for distribution sampling
 */
typedef struct {
    stats_gpu_distribution_t distribution; /**< Distribution type */
    float param1;                       /**< First parameter (e.g., mean, min) */
    float param2;                       /**< Second parameter (e.g., std, max) */
    uint32_t num_samples;               /**< Number of samples to generate */
    uint64_t seed;                      /**< RNG seed (0 for time-based) */
} stats_gpu_sample_params_t;

/**
 * @brief Parameters for batch PDF/CDF evaluation
 */
typedef struct {
    stats_gpu_distribution_t distribution; /**< Distribution type */
    float param1;                       /**< First distribution parameter */
    float param2;                       /**< Second distribution parameter */
    uint32_t num_points;                /**< Number of evaluation points */
    bool log_scale;                     /**< Return log-PDF/CDF */
} stats_gpu_density_params_t;

/**
 * @brief Parameters for entropy and information theory
 */
typedef struct {
    uint32_t num_samples;               /**< Number of samples */
    uint32_t num_bins;                  /**< Histogram bins for estimation */
    float bandwidth;                    /**< KDE bandwidth (0 for auto) */
    bool use_kde;                       /**< Use KDE instead of histogram */
    float base;                         /**< Logarithm base (2=bits, e=nats) */
} stats_gpu_entropy_params_t;

/**
 * @brief Parameters for eigendecomposition
 */
typedef struct {
    uint32_t n;                         /**< Matrix dimension (n x n) */
    bool compute_eigenvectors;          /**< Compute eigenvectors */
    bool symmetric;                     /**< Matrix is symmetric (use faster algo) */
    uint32_t max_iterations;            /**< Max iterations for iterative methods */
    float tolerance;                    /**< Convergence tolerance */
} stats_gpu_eigen_params_t;

/**
 * @brief Parameters for SVD
 */
typedef struct {
    uint32_t m;                         /**< Number of rows */
    uint32_t n;                         /**< Number of columns */
    bool compute_u;                     /**< Compute left singular vectors */
    bool compute_v;                     /**< Compute right singular vectors */
    bool economy;                       /**< Economy/thin SVD */
} stats_gpu_svd_params_t;

//=============================================================================
// Result Structures
//=============================================================================

/**
 * @brief Result of descriptive statistics computation
 */
typedef struct {
    float* means;                       /**< Means [num_variables] */
    float* variances;                   /**< Variances [num_variables] */
    float* std_devs;                    /**< Standard deviations [num_variables] */
    float* skewness;                    /**< Skewness [num_variables] */
    float* kurtosis;                    /**< Excess kurtosis [num_variables] */
    float* mins;                        /**< Minimum values [num_variables] */
    float* maxs;                        /**< Maximum values [num_variables] */
    uint32_t num_variables;             /**< Number of variables */
    float kernel_time_ms;               /**< GPU kernel time */
} stats_gpu_descriptive_result_t;

/**
 * @brief Result of bootstrap analysis
 */
typedef struct {
    float point_estimate;               /**< Original statistic value */
    float* bootstrap_distribution;      /**< Bootstrap distribution [num_resamples] */
    float standard_error;               /**< Bootstrap standard error */
    float bias;                         /**< Bootstrap bias estimate */
    uint32_t num_resamples;             /**< Number of resamples completed */
    float kernel_time_ms;               /**< GPU kernel time */
} stats_gpu_bootstrap_result_t;

/**
 * @brief Confidence interval result
 */
typedef struct {
    float lower;                        /**< Lower bound */
    float upper;                        /**< Upper bound */
    float point_estimate;               /**< Point estimate */
    float confidence_level;             /**< Confidence level used */
} stats_gpu_ci_result_t;

/**
 * @brief Result of eigendecomposition
 */
typedef struct {
    float* eigenvalues;                 /**< Eigenvalues [n] (descending order) */
    float* eigenvectors;                /**< Eigenvectors [n x n] (column-major) */
    uint32_t n;                         /**< Matrix dimension */
    uint32_t rank;                      /**< Numerical rank */
    float kernel_time_ms;               /**< GPU kernel time */
} stats_gpu_eigen_result_t;

/**
 * @brief Result of SVD
 */
typedef struct {
    float* singular_values;             /**< Singular values [min(m,n)] */
    float* u;                           /**< Left singular vectors [m x k] */
    float* vt;                          /**< Right singular vectors (transposed) [k x n] */
    uint32_t m;                         /**< Number of rows */
    uint32_t n;                         /**< Number of columns */
    uint32_t k;                         /**< Number of singular values */
    float kernel_time_ms;               /**< GPU kernel time */
} stats_gpu_svd_result_t;

/**
 * @brief GPU statistics operation statistics
 */
typedef struct {
    uint64_t mean_computations;         /**< Total mean computations */
    uint64_t variance_computations;     /**< Total variance computations */
    uint64_t covariance_computations;   /**< Total covariance matrix computations */
    uint64_t bootstrap_resamples;       /**< Total bootstrap resamples */
    uint64_t samples_generated;         /**< Total random samples generated */
    uint64_t pdf_evaluations;           /**< Total PDF evaluations */
    uint64_t entropy_computations;      /**< Total entropy computations */
    uint64_t eigendecompositions;       /**< Total eigendecompositions */
    uint64_t svd_decompositions;        /**< Total SVD decompositions */
    float total_kernel_time_ms;         /**< Total GPU kernel time */
    size_t peak_memory_bytes;           /**< Peak GPU memory usage */
} stats_gpu_stats_t;

//=============================================================================
// Default Parameter Initializers
//=============================================================================

/**
 * @brief Default descriptive statistics parameters
 */
static inline stats_gpu_descriptive_params_t stats_gpu_descriptive_params_default(void)
{
    stats_gpu_descriptive_params_t params = {
        .num_samples = 0,
        .num_variables = 1,
        .var_mode = STATS_GPU_VAR_SAMPLE,
        .compute_std = true,
        .compute_skewness = false,
        .compute_kurtosis = false,
        .use_welford = true
    };
    return params;
}

/**
 * @brief Default covariance parameters
 */
static inline stats_gpu_covariance_params_t stats_gpu_covariance_params_default(void)
{
    stats_gpu_covariance_params_t params = {
        .num_samples = 0,
        .num_variables = 0,
        .var_mode = STATS_GPU_VAR_SAMPLE,
        .compute_correlation = false,
        .row_major = true
    };
    return params;
}

/**
 * @brief Default bootstrap parameters
 */
static inline stats_gpu_bootstrap_params_t stats_gpu_bootstrap_params_default(void)
{
    stats_gpu_bootstrap_params_t params = {
        .num_samples = 0,
        .num_resamples = 10000,
        .stat = STATS_GPU_BOOTSTRAP_MEAN,
        .percentile = 0.5f,
        .seed = 0,
        .stratified = false
    };
    return params;
}

/**
 * @brief Default CI parameters
 */
static inline stats_gpu_ci_params_t stats_gpu_ci_params_default(void)
{
    stats_gpu_ci_params_t params = {
        .confidence_level = 0.95f,
        .method = STATS_GPU_CI_PERCENTILE,
        .num_bootstrap_samples = 10000
    };
    return params;
}

/**
 * @brief Default entropy parameters
 */
static inline stats_gpu_entropy_params_t stats_gpu_entropy_params_default(void)
{
    stats_gpu_entropy_params_t params = {
        .num_samples = 0,
        .num_bins = 256,
        .bandwidth = 0.0f,
        .use_kde = false,
        .base = 2.0f
    };
    return params;
}

//=============================================================================
// RNG Lifecycle
//=============================================================================

/**
 * @brief Create GPU RNG state for statistical sampling
 *
 * @param ctx   GPU context
 * @param n     Number of parallel generators
 * @param seed  Initial seed (0 for time-based)
 * @return RNG handle on success, NULL on failure
 */
NIMCP_EXPORT stats_gpu_rng_t* stats_gpu_rng_create(
    nimcp_gpu_context_t* ctx,
    uint32_t n,
    uint64_t seed);

/**
 * @brief Destroy GPU RNG state
 */
NIMCP_EXPORT void stats_gpu_rng_destroy(stats_gpu_rng_t* rng);

/**
 * @brief Reseed GPU RNG
 */
NIMCP_EXPORT bool stats_gpu_rng_reseed(stats_gpu_rng_t* rng, uint64_t seed);

//=============================================================================
// Workspace Management
//=============================================================================

/**
 * @brief Create GPU workspace for statistics operations
 *
 * Pre-allocates device memory to avoid repeated allocations.
 *
 * @param ctx          GPU context
 * @param max_samples  Maximum samples to support
 * @param max_vars     Maximum variables to support
 * @return Workspace on success, NULL on failure
 */
NIMCP_EXPORT stats_gpu_workspace_t* stats_gpu_workspace_create(
    nimcp_gpu_context_t* ctx,
    uint32_t max_samples,
    uint32_t max_vars);

/**
 * @brief Destroy GPU workspace
 */
NIMCP_EXPORT void stats_gpu_workspace_destroy(stats_gpu_workspace_t* workspace);

//=============================================================================
// Descriptive Statistics API (Batch)
//=============================================================================

/**
 * @brief Compute mean for batch of variables on GPU
 *
 * WHAT: Parallel mean computation using reduction
 * WHY:  50x speedup over CPU for large batches
 * HOW:  Two-level parallel reduction with Kahan summation
 *
 * @param ctx        GPU context
 * @param data       Input data [num_samples x num_variables] (device)
 * @param means_out  Output means [num_variables] (device)
 * @param params     Computation parameters
 * @return true on success
 *
 * EXAMPLE:
 *   float* d_data;  // [1000000 x 100] on device
 *   float* d_means; // [100] on device
 *   stats_gpu_descriptive_params_t params = stats_gpu_descriptive_params_default();
 *   params.num_samples = 1000000;
 *   params.num_variables = 100;
 *   nimcp_stats_gpu_mean_batch(ctx, d_data, d_means, &params);
 */
NIMCP_EXPORT bool nimcp_stats_gpu_mean_batch(
    nimcp_gpu_context_t* ctx,
    const float* data,
    float* means_out,
    const stats_gpu_descriptive_params_t* params);

/**
 * @brief Compute variance for batch of variables on GPU (Welford's algorithm)
 *
 * Numerically stable single-pass variance computation.
 *
 * @param ctx           GPU context
 * @param data          Input data [num_samples x num_variables] (device)
 * @param means_out     Output means [num_variables] (device, can be NULL)
 * @param variances_out Output variances [num_variables] (device)
 * @param params        Computation parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_stats_gpu_variance_batch(
    nimcp_gpu_context_t* ctx,
    const float* data,
    float* means_out,
    float* variances_out,
    const stats_gpu_descriptive_params_t* params);

/**
 * @brief Compute covariance matrix on GPU
 *
 * Uses cuBLAS for efficient matrix multiplication.
 *
 * @param ctx       GPU context
 * @param data      Input data (device)
 * @param cov_out   Output covariance matrix [num_vars x num_vars] (device)
 * @param params    Computation parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_stats_gpu_covariance_matrix(
    nimcp_gpu_context_t* ctx,
    const float* data,
    float* cov_out,
    const stats_gpu_covariance_params_t* params);

/**
 * @brief Compute correlation matrix on GPU
 *
 * Covariance matrix normalized by standard deviations.
 *
 * @param ctx       GPU context
 * @param data      Input data (device)
 * @param corr_out  Output correlation matrix [num_vars x num_vars] (device)
 * @param params    Computation parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_stats_gpu_correlation_matrix(
    nimcp_gpu_context_t* ctx,
    const float* data,
    float* corr_out,
    const stats_gpu_covariance_params_t* params);

/**
 * @brief Compute multiple quantiles in parallel on GPU
 *
 * @param ctx           GPU context
 * @param data          Input data [num_samples] (device)
 * @param quantiles_out Output quantile values [num_quantiles] (device)
 * @param params        Quantile parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_stats_gpu_quantiles_batch(
    nimcp_gpu_context_t* ctx,
    const float* data,
    float* quantiles_out,
    const stats_gpu_quantile_params_t* params);

//=============================================================================
// Bootstrap Methods API
//=============================================================================

/**
 * @brief GPU-accelerated bootstrap resampling
 *
 * WHAT: Parallel bootstrap with GPU-accelerated statistic computation
 * WHY:  100x speedup for bootstrap-based inference
 * HOW:  Parallel resampling with cuRAND, batch statistic computation
 *
 * @param ctx     GPU context
 * @param rng     GPU RNG state
 * @param data    Original sample [num_samples] (device)
 * @param result  Output bootstrap result
 * @param params  Bootstrap parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_stats_gpu_bootstrap(
    nimcp_gpu_context_t* ctx,
    stats_gpu_rng_t* rng,
    const float* data,
    stats_gpu_bootstrap_result_t* result,
    const stats_gpu_bootstrap_params_t* params);

/**
 * @brief Compute bootstrap confidence intervals on GPU
 *
 * @param ctx         GPU context
 * @param rng         GPU RNG state
 * @param data        Original sample (device)
 * @param num_samples Sample size
 * @param ci_out      Output confidence interval
 * @param params      CI parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_stats_gpu_bootstrap_ci(
    nimcp_gpu_context_t* ctx,
    stats_gpu_rng_t* rng,
    const float* data,
    uint32_t num_samples,
    stats_gpu_ci_result_t* ci_out,
    const stats_gpu_ci_params_t* params);

/**
 * @brief Free bootstrap result resources
 */
NIMCP_EXPORT void nimcp_stats_gpu_bootstrap_result_free(
    nimcp_gpu_context_t* ctx,
    stats_gpu_bootstrap_result_t* result);

//=============================================================================
// Distribution Operations API
//=============================================================================

/**
 * @brief Batch normal sampling on GPU (cuRAND)
 *
 * @param ctx       GPU context
 * @param rng       GPU RNG state
 * @param mean      Distribution mean
 * @param std       Distribution standard deviation
 * @param samples   Output samples [num_samples] (device)
 * @param n         Number of samples
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_stats_gpu_sample_normal(
    nimcp_gpu_context_t* ctx,
    stats_gpu_rng_t* rng,
    float mean,
    float std,
    float* samples,
    uint32_t n);

/**
 * @brief Batch uniform sampling on GPU (cuRAND)
 *
 * @param ctx       GPU context
 * @param rng       GPU RNG state
 * @param min_val   Minimum value
 * @param max_val   Maximum value
 * @param samples   Output samples [num_samples] (device)
 * @param n         Number of samples
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_stats_gpu_sample_uniform(
    nimcp_gpu_context_t* ctx,
    stats_gpu_rng_t* rng,
    float min_val,
    float max_val,
    float* samples,
    uint32_t n);

/**
 * @brief Batch sample from various distributions on GPU
 *
 * Supports normal, uniform, exponential, gamma, beta, etc.
 *
 * @param ctx       GPU context
 * @param rng       GPU RNG state
 * @param samples   Output samples [num_samples] (device)
 * @param params    Sampling parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_stats_gpu_sample_distribution(
    nimcp_gpu_context_t* ctx,
    stats_gpu_rng_t* rng,
    float* samples,
    const stats_gpu_sample_params_t* params);

/**
 * @brief Batch PDF evaluation on GPU
 *
 * @param ctx       GPU context
 * @param points    Evaluation points [num_points] (device)
 * @param pdf_out   Output PDF values [num_points] (device)
 * @param params    Density parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_stats_gpu_pdf_batch(
    nimcp_gpu_context_t* ctx,
    const float* points,
    float* pdf_out,
    const stats_gpu_density_params_t* params);

/**
 * @brief Batch CDF evaluation on GPU
 *
 * @param ctx       GPU context
 * @param points    Evaluation points [num_points] (device)
 * @param cdf_out   Output CDF values [num_points] (device)
 * @param params    Density parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_stats_gpu_cdf_batch(
    nimcp_gpu_context_t* ctx,
    const float* points,
    float* cdf_out,
    const stats_gpu_density_params_t* params);

//=============================================================================
// Information Theory API
//=============================================================================

/**
 * @brief Compute Shannon entropy on GPU
 *
 * Uses histogram or KDE-based estimation.
 *
 * @param ctx      GPU context
 * @param data     Input data [num_samples] (device)
 * @param entropy  Output entropy value
 * @param params   Entropy parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_stats_gpu_entropy(
    nimcp_gpu_context_t* ctx,
    const float* data,
    float* entropy,
    const stats_gpu_entropy_params_t* params);

/**
 * @brief Compute mutual information between two variables on GPU
 *
 * MI(X;Y) = H(X) + H(Y) - H(X,Y)
 *
 * @param ctx     GPU context
 * @param data_x  First variable [num_samples] (device)
 * @param data_y  Second variable [num_samples] (device)
 * @param mi      Output mutual information
 * @param params  Entropy parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_stats_gpu_mutual_information(
    nimcp_gpu_context_t* ctx,
    const float* data_x,
    const float* data_y,
    float* mi,
    const stats_gpu_entropy_params_t* params);

/**
 * @brief Compute KL divergence D_KL(P || Q) on GPU
 *
 * @param ctx           GPU context
 * @param p             Distribution P [n] (device, probabilities)
 * @param q             Distribution Q [n] (device, probabilities)
 * @param n             Number of bins
 * @param kl_divergence Output KL divergence
 * @param base          Logarithm base (2=bits, e=nats)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_stats_gpu_kl_divergence(
    nimcp_gpu_context_t* ctx,
    const float* p,
    const float* q,
    uint32_t n,
    float* kl_divergence,
    float base);

/**
 * @brief Compute Jensen-Shannon divergence on GPU
 *
 * JSD(P || Q) = 0.5 * D_KL(P || M) + 0.5 * D_KL(Q || M) where M = 0.5*(P+Q)
 *
 * @param ctx       GPU context
 * @param p         Distribution P [n] (device)
 * @param q         Distribution Q [n] (device)
 * @param n         Number of bins
 * @param js_div    Output JS divergence
 * @param base      Logarithm base
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_stats_gpu_js_divergence(
    nimcp_gpu_context_t* ctx,
    const float* p,
    const float* q,
    uint32_t n,
    float* js_div,
    float base);

//=============================================================================
// Matrix Operations for Statistics API
//=============================================================================

/**
 * @brief Compute eigendecomposition on GPU (cuSOLVER)
 *
 * For symmetric matrices, uses syevd (faster).
 * For general matrices, uses geev.
 *
 * @param ctx     GPU context
 * @param matrix  Input matrix [n x n] (device, column-major)
 * @param result  Output eigendecomposition result
 * @param params  Eigendecomposition parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_stats_gpu_eigendecomposition(
    nimcp_gpu_context_t* ctx,
    const float* matrix,
    stats_gpu_eigen_result_t* result,
    const stats_gpu_eigen_params_t* params);

/**
 * @brief Compute singular value decomposition on GPU (cuSOLVER)
 *
 * A = U * S * V^T
 *
 * @param ctx     GPU context
 * @param matrix  Input matrix [m x n] (device, column-major)
 * @param result  Output SVD result
 * @param params  SVD parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_stats_gpu_svd(
    nimcp_gpu_context_t* ctx,
    const float* matrix,
    stats_gpu_svd_result_t* result,
    const stats_gpu_svd_params_t* params);

/**
 * @brief Compute matrix inverse on GPU (cuSOLVER)
 *
 * Uses LU decomposition with pivoting.
 *
 * @param ctx       GPU context
 * @param matrix    Input matrix [n x n] (device, column-major)
 * @param inverse   Output inverse [n x n] (device, column-major)
 * @param n         Matrix dimension
 * @return true on success (false if singular)
 */
NIMCP_EXPORT bool nimcp_stats_gpu_matrix_inverse(
    nimcp_gpu_context_t* ctx,
    const float* matrix,
    float* inverse,
    uint32_t n);

/**
 * @brief Compute matrix pseudo-inverse (Moore-Penrose) on GPU
 *
 * Uses SVD: A^+ = V * S^+ * U^T
 *
 * @param ctx        GPU context
 * @param matrix     Input matrix [m x n] (device)
 * @param pinv       Output pseudo-inverse [n x m] (device)
 * @param m          Number of rows
 * @param n          Number of columns
 * @param tolerance  Singular value threshold for rank
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_stats_gpu_matrix_pinverse(
    nimcp_gpu_context_t* ctx,
    const float* matrix,
    float* pinv,
    uint32_t m,
    uint32_t n,
    float tolerance);

/**
 * @brief Solve linear system Ax = b on GPU
 *
 * @param ctx   GPU context
 * @param A     Matrix A [n x n] (device)
 * @param b     Right-hand side [n] (device)
 * @param x     Solution [n] (device)
 * @param n     System dimension
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_stats_gpu_solve_linear(
    nimcp_gpu_context_t* ctx,
    const float* A,
    const float* b,
    float* x,
    uint32_t n);

/**
 * @brief Compute Cholesky decomposition on GPU
 *
 * A = L * L^T (for symmetric positive definite matrices)
 *
 * @param ctx       GPU context
 * @param matrix    Input SPD matrix [n x n] (device)
 * @param L         Output lower triangular [n x n] (device)
 * @param n         Matrix dimension
 * @return true on success (false if not positive definite)
 */
NIMCP_EXPORT bool nimcp_stats_gpu_cholesky(
    nimcp_gpu_context_t* ctx,
    const float* matrix,
    float* L,
    uint32_t n);

//=============================================================================
// Result Cleanup
//=============================================================================

/**
 * @brief Free eigendecomposition result resources
 */
NIMCP_EXPORT void nimcp_stats_gpu_eigen_result_free(
    nimcp_gpu_context_t* ctx,
    stats_gpu_eigen_result_t* result);

/**
 * @brief Free SVD result resources
 */
NIMCP_EXPORT void nimcp_stats_gpu_svd_result_free(
    nimcp_gpu_context_t* ctx,
    stats_gpu_svd_result_t* result);

/**
 * @brief Free descriptive statistics result resources
 */
NIMCP_EXPORT void nimcp_stats_gpu_descriptive_result_free(
    nimcp_gpu_context_t* ctx,
    stats_gpu_descriptive_result_t* result);

//=============================================================================
// Statistics and Utilities
//=============================================================================

/**
 * @brief Get GPU statistics module statistics
 */
NIMCP_EXPORT int nimcp_stats_gpu_get_stats(stats_gpu_stats_t* stats);

/**
 * @brief Reset GPU statistics module statistics
 */
NIMCP_EXPORT void nimcp_stats_gpu_reset_stats(void);

/**
 * @brief Get last GPU statistics error message
 */
NIMCP_EXPORT const char* nimcp_stats_gpu_get_last_error(void);

/**
 * @brief Check if GPU statistics acceleration is available
 */
NIMCP_EXPORT bool nimcp_stats_gpu_is_available(void);

/**
 * @brief Get recommended sample count for current GPU
 *
 * @param ctx GPU context
 * @return Recommended maximum samples
 */
NIMCP_EXPORT uint32_t nimcp_stats_gpu_recommended_samples(nimcp_gpu_context_t* ctx);

/**
 * @brief Get maximum supported matrix dimension
 *
 * @param ctx GPU context
 * @return Maximum dimension for matrix operations
 */
NIMCP_EXPORT uint32_t nimcp_stats_gpu_max_matrix_dim(nimcp_gpu_context_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_STATISTICS_GPU_H */
