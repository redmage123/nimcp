//=============================================================================
// nimcp_multivariate.h - Multivariate Analysis for NIMCP Statistics
//=============================================================================
/**
 * @file nimcp_multivariate.h
 * @brief Comprehensive multivariate statistical analysis module
 *
 * WHAT: Dimensionality reduction, component analysis, and discriminant methods
 *
 * WHY:  Neural systems process high-dimensional data. Multivariate methods
 *       extract meaningful structure, reduce dimensionality, and identify
 *       latent factors underlying observed neural activity patterns.
 *
 * HOW:  Numerically stable SVD-based implementations with optional GPU
 *       acceleration via cuSOLVER for large-scale eigendecomposition.
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Dimensionality Reduction in Neural Data:
 *   +-----------------------------------------------------------------------+
 *   |  Neural recordings are inherently high-dimensional:                  |
 *   |  - EEG: 64-256 channels x time                                       |
 *   |  - fMRI: 100k+ voxels per volume                                     |
 *   |  - Spike trains: hundreds of neurons                                 |
 *   |                                                                       |
 *   |  But neural dynamics often lie on low-dimensional manifolds:         |
 *   |  - Motor cortex: ~10 latent dimensions for arm movement              |
 *   |  - Visual cortex: sparse codes for natural images                    |
 *   |  - Prefrontal cortex: task-relevant subspaces                        |
 *   |                                                                       |
 *   |  Multivariate methods reveal this structure:                         |
 *   |  - PCA: Linear projection maximizing variance                        |
 *   |  - ICA: Non-Gaussian source separation (brain signals)               |
 *   |  - Factor Analysis: Latent variable models                           |
 *   |  - LDA: Task-relevant discriminant dimensions                        |
 *   |  - CCA: Cross-modal correspondences (e.g., audio-visual)             |
 *   +-----------------------------------------------------------------------+
 *
 * FEATURES:
 * - Principal Component Analysis (PCA) with SVD-based numerical stability
 * - Independent Component Analysis (ICA) using FastICA algorithm
 * - Factor Analysis with multiple rotation methods
 * - Linear and Quadratic Discriminant Analysis (LDA/QDA)
 * - Canonical Correlation Analysis (CCA)
 * - Incremental/online variants where applicable
 * - GPU acceleration for large datasets
 *
 * PERFORMANCE:
 * - PCA (1000x100): ~50ms CPU, ~5ms GPU
 * - ICA (1000x20): ~200ms CPU (iterative)
 * - LDA fit: ~10ms CPU for typical datasets
 * - CCA: ~100ms CPU for moderate dimensions
 *
 * MEMORY:
 * - nimcp_pca_t: ~256 bytes + O(p*k) for components
 * - nimcp_ica_t: ~512 bytes + O(p*p) for mixing matrices
 * - nimcp_lda_t: ~128 bytes + O(n_classes * p)
 * - nimcp_cca_t: ~256 bytes + O(p1*k + p2*k)
 *
 * THREAD SAFETY:
 * - All context structures are single-threaded
 * - Functions are reentrant with different contexts
 * - GPU operations use stream synchronization
 *
 * @author NIMCP Development Team
 * @date 2026-01-30
 * @version 1.0.0
 */

#ifndef NIMCP_MULTIVARIATE_H
#define NIMCP_MULTIVARIATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Export macro
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default number of PCA components (0 = keep all) */
#define NIMCP_MV_DEFAULT_COMPONENTS        0

/** Maximum ICA iterations */
#define NIMCP_MV_ICA_MAX_ITERATIONS        200

/** ICA convergence tolerance */
#define NIMCP_MV_ICA_TOLERANCE             1e-5f

/** Factor analysis maximum iterations */
#define NIMCP_MV_FA_MAX_ITERATIONS         100

/** Numerical stability epsilon */
#define NIMCP_MV_EPSILON                   1e-10f

/** Minimum eigenvalue for pseudo-inverse */
#define NIMCP_MV_MIN_EIGENVALUE            1e-12f

/** Default regularization for QDA */
#define NIMCP_MV_QDA_REG_DEFAULT           1e-6f

//=============================================================================
// Result Codes
//=============================================================================

/**
 * @brief Result codes for multivariate operations
 */
typedef enum nimcp_mv_result {
    NIMCP_MV_OK              =  0,   /**< Success */
    NIMCP_MV_ERROR_NULL      = -1,   /**< NULL pointer argument */
    NIMCP_MV_ERROR_SIZE      = -2,   /**< Invalid size (n=0, p=0, etc.) */
    NIMCP_MV_ERROR_MEMORY    = -3,   /**< Memory allocation failed */
    NIMCP_MV_ERROR_PARAMS    = -4,   /**< Invalid parameters */
    NIMCP_MV_ERROR_CONVERGE  = -5,   /**< Algorithm did not converge */
    NIMCP_MV_ERROR_SINGULAR  = -6,   /**< Singular matrix encountered */
    NIMCP_MV_ERROR_NOT_FIT   = -7,   /**< Model not yet fitted */
    NIMCP_MV_ERROR_DIMS      = -8,   /**< Dimension mismatch */
    NIMCP_MV_ERROR_GPU       = -9,   /**< GPU operation failed */
    NIMCP_MV_ERROR_LAPACK    = -10,  /**< LAPACK/cuSOLVER error */
    NIMCP_MV_ERROR_LABELS    = -11   /**< Invalid class labels */
} nimcp_mv_result_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Multivariate module configuration
 */
typedef struct nimcp_mv_config {
    bool use_gpu;               /**< Use GPU acceleration if available */
    bool center_data;           /**< Center data before analysis (default: true) */
    bool scale_data;            /**< Scale to unit variance (default: false for PCA) */
    uint32_t random_seed;       /**< Random seed for stochastic algorithms */
    float tolerance;            /**< Convergence tolerance */
    uint32_t max_iterations;    /**< Maximum iterations for iterative methods */
} nimcp_mv_config_t;

/**
 * @brief Get default multivariate configuration
 */
NIMCP_EXPORT nimcp_mv_config_t nimcp_mv_default_config(void);

//=============================================================================
// PCA - Principal Component Analysis
//=============================================================================

/**
 * @brief PCA whitening mode
 */
typedef enum nimcp_pca_whiten {
    NIMCP_PCA_WHITEN_NONE,      /**< No whitening */
    NIMCP_PCA_WHITEN_UNIT,      /**< Scale to unit variance */
    NIMCP_PCA_WHITEN_ZCA        /**< ZCA whitening (rotated back) */
} nimcp_pca_whiten_t;

/**
 * @brief PCA SVD solver type
 */
typedef enum nimcp_pca_solver {
    NIMCP_PCA_SOLVER_AUTO,      /**< Auto-select based on data size */
    NIMCP_PCA_SOLVER_FULL,      /**< Full SVD */
    NIMCP_PCA_SOLVER_ARPACK,    /**< Truncated (ARPACK-style) */
    NIMCP_PCA_SOLVER_RANDOMIZED /**< Randomized SVD for very large data */
} nimcp_pca_solver_t;

/**
 * @brief PCA context structure
 */
typedef struct nimcp_pca {
    uint32_t n_features;           /**< Original feature dimension (p) */
    uint32_t n_components;         /**< Number of components (k) */
    uint32_t n_samples_seen;       /**< Number of samples fitted */

    float* components;             /**< Principal components (k x p) */
    float* explained_variance;     /**< Variance per component (k) */
    float* explained_variance_ratio; /**< Proportion of variance (k) */
    float* singular_values;        /**< Singular values (k) */
    float* mean;                   /**< Feature means for centering (p) */
    float* std;                    /**< Feature std devs if scaled (p) */
    float total_variance;          /**< Total variance in data */
    float noise_variance;          /**< Estimated noise variance */

    nimcp_pca_whiten_t whiten;     /**< Whitening mode */
    nimcp_pca_solver_t solver;     /**< SVD solver type */

    bool is_fitted;                /**< Whether model is fitted */
    void* gpu_ctx;                 /**< GPU context if using GPU */
} nimcp_pca_t;

/**
 * @brief Create PCA context
 *
 * @param n_components Number of components to keep (0 = all)
 * @param whiten Whitening mode
 * @param solver SVD solver type
 * @return Allocated PCA context (caller must destroy)
 *
 * WHAT: Initialize PCA dimensionality reduction
 * WHY:  Find orthogonal directions of maximum variance
 * HOW:  SVD of centered data matrix
 */
NIMCP_EXPORT nimcp_pca_t* nimcp_pca_create(
    uint32_t n_components,
    nimcp_pca_whiten_t whiten,
    nimcp_pca_solver_t solver
);

/**
 * @brief Destroy PCA context
 */
NIMCP_EXPORT void nimcp_pca_destroy(nimcp_pca_t* pca);

/**
 * @brief Fit PCA to data
 *
 * @param pca PCA context
 * @param X Data matrix (n_samples x n_features, row-major)
 * @param n_samples Number of samples (rows)
 * @param n_features Number of features (columns)
 * @return NIMCP_MV_OK on success
 *
 * Computes principal components via SVD:
 * X_centered = U S V^T
 * Components = V^T (top k rows)
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_pca_fit(
    nimcp_pca_t* pca,
    const float* X,
    uint32_t n_samples,
    uint32_t n_features
);

/**
 * @brief Transform data to principal component space
 *
 * @param pca Fitted PCA context
 * @param X Data matrix (n_samples x n_features)
 * @param n_samples Number of samples
 * @param X_transformed Output (n_samples x n_components)
 * @return NIMCP_MV_OK on success
 *
 * X_transformed = (X - mean) @ components.T
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_pca_transform(
    const nimcp_pca_t* pca,
    const float* X,
    uint32_t n_samples,
    float* X_transformed
);

/**
 * @brief Inverse transform from component space to original space
 *
 * @param pca Fitted PCA context
 * @param X_transformed Data in component space (n_samples x n_components)
 * @param n_samples Number of samples
 * @param X_reconstructed Output (n_samples x n_features)
 * @return NIMCP_MV_OK on success
 *
 * X_reconstructed = X_transformed @ components + mean
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_pca_inverse_transform(
    const nimcp_pca_t* pca,
    const float* X_transformed,
    uint32_t n_samples,
    float* X_reconstructed
);

/**
 * @brief Combined fit and transform
 *
 * @param pca PCA context
 * @param X Data matrix (n_samples x n_features)
 * @param n_samples Number of samples
 * @param n_features Number of features
 * @param X_transformed Output (n_samples x n_components)
 * @return NIMCP_MV_OK on success
 *
 * More efficient than separate fit + transform calls.
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_pca_fit_transform(
    nimcp_pca_t* pca,
    const float* X,
    uint32_t n_samples,
    uint32_t n_features,
    float* X_transformed
);

/**
 * @brief Get explained variance array
 *
 * @param pca Fitted PCA context
 * @param variance Output array (n_components)
 * @return NIMCP_MV_OK on success
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_pca_explained_variance(
    const nimcp_pca_t* pca,
    float* variance
);

/**
 * @brief Get principal components matrix
 *
 * @param pca Fitted PCA context
 * @param components Output matrix (n_components x n_features)
 * @return NIMCP_MV_OK on success
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_pca_get_components(
    const nimcp_pca_t* pca,
    float* components
);

/**
 * @brief Compute reconstruction error for data
 *
 * @param pca Fitted PCA context
 * @param X Original data (n_samples x n_features)
 * @param n_samples Number of samples
 * @return Mean squared reconstruction error, or NAN on error
 */
NIMCP_EXPORT float nimcp_pca_score(
    const nimcp_pca_t* pca,
    const float* X,
    uint32_t n_samples
);

/**
 * @brief Incremental PCA update with new batch
 *
 * @param pca PCA context (can be unfitted or previously fitted)
 * @param X New data batch (n_samples x n_features)
 * @param n_samples Number of new samples
 * @param n_features Number of features
 * @return NIMCP_MV_OK on success
 *
 * Uses incremental SVD update for online learning.
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_pca_partial_fit(
    nimcp_pca_t* pca,
    const float* X,
    uint32_t n_samples,
    uint32_t n_features
);

//=============================================================================
// ICA - Independent Component Analysis
//=============================================================================

/**
 * @brief ICA nonlinearity function type
 */
typedef enum nimcp_ica_fun {
    NIMCP_ICA_FUN_LOGCOSH,     /**< G(u) = log(cosh(u)) - robust to outliers */
    NIMCP_ICA_FUN_EXP,         /**< G(u) = -exp(-u^2/2) - super-Gaussian */
    NIMCP_ICA_FUN_CUBE         /**< G(u) = u^3/3 - simple, sub-Gaussian */
} nimcp_ica_fun_t;

/**
 * @brief ICA algorithm variant
 */
typedef enum nimcp_ica_algorithm {
    NIMCP_ICA_PARALLEL,        /**< Parallel FastICA (symmetric) */
    NIMCP_ICA_DEFLATION        /**< Deflation FastICA (sequential) */
} nimcp_ica_algorithm_t;

/**
 * @brief ICA context structure
 */
typedef struct nimcp_ica {
    uint32_t n_components;         /**< Number of independent components */
    uint32_t n_features;           /**< Original feature dimension */
    uint32_t n_iter;               /**< Iterations used in fitting */

    float* mixing;                 /**< Mixing matrix A (n_features x n_components) */
    float* unmixing;               /**< Unmixing matrix W (n_components x n_features) */
    float* components;             /**< IC x features (after whitening) */
    float* mean;                   /**< Feature means for centering */
    float* whitening;              /**< Whitening matrix */

    nimcp_ica_fun_t fun;           /**< Nonlinearity function */
    nimcp_ica_algorithm_t algorithm; /**< Algorithm variant */
    float tolerance;               /**< Convergence tolerance */
    uint32_t max_iter;             /**< Maximum iterations */
    uint32_t random_state;         /**< Random state for initialization */

    bool is_fitted;                /**< Whether model is fitted */
    void* gpu_ctx;                 /**< GPU context if using GPU */
} nimcp_ica_t;

/**
 * @brief Create ICA context
 *
 * @param n_components Number of independent components to extract
 * @param fun Nonlinearity function for FastICA
 * @param algorithm Parallel or deflation
 * @return Allocated ICA context (caller must destroy)
 *
 * WHAT: Initialize independent component analysis
 * WHY:  Separate mixed signals into statistically independent sources
 * HOW:  FastICA algorithm with fixed-point iteration
 */
NIMCP_EXPORT nimcp_ica_t* nimcp_ica_create(
    uint32_t n_components,
    nimcp_ica_fun_t fun,
    nimcp_ica_algorithm_t algorithm
);

/**
 * @brief Destroy ICA context
 */
NIMCP_EXPORT void nimcp_ica_destroy(nimcp_ica_t* ica);

/**
 * @brief Fit ICA to data
 *
 * @param ica ICA context
 * @param X Data matrix (n_samples x n_features)
 * @param n_samples Number of samples
 * @param n_features Number of features
 * @return NIMCP_MV_OK on success
 *
 * Assumes X = A @ S where S are independent sources.
 * Finds W such that S_hat = W @ X are maximally independent.
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_ica_fit(
    nimcp_ica_t* ica,
    const float* X,
    uint32_t n_samples,
    uint32_t n_features
);

/**
 * @brief Transform data to independent components
 *
 * @param ica Fitted ICA context
 * @param X Data matrix (n_samples x n_features)
 * @param n_samples Number of samples
 * @param S Output independent components (n_samples x n_components)
 * @return NIMCP_MV_OK on success
 *
 * S = (X - mean) @ W.T
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_ica_transform(
    const nimcp_ica_t* ica,
    const float* X,
    uint32_t n_samples,
    float* S
);

/**
 * @brief Get mixing matrix A
 *
 * @param ica Fitted ICA context
 * @param A Output mixing matrix (n_features x n_components)
 * @return NIMCP_MV_OK on success
 *
 * X = A @ S + mean, where S are the sources.
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_ica_get_mixing_matrix(
    const nimcp_ica_t* ica,
    float* A
);

/**
 * @brief Get unmixing matrix W
 *
 * @param ica Fitted ICA context
 * @param W Output unmixing matrix (n_components x n_features)
 * @return NIMCP_MV_OK on success
 *
 * S = W @ (X - mean).T
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_ica_get_unmixing_matrix(
    const nimcp_ica_t* ica,
    float* W
);

/**
 * @brief Combined fit and transform
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_ica_fit_transform(
    nimcp_ica_t* ica,
    const float* X,
    uint32_t n_samples,
    uint32_t n_features,
    float* S
);

//=============================================================================
// Factor Analysis
//=============================================================================

/**
 * @brief Factor rotation method
 */
typedef enum nimcp_factor_rotation {
    NIMCP_FACTOR_ROTATION_NONE,      /**< No rotation */
    NIMCP_FACTOR_ROTATION_VARIMAX,   /**< Orthogonal varimax */
    NIMCP_FACTOR_ROTATION_PROMAX,    /**< Oblique promax */
    NIMCP_FACTOR_ROTATION_QUARTIMAX, /**< Orthogonal quartimax */
    NIMCP_FACTOR_ROTATION_EQUAMAX    /**< Orthogonal equamax */
} nimcp_factor_rotation_t;

/**
 * @brief Factor Analysis context structure
 */
typedef struct nimcp_factor {
    uint32_t n_factors;             /**< Number of factors */
    uint32_t n_features;            /**< Original feature dimension */
    uint32_t n_iter;                /**< Iterations used in fitting */

    float* loadings;                /**< Factor loadings (n_features x n_factors) */
    float* communalities;           /**< Communalities (n_features) */
    float* uniquenesses;            /**< Uniquenesses = 1 - communalities */
    float* factor_variance;         /**< Variance explained per factor */
    float* mean;                    /**< Feature means */
    float* rotation_matrix;         /**< Rotation matrix if rotated */
    float* factor_correlation;      /**< Factor correlations (for oblique) */

    nimcp_factor_rotation_t rotation; /**< Rotation method */
    float tolerance;                  /**< Convergence tolerance */
    uint32_t max_iter;                /**< Maximum EM iterations */

    bool is_fitted;                 /**< Whether model is fitted */
    void* gpu_ctx;                  /**< GPU context */
} nimcp_factor_t;

/**
 * @brief Create Factor Analysis context
 *
 * @param n_factors Number of latent factors
 * @param rotation Rotation method for loadings
 * @return Allocated factor context
 *
 * WHAT: Initialize exploratory factor analysis
 * WHY:  Model observed variables as linear combinations of latent factors
 * HOW:  Maximum likelihood estimation via EM algorithm
 */
NIMCP_EXPORT nimcp_factor_t* nimcp_factor_create(
    uint32_t n_factors,
    nimcp_factor_rotation_t rotation
);

/**
 * @brief Destroy Factor Analysis context
 */
NIMCP_EXPORT void nimcp_factor_destroy(nimcp_factor_t* fa);

/**
 * @brief Fit Factor Analysis model
 *
 * @param fa Factor context
 * @param X Data matrix (n_samples x n_features)
 * @param n_samples Number of samples
 * @param n_features Number of features
 * @return NIMCP_MV_OK on success
 *
 * Model: X = mu + L @ F + epsilon
 * where L are loadings, F are factors, epsilon is noise.
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_factor_fit(
    nimcp_factor_t* fa,
    const float* X,
    uint32_t n_samples,
    uint32_t n_features
);

/**
 * @brief Get factor loadings matrix
 *
 * @param fa Fitted factor context
 * @param loadings Output (n_features x n_factors)
 * @return NIMCP_MV_OK on success
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_factor_get_loadings(
    const nimcp_factor_t* fa,
    float* loadings
);

/**
 * @brief Get communalities (variance explained by factors)
 *
 * @param fa Fitted factor context
 * @param communalities Output (n_features)
 * @return NIMCP_MV_OK on success
 *
 * communality[j] = sum(loadings[j,:]^2)
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_factor_get_communalities(
    const nimcp_factor_t* fa,
    float* communalities
);

/**
 * @brief Compute factor scores for data
 *
 * @param fa Fitted factor context
 * @param X Data matrix (n_samples x n_features)
 * @param n_samples Number of samples
 * @param scores Output factor scores (n_samples x n_factors)
 * @return NIMCP_MV_OK on success
 *
 * Uses regression method (Thurstone's) to estimate scores.
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_factor_scores(
    const nimcp_factor_t* fa,
    const float* X,
    uint32_t n_samples,
    float* scores
);

/**
 * @brief Apply rotation to loadings
 *
 * @param fa Factor context (already fitted)
 * @param rotation Rotation method
 * @return NIMCP_MV_OK on success
 *
 * Can be called after fit to apply different rotations.
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_factor_rotate(
    nimcp_factor_t* fa,
    nimcp_factor_rotation_t rotation
);

//=============================================================================
// LDA - Linear Discriminant Analysis
//=============================================================================

/**
 * @brief LDA solver type
 */
typedef enum nimcp_lda_solver {
    NIMCP_LDA_SOLVER_SVD,       /**< SVD-based (recommended) */
    NIMCP_LDA_SOLVER_LSQR,      /**< Least squares */
    NIMCP_LDA_SOLVER_EIGEN      /**< Eigenvalue decomposition */
} nimcp_lda_solver_t;

/**
 * @brief Linear Discriminant Analysis context
 */
typedef struct nimcp_lda {
    uint32_t n_features;            /**< Number of features */
    uint32_t n_classes;             /**< Number of classes */
    uint32_t n_components;          /**< Discriminant dimensions (min(n_classes-1, n_features)) */

    float* means;                   /**< Class means (n_classes x n_features) */
    float* priors;                  /**< Class prior probabilities (n_classes) */
    float* coef;                    /**< Coefficients (n_classes x n_features) */
    float* intercept;               /**< Intercepts (n_classes) */
    float* scalings;                /**< Discriminant directions (n_components x n_features) */
    float* explained_variance_ratio; /**< Variance ratio per component */
    float* xbar;                    /**< Global mean (n_features) */

    nimcp_lda_solver_t solver;      /**< Solver type */
    float shrinkage;                /**< Shrinkage parameter (0 = none, 1 = full) */

    bool is_fitted;                 /**< Whether model is fitted */
    void* gpu_ctx;                  /**< GPU context */
} nimcp_lda_t;

/**
 * @brief Create LDA context
 *
 * @param solver Solver type
 * @param n_components Number of discriminant dimensions (0 = auto)
 * @return Allocated LDA context
 *
 * WHAT: Initialize linear discriminant analysis
 * WHY:  Find projection maximizing class separation
 * HOW:  Generalized eigenvalue problem: S_W^-1 @ S_B
 */
NIMCP_EXPORT nimcp_lda_t* nimcp_lda_create(
    nimcp_lda_solver_t solver,
    uint32_t n_components
);

/**
 * @brief Destroy LDA context
 */
NIMCP_EXPORT void nimcp_lda_destroy(nimcp_lda_t* lda);

/**
 * @brief Fit LDA to labeled data
 *
 * @param lda LDA context
 * @param X Data matrix (n_samples x n_features)
 * @param y Class labels (n_samples) - integers from 0 to n_classes-1
 * @param n_samples Number of samples
 * @param n_features Number of features
 * @return NIMCP_MV_OK on success
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_lda_fit(
    nimcp_lda_t* lda,
    const float* X,
    const uint32_t* y,
    uint32_t n_samples,
    uint32_t n_features
);

/**
 * @brief Transform data to discriminant space
 *
 * @param lda Fitted LDA context
 * @param X Data matrix (n_samples x n_features)
 * @param n_samples Number of samples
 * @param X_transformed Output (n_samples x n_components)
 * @return NIMCP_MV_OK on success
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_lda_transform(
    const nimcp_lda_t* lda,
    const float* X,
    uint32_t n_samples,
    float* X_transformed
);

/**
 * @brief Predict class labels for new data
 *
 * @param lda Fitted LDA context
 * @param X Data matrix (n_samples x n_features)
 * @param n_samples Number of samples
 * @param y_pred Predicted labels (n_samples)
 * @return NIMCP_MV_OK on success
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_lda_predict(
    const nimcp_lda_t* lda,
    const float* X,
    uint32_t n_samples,
    uint32_t* y_pred
);

/**
 * @brief Predict class probabilities
 *
 * @param lda Fitted LDA context
 * @param X Data matrix (n_samples x n_features)
 * @param n_samples Number of samples
 * @param proba Output probabilities (n_samples x n_classes)
 * @return NIMCP_MV_OK on success
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_lda_predict_proba(
    const nimcp_lda_t* lda,
    const float* X,
    uint32_t n_samples,
    float* proba
);

/**
 * @brief Compute log-likelihood for data
 *
 * @param lda Fitted LDA context
 * @param X Data matrix (n_samples x n_features)
 * @param n_samples Number of samples
 * @param log_likelihood Output (n_samples x n_classes)
 * @return NIMCP_MV_OK on success
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_lda_decision_function(
    const nimcp_lda_t* lda,
    const float* X,
    uint32_t n_samples,
    float* log_likelihood
);

//=============================================================================
// QDA - Quadratic Discriminant Analysis
//=============================================================================

/**
 * @brief Quadratic Discriminant Analysis context
 */
typedef struct nimcp_qda {
    uint32_t n_features;            /**< Number of features */
    uint32_t n_classes;             /**< Number of classes */

    float* means;                   /**< Class means (n_classes x n_features) */
    float* priors;                  /**< Class prior probabilities */
    float** covariances;            /**< Class covariances (n_classes arrays of n_features x n_features) */
    float** covariance_inv;         /**< Inverse covariances */
    float* log_det;                 /**< Log determinants of covariances */

    float reg_param;                /**< Regularization parameter */

    bool is_fitted;                 /**< Whether model is fitted */
    void* gpu_ctx;                  /**< GPU context */
} nimcp_qda_t;

/**
 * @brief Create QDA context
 *
 * @param reg_param Regularization (0 = none)
 * @return Allocated QDA context
 *
 * WHAT: Initialize quadratic discriminant analysis
 * WHY:  Allow class-specific covariance (quadratic decision boundary)
 * HOW:  Fit Gaussian to each class, predict by maximum posterior
 */
NIMCP_EXPORT nimcp_qda_t* nimcp_qda_create(float reg_param);

/**
 * @brief Destroy QDA context
 */
NIMCP_EXPORT void nimcp_qda_destroy(nimcp_qda_t* qda);

/**
 * @brief Fit QDA to labeled data
 *
 * @param qda QDA context
 * @param X Data matrix (n_samples x n_features)
 * @param y Class labels (n_samples)
 * @param n_samples Number of samples
 * @param n_features Number of features
 * @return NIMCP_MV_OK on success
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_qda_fit(
    nimcp_qda_t* qda,
    const float* X,
    const uint32_t* y,
    uint32_t n_samples,
    uint32_t n_features
);

/**
 * @brief Predict class labels using QDA
 *
 * @param qda Fitted QDA context
 * @param X Data matrix (n_samples x n_features)
 * @param n_samples Number of samples
 * @param y_pred Predicted labels (n_samples)
 * @return NIMCP_MV_OK on success
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_qda_predict(
    const nimcp_qda_t* qda,
    const float* X,
    uint32_t n_samples,
    uint32_t* y_pred
);

/**
 * @brief Predict class probabilities using QDA
 *
 * @param qda Fitted QDA context
 * @param X Data matrix (n_samples x n_features)
 * @param n_samples Number of samples
 * @param proba Output probabilities (n_samples x n_classes)
 * @return NIMCP_MV_OK on success
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_qda_predict_proba(
    const nimcp_qda_t* qda,
    const float* X,
    uint32_t n_samples,
    float* proba
);

//=============================================================================
// CCA - Canonical Correlation Analysis
//=============================================================================

/**
 * @brief Canonical Correlation Analysis context
 */
typedef struct nimcp_cca {
    uint32_t n_features_x;          /**< Features in first dataset */
    uint32_t n_features_y;          /**< Features in second dataset */
    uint32_t n_components;          /**< Number of canonical components */

    float* x_weights;               /**< X weights (n_features_x x n_components) */
    float* y_weights;               /**< Y weights (n_features_y x n_components) */
    float* x_loadings;              /**< X loadings (correlation of X with X_c) */
    float* y_loadings;              /**< Y loadings */
    float* correlations;            /**< Canonical correlations (n_components) */
    float* x_mean;                  /**< X feature means */
    float* y_mean;                  /**< Y feature means */
    float* x_std;                   /**< X feature stds (if scaled) */
    float* y_std;                   /**< Y feature stds (if scaled) */

    bool scale;                     /**< Whether to scale features */
    float tolerance;                /**< Numerical tolerance */

    bool is_fitted;                 /**< Whether model is fitted */
    void* gpu_ctx;                  /**< GPU context */
} nimcp_cca_t;

/**
 * @brief Create CCA context
 *
 * @param n_components Number of canonical components
 * @param scale Whether to scale features to unit variance
 * @return Allocated CCA context
 *
 * WHAT: Initialize canonical correlation analysis
 * WHY:  Find projections of two datasets that are maximally correlated
 * HOW:  Generalized eigenvalue problem on cross-covariance matrices
 */
NIMCP_EXPORT nimcp_cca_t* nimcp_cca_create(
    uint32_t n_components,
    bool scale
);

/**
 * @brief Destroy CCA context
 */
NIMCP_EXPORT void nimcp_cca_destroy(nimcp_cca_t* cca);

/**
 * @brief Fit CCA to two datasets
 *
 * @param cca CCA context
 * @param X First dataset (n_samples x n_features_x)
 * @param Y Second dataset (n_samples x n_features_y)
 * @param n_samples Number of samples (must match for both)
 * @param n_features_x Features in X
 * @param n_features_y Features in Y
 * @return NIMCP_MV_OK on success
 *
 * Finds weight vectors a_k, b_k maximizing:
 * corr(X @ a_k, Y @ b_k) subject to orthogonality
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_cca_fit(
    nimcp_cca_t* cca,
    const float* X,
    const float* Y,
    uint32_t n_samples,
    uint32_t n_features_x,
    uint32_t n_features_y
);

/**
 * @brief Transform both datasets to canonical variate space
 *
 * @param cca Fitted CCA context
 * @param X First dataset (n_samples x n_features_x)
 * @param Y Second dataset (n_samples x n_features_y)
 * @param n_samples Number of samples
 * @param X_c Output X canonical variates (n_samples x n_components)
 * @param Y_c Output Y canonical variates (n_samples x n_components)
 * @return NIMCP_MV_OK on success
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_cca_transform(
    const nimcp_cca_t* cca,
    const float* X,
    const float* Y,
    uint32_t n_samples,
    float* X_c,
    float* Y_c
);

/**
 * @brief Get canonical correlations
 *
 * @param cca Fitted CCA context
 * @param correlations Output (n_components)
 * @return NIMCP_MV_OK on success
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_cca_correlations(
    const nimcp_cca_t* cca,
    float* correlations
);

/**
 * @brief Combined fit and transform
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_cca_fit_transform(
    nimcp_cca_t* cca,
    const float* X,
    const float* Y,
    uint32_t n_samples,
    uint32_t n_features_x,
    uint32_t n_features_y,
    float* X_c,
    float* Y_c
);

/**
 * @brief Compute cross-loadings (correlation of variables with opposite variates)
 *
 * @param cca Fitted CCA context
 * @param X First dataset
 * @param Y Second dataset
 * @param n_samples Number of samples
 * @param x_cross_loadings X correlations with Y variates (n_features_x x n_components)
 * @param y_cross_loadings Y correlations with X variates (n_features_y x n_components)
 * @return NIMCP_MV_OK on success
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_cca_cross_loadings(
    const nimcp_cca_t* cca,
    const float* X,
    const float* Y,
    uint32_t n_samples,
    float* x_cross_loadings,
    float* y_cross_loadings
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Center data (subtract mean)
 *
 * @param X Input data (n_samples x n_features)
 * @param n_samples Number of samples
 * @param n_features Number of features
 * @param X_centered Output centered data
 * @param mean Output mean (n_features, can be NULL if not needed)
 * @return NIMCP_MV_OK on success
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_mv_center(
    const float* X,
    uint32_t n_samples,
    uint32_t n_features,
    float* X_centered,
    float* mean
);

/**
 * @brief Standardize data (center and scale to unit variance)
 *
 * @param X Input data
 * @param n_samples Number of samples
 * @param n_features Number of features
 * @param X_standardized Output standardized data
 * @param mean Output mean (can be NULL)
 * @param std Output std (can be NULL)
 * @return NIMCP_MV_OK on success
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_mv_standardize(
    const float* X,
    uint32_t n_samples,
    uint32_t n_features,
    float* X_standardized,
    float* mean,
    float* std
);

/**
 * @brief Compute covariance matrix
 *
 * @param X Centered data (n_samples x n_features)
 * @param n_samples Number of samples
 * @param n_features Number of features
 * @param cov Output covariance matrix (n_features x n_features)
 * @param ddof Delta degrees of freedom (0 = population, 1 = sample)
 * @return NIMCP_MV_OK on success
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_mv_covariance(
    const float* X,
    uint32_t n_samples,
    uint32_t n_features,
    float* cov,
    uint32_t ddof
);

/**
 * @brief Compute SVD of matrix
 *
 * @param A Input matrix (m x n, row-major)
 * @param m Number of rows
 * @param n Number of columns
 * @param U Left singular vectors (m x min(m,n))
 * @param S Singular values (min(m,n))
 * @param Vt Right singular vectors transposed (min(m,n) x n)
 * @param full_matrices If true, U is m x m, Vt is n x n
 * @return NIMCP_MV_OK on success
 *
 * Uses LAPACK sgesdd for divide-and-conquer SVD.
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_mv_svd(
    const float* A,
    uint32_t m,
    uint32_t n,
    float* U,
    float* S,
    float* Vt,
    bool full_matrices
);

/**
 * @brief Compute eigendecomposition of symmetric matrix
 *
 * @param A Symmetric matrix (n x n)
 * @param n Matrix dimension
 * @param eigenvalues Output eigenvalues (n) in ascending order
 * @param eigenvectors Output eigenvectors (n x n, column-major)
 * @return NIMCP_MV_OK on success
 *
 * Uses LAPACK ssyevd for divide-and-conquer eigen.
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_mv_eigh(
    const float* A,
    uint32_t n,
    float* eigenvalues,
    float* eigenvectors
);

/**
 * @brief Compute pseudo-inverse of matrix
 *
 * @param A Input matrix (m x n)
 * @param m Number of rows
 * @param n Number of columns
 * @param A_pinv Output pseudo-inverse (n x m)
 * @param rcond Cutoff for small singular values
 * @return NIMCP_MV_OK on success
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_mv_pinv(
    const float* A,
    uint32_t m,
    uint32_t n,
    float* A_pinv,
    float rcond
);

/**
 * @brief Matrix multiplication C = alpha * A @ B + beta * C
 *
 * @param A Matrix A (m x k)
 * @param B Matrix B (k x n)
 * @param C Output matrix (m x n)
 * @param m Rows of A and C
 * @param k Inner dimension
 * @param n Columns of B and C
 * @param alpha Scalar multiplier
 * @param beta Scalar for C
 * @param trans_a Transpose A
 * @param trans_b Transpose B
 * @return NIMCP_MV_OK on success
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_mv_gemm(
    const float* A,
    const float* B,
    float* C,
    uint32_t m,
    uint32_t k,
    uint32_t n,
    float alpha,
    float beta,
    bool trans_a,
    bool trans_b
);

/**
 * @brief Solve linear system A @ X = B
 *
 * @param A Coefficient matrix (n x n)
 * @param B Right-hand side (n x nrhs)
 * @param n System dimension
 * @param nrhs Number of right-hand sides
 * @param X Output solution (n x nrhs)
 * @return NIMCP_MV_OK on success
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_mv_solve(
    const float* A,
    const float* B,
    uint32_t n,
    uint32_t nrhs,
    float* X
);

//=============================================================================
// Error Handling
//=============================================================================

/**
 * @brief Get error message for result code
 */
NIMCP_EXPORT const char* nimcp_mv_error_string(nimcp_mv_result_t result);

//=============================================================================
// GPU Acceleration Interface
//=============================================================================

#ifdef NIMCP_ENABLE_CUDA

/**
 * @brief GPU-accelerated PCA fit
 *
 * Uses cuSOLVER for SVD computation on GPU.
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_pca_fit_gpu(
    nimcp_pca_t* pca,
    const float* X,
    uint32_t n_samples,
    uint32_t n_features,
    void* gpu_ctx
);

/**
 * @brief GPU-accelerated PCA transform
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_pca_transform_gpu(
    const nimcp_pca_t* pca,
    const float* X,
    uint32_t n_samples,
    float* X_transformed,
    void* gpu_ctx
);

/**
 * @brief GPU-accelerated ICA fit
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_ica_fit_gpu(
    nimcp_ica_t* ica,
    const float* X,
    uint32_t n_samples,
    uint32_t n_features,
    void* gpu_ctx
);

/**
 * @brief GPU-accelerated LDA fit
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_lda_fit_gpu(
    nimcp_lda_t* lda,
    const float* X,
    const uint32_t* y,
    uint32_t n_samples,
    uint32_t n_features,
    void* gpu_ctx
);

/**
 * @brief GPU-accelerated covariance matrix computation
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_mv_covariance_gpu(
    const float* X,
    uint32_t n_samples,
    uint32_t n_features,
    float* cov,
    uint32_t ddof,
    void* gpu_ctx
);

/**
 * @brief GPU-accelerated SVD
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_mv_svd_gpu(
    const float* A,
    uint32_t m,
    uint32_t n,
    float* U,
    float* S,
    float* Vt,
    bool full_matrices,
    void* gpu_ctx
);

/**
 * @brief GPU-accelerated eigendecomposition
 */
NIMCP_EXPORT nimcp_mv_result_t nimcp_mv_eigh_gpu(
    const float* A,
    uint32_t n,
    float* eigenvalues,
    float* eigenvectors,
    void* gpu_ctx
);

#endif /* NIMCP_ENABLE_CUDA */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MULTIVARIATE_H */
