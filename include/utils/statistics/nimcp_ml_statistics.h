//=============================================================================
// nimcp_ml_statistics.h - Machine Learning Statistics for NIMCP
//=============================================================================
/**
 * @file nimcp_ml_statistics.h
 * @brief Machine Learning statistical models: GMM, GP, HMM, KDE, Naive Bayes
 *
 * WHAT: Advanced ML statistical methods for density estimation, regression,
 *       sequence modeling, and probabilistic classification
 *
 * WHY:  Neural systems use probabilistic models for perception, learning,
 *       and decision-making. GMMs model population heterogeneity, GPs provide
 *       uncertainty-aware predictions, HMMs capture temporal dynamics, KDEs
 *       estimate neural response distributions, and Naive Bayes enables fast
 *       probabilistic classification.
 *
 * HOW:  C99 implementation with GPU acceleration for compute-intensive
 *       operations (EM algorithm, GP kernel matrices), numerical stability
 *       through log-space computations, and full immune system integration.
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Machine Learning in Neural Computation:
 *   +-----------------------------------------------------------------------+
 *   |                                                                       |
 *   |  Gaussian Mixture Models (GMM):                                       |
 *   |    - Model heterogeneous neural populations                           |
 *   |    - Cluster spike waveforms for cell sorting                         |
 *   |    - Represent multimodal probability distributions                   |
 *   |                                                                       |
 *   |  Gaussian Processes (GP):                                             |
 *   |    - Non-parametric regression with uncertainty                       |
 *   |    - Model smooth neural tuning curves                                |
 *   |    - Bayesian optimization for parameter tuning                       |
 *   |                                                                       |
 *   |  Hidden Markov Models (HMM):                                          |
 *   |    - Model discrete brain states and transitions                      |
 *   |    - Decode motor intentions from neural signals                      |
 *   |    - Segment continuous neural data into episodes                     |
 *   |                                                                       |
 *   |  Kernel Density Estimation (KDE):                                     |
 *   |    - Non-parametric probability density estimation                    |
 *   |    - Model spike rate distributions                                   |
 *   |    - Smooth histograms of neural responses                            |
 *   |                                                                       |
 *   |  Naive Bayes:                                                         |
 *   |    - Fast probabilistic classification                                |
 *   |    - Decode stimulus categories from population activity              |
 *   |    - Real-time intent classification                                  |
 *   +-----------------------------------------------------------------------+
 *
 * FEATURES:
 * - GMM: EM fitting, soft/hard clustering, model selection (BIC/AIC)
 * - GP:  Multiple kernels, hyperparameter optimization, uncertainty bounds
 * - HMM: Baum-Welch training, Viterbi decoding, forward-backward algorithm
 * - KDE: Multiple bandwidth selection methods, multivariate support
 * - NB:  Gaussian, Multinomial, and Bernoulli variants
 * - Model evaluation: cross-validation, confusion matrix, ROC/AUC
 * - GPU acceleration for EM, GP kernel computation, large-scale operations
 *
 * PERFORMANCE (GPU-accelerated):
 * - GMM fit (N=10000, K=5, D=10): ~10ms per EM iteration
 * - GP predict (N=1000 train, M=100 test): ~5ms
 * - HMM decode (T=1000, S=10): ~2ms
 * - KDE evaluate (N=10000, M=100): ~1ms
 *
 * THREAD SAFETY:
 * - Model structures are not thread-safe (use separate instances)
 * - Fitting operations acquire internal locks
 * - Prediction is thread-safe on fitted models
 *
 * @author NIMCP Development Team
 * @date 2026-01-30
 * @version 1.0.0
 */

#ifndef NIMCP_ML_STATISTICS_H
#define NIMCP_ML_STATISTICS_H

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

/** Maximum number of GMM components */
#define NIMCP_GMM_MAX_COMPONENTS 100

/** Maximum number of HMM states */
#define NIMCP_HMM_MAX_STATES 256

/** Maximum feature dimensions for models */
#define NIMCP_ML_MAX_FEATURES 1024

/** Default EM convergence tolerance */
#define NIMCP_EM_TOLERANCE 1e-6f

/** Default maximum EM iterations */
#define NIMCP_EM_MAX_ITER 100

/** Default number of cross-validation folds */
#define NIMCP_CV_DEFAULT_FOLDS 5

/** Small value for numerical stability */
#define NIMCP_ML_EPS 1e-10f

/** Log of small value */
#define NIMCP_ML_LOG_EPS (-23.025850929940457f)

//=============================================================================
// Error Codes
//=============================================================================

/**
 * @brief ML Statistics error codes
 */
typedef enum nimcp_ml_error {
    NIMCP_ML_OK = 0,                    /**< Success */
    NIMCP_ML_ERROR_NULL = -1,           /**< NULL pointer argument */
    NIMCP_ML_ERROR_MEMORY = -2,         /**< Memory allocation failed */
    NIMCP_ML_ERROR_PARAMS = -3,         /**< Invalid parameters */
    NIMCP_ML_ERROR_NOT_FITTED = -4,     /**< Model not fitted */
    NIMCP_ML_ERROR_CONVERGE = -5,       /**< Algorithm did not converge */
    NIMCP_ML_ERROR_SINGULAR = -6,       /**< Singular matrix */
    NIMCP_ML_ERROR_DIMENSION = -7,      /**< Dimension mismatch */
    NIMCP_ML_ERROR_RANGE = -8,          /**< Value out of range */
    NIMCP_ML_ERROR_GPU = -9,            /**< GPU operation failed */
    NIMCP_ML_ERROR_NOT_INIT = -10       /**< Module not initialized */
} nimcp_ml_error_t;

//=============================================================================
// Forward Declarations
//=============================================================================

struct nimcp_gpu_context_s;
typedef struct nimcp_gpu_context_s nimcp_gpu_context_t;

//=============================================================================
// GMM (Gaussian Mixture Model) Types
//=============================================================================

/**
 * @brief Covariance type for GMM
 */
typedef enum nimcp_gmm_cov_type {
    NIMCP_GMM_COV_FULL,        /**< Full covariance matrix per component */
    NIMCP_GMM_COV_DIAGONAL,    /**< Diagonal covariance per component */
    NIMCP_GMM_COV_SPHERICAL,   /**< Single variance per component */
    NIMCP_GMM_COV_TIED         /**< Shared covariance across components */
} nimcp_gmm_cov_type_t;

/**
 * @brief GMM initialization method
 */
typedef enum nimcp_gmm_init {
    NIMCP_GMM_INIT_KMEANS,     /**< Initialize with k-means clustering */
    NIMCP_GMM_INIT_RANDOM,     /**< Random initialization */
    NIMCP_GMM_INIT_KMEANSPP    /**< K-means++ initialization */
} nimcp_gmm_init_t;

/**
 * @brief GMM configuration
 */
typedef struct nimcp_gmm_config {
    uint32_t n_components;        /**< Number of mixture components */
    nimcp_gmm_cov_type_t cov_type;/**< Covariance matrix type */
    nimcp_gmm_init_t init_method; /**< Initialization method */
    float tol;                    /**< Convergence tolerance */
    uint32_t max_iter;            /**< Maximum EM iterations */
    uint32_t n_init;              /**< Number of initializations */
    float reg_covar;              /**< Regularization for covariance */
    uint32_t random_seed;         /**< Random seed (0 = random) */
    bool use_gpu;                 /**< Use GPU acceleration */
    bool warm_start;              /**< Use previous solution as init */
} nimcp_gmm_config_t;

/**
 * @brief GMM model structure
 */
typedef struct nimcp_gmm {
    uint32_t magic;              /**< Magic number for validation */
    uint32_t n_components;       /**< Number of components */
    uint32_t n_features;         /**< Number of features */
    nimcp_gmm_cov_type_t cov_type; /**< Covariance type */

    /* Fitted parameters */
    float* weights;              /**< Component weights [n_components] */
    float* means;                /**< Component means [n_components x n_features] */
    float* covariances;          /**< Covariances (shape depends on cov_type) */
    float* precisions;           /**< Precision matrices (inverse covariances) */
    float* precisions_chol;      /**< Cholesky of precisions */
    float* log_det;              /**< Log determinants [n_components] */

    /* Fitting state */
    bool is_fitted;              /**< Model has been fitted */
    float lower_bound;           /**< Final log-likelihood lower bound */
    uint32_t n_iter;             /**< Number of iterations performed */
    bool converged;              /**< Whether EM converged */

    /* GPU resources */
    void* gpu_data;              /**< GPU-side data (opaque) */
    bool use_gpu;                /**< GPU acceleration enabled */
    nimcp_gpu_context_t* gpu_ctx;/**< GPU context */
} nimcp_gmm_t;

/**
 * @brief GMM fit result
 */
typedef struct nimcp_gmm_fit_result {
    bool converged;              /**< Whether EM converged */
    uint32_t n_iter;             /**< Iterations performed */
    float log_likelihood;        /**< Final log-likelihood */
    float bic;                   /**< Bayesian Information Criterion */
    float aic;                   /**< Akaike Information Criterion */
} nimcp_gmm_fit_result_t;

//=============================================================================
// Gaussian Process Types
//=============================================================================

/**
 * @brief GP kernel types
 */
typedef enum nimcp_gp_kernel {
    NIMCP_GP_KERNEL_RBF,          /**< Radial Basis Function (squared exp) */
    NIMCP_GP_KERNEL_MATERN_12,    /**< Matern 1/2 (exponential) */
    NIMCP_GP_KERNEL_MATERN_32,    /**< Matern 3/2 */
    NIMCP_GP_KERNEL_MATERN_52,    /**< Matern 5/2 */
    NIMCP_GP_KERNEL_PERIODIC,     /**< Periodic kernel */
    NIMCP_GP_KERNEL_RATIONAL_QUAD,/**< Rational quadratic */
    NIMCP_GP_KERNEL_LINEAR,       /**< Linear kernel */
    NIMCP_GP_KERNEL_POLYNOMIAL,   /**< Polynomial kernel */
    NIMCP_GP_KERNEL_WHITE,        /**< White noise kernel */
    NIMCP_GP_KERNEL_COMPOSITE     /**< Combination of kernels */
} nimcp_gp_kernel_t;

/**
 * @brief GP kernel parameters
 */
typedef struct nimcp_gp_kernel_params {
    nimcp_gp_kernel_t type;       /**< Kernel type */
    float length_scale;           /**< Length scale parameter */
    float variance;               /**< Kernel variance (amplitude^2) */
    float period;                 /**< Period for periodic kernel */
    float alpha;                  /**< Alpha for rational quadratic */
    uint32_t degree;              /**< Degree for polynomial kernel */
    float* length_scales;         /**< Per-dimension length scales (ARD) */
    bool use_ard;                 /**< Use Automatic Relevance Determination */
} nimcp_gp_kernel_params_t;

/**
 * @brief GP configuration
 */
typedef struct nimcp_gp_config {
    nimcp_gp_kernel_params_t kernel; /**< Kernel parameters */
    float noise_variance;         /**< Observation noise variance */
    bool normalize_y;             /**< Normalize target values */
    bool optimize_hyperparams;    /**< Optimize hyperparameters */
    uint32_t optimizer_max_iter;  /**< Optimizer iterations */
    float optimizer_tol;          /**< Optimizer tolerance */
    uint32_t random_seed;         /**< Random seed */
    bool use_gpu;                 /**< GPU acceleration */
    float jitter;                 /**< Jitter for numerical stability */
} nimcp_gp_config_t;

/**
 * @brief Gaussian Process model
 */
typedef struct nimcp_gp {
    uint32_t magic;              /**< Magic number for validation */
    uint32_t n_train;            /**< Number of training points */
    uint32_t n_features;         /**< Number of input features */

    /* Kernel configuration */
    nimcp_gp_kernel_params_t kernel; /**< Kernel parameters */
    float noise_variance;        /**< Noise variance */

    /* Training data (stored for prediction) */
    float* X_train;              /**< Training inputs [n_train x n_features] */
    float* y_train;              /**< Training targets [n_train] */
    float y_mean;                /**< Mean of training targets */
    float y_std;                 /**< Std of training targets */
    bool normalize_y;            /**< Y normalization flag */

    /* Precomputed quantities for prediction */
    float* L;                    /**< Cholesky of K + noise*I [n_train x n_train] */
    float* alpha;                /**< K^-1 * y [n_train] */
    float log_marginal_likelihood;/**< Log marginal likelihood */

    /* State */
    bool is_fitted;              /**< Model has been fitted */

    /* GPU resources */
    void* gpu_data;              /**< GPU-side data */
    bool use_gpu;                /**< GPU enabled */
    nimcp_gpu_context_t* gpu_ctx;/**< GPU context */
} nimcp_gp_t;

//=============================================================================
// Hidden Markov Model Types
//=============================================================================

/**
 * @brief HMM emission distribution type
 */
typedef enum nimcp_hmm_emission {
    NIMCP_HMM_EMISSION_GAUSSIAN,   /**< Gaussian emissions */
    NIMCP_HMM_EMISSION_GMM,        /**< GMM emissions per state */
    NIMCP_HMM_EMISSION_DISCRETE,   /**< Discrete/categorical emissions */
    NIMCP_HMM_EMISSION_POISSON     /**< Poisson emissions (for spike counts) */
} nimcp_hmm_emission_t;

/**
 * @brief HMM configuration
 */
typedef struct nimcp_hmm_config {
    uint32_t n_states;            /**< Number of hidden states */
    nimcp_hmm_emission_t emission_type; /**< Emission distribution type */
    uint32_t n_features;          /**< Number of observation features */
    uint32_t n_gmm_components;    /**< Components per state (for GMM) */
    float tol;                    /**< Baum-Welch tolerance */
    uint32_t max_iter;            /**< Maximum Baum-Welch iterations */
    bool left_right;              /**< Left-right topology constraint */
    uint32_t random_seed;         /**< Random seed */
    bool use_gpu;                 /**< GPU acceleration */
} nimcp_hmm_config_t;

/**
 * @brief Hidden Markov Model
 */
typedef struct nimcp_hmm {
    uint32_t magic;              /**< Magic number */
    uint32_t n_states;           /**< Number of hidden states */
    uint32_t n_features;         /**< Number of observation features */
    nimcp_hmm_emission_t emission_type; /**< Emission type */

    /* Model parameters */
    float* initial_prob;         /**< Initial state distribution [n_states] */
    float* transition_prob;      /**< Transition matrix [n_states x n_states] */

    /* Emission parameters (depend on emission_type) */
    float* emission_means;       /**< Gaussian means [n_states x n_features] */
    float* emission_covars;      /**< Gaussian covariances */
    float* emission_probs;       /**< Discrete emission probs */
    nimcp_gmm_t** state_gmms;    /**< GMM for each state (if GMM emissions) */
    float* emission_rates;       /**< Poisson rates [n_states x n_features] */

    /* Log versions for numerical stability */
    float* log_initial;          /**< Log initial probs */
    float* log_transition;       /**< Log transition probs */

    /* State */
    bool is_fitted;              /**< Model has been fitted */
    float log_likelihood;        /**< Final log-likelihood */
    uint32_t n_iter;             /**< Iterations performed */
    bool converged;              /**< Whether Baum-Welch converged */

    /* GPU resources */
    void* gpu_data;              /**< GPU-side data */
    bool use_gpu;                /**< GPU enabled */
    nimcp_gpu_context_t* gpu_ctx;/**< GPU context */
} nimcp_hmm_t;

/**
 * @brief HMM decoding result
 */
typedef struct nimcp_hmm_decode_result {
    uint32_t* states;            /**< Decoded state sequence [T] */
    float* state_probs;          /**< State probabilities [T x n_states] */
    float log_likelihood;        /**< Sequence log-likelihood */
    uint32_t length;             /**< Sequence length */
} nimcp_hmm_decode_result_t;

//=============================================================================
// Kernel Density Estimation Types
//=============================================================================

/**
 * @brief KDE kernel types
 */
typedef enum nimcp_kde_kernel {
    NIMCP_KDE_KERNEL_GAUSSIAN,    /**< Gaussian kernel */
    NIMCP_KDE_KERNEL_TOPHAT,      /**< Top-hat (uniform) kernel */
    NIMCP_KDE_KERNEL_EPANECHNIKOV,/**< Epanechnikov kernel */
    NIMCP_KDE_KERNEL_EXPONENTIAL, /**< Exponential kernel */
    NIMCP_KDE_KERNEL_LINEAR,      /**< Linear kernel */
    NIMCP_KDE_KERNEL_COSINE       /**< Cosine kernel */
} nimcp_kde_kernel_t;

/**
 * @brief Bandwidth selection method
 */
typedef enum nimcp_kde_bw_method {
    NIMCP_KDE_BW_SILVERMAN,       /**< Silverman's rule of thumb */
    NIMCP_KDE_BW_SCOTT,           /**< Scott's rule */
    NIMCP_KDE_BW_ISJ,             /**< Improved Sheather-Jones */
    NIMCP_KDE_BW_CV_ML,           /**< Maximum likelihood CV */
    NIMCP_KDE_BW_CV_LS,           /**< Least squares CV */
    NIMCP_KDE_BW_MANUAL           /**< User-specified bandwidth */
} nimcp_kde_bw_method_t;

/**
 * @brief KDE configuration
 */
typedef struct nimcp_kde_config {
    nimcp_kde_kernel_t kernel;    /**< Kernel type */
    nimcp_kde_bw_method_t bw_method; /**< Bandwidth selection method */
    float bandwidth;              /**< Manual bandwidth (if BW_MANUAL) */
    float* bandwidths;            /**< Per-dimension bandwidths */
    bool adaptive;                /**< Use adaptive bandwidth */
    uint32_t cv_folds;            /**< Folds for CV bandwidth selection */
    bool use_gpu;                 /**< GPU acceleration */
} nimcp_kde_config_t;

/**
 * @brief Kernel Density Estimator
 */
typedef struct nimcp_kde {
    uint32_t magic;              /**< Magic number */
    uint32_t n_samples;          /**< Number of samples */
    uint32_t n_features;         /**< Number of features */
    nimcp_kde_kernel_t kernel;   /**< Kernel type */

    /* Data */
    float* data;                 /**< Training data [n_samples x n_features] */
    float* bandwidth;            /**< Bandwidth [n_features] or [1] */
    float* adaptive_bw;          /**< Adaptive bandwidths [n_samples] */
    bool adaptive;               /**< Adaptive mode */

    /* Normalization */
    float log_norm;              /**< Log normalization constant */

    /* State */
    bool is_fitted;              /**< Model fitted */

    /* GPU resources */
    void* gpu_data;              /**< GPU-side data */
    bool use_gpu;                /**< GPU enabled */
    nimcp_gpu_context_t* gpu_ctx;/**< GPU context */
} nimcp_kde_t;

//=============================================================================
// Naive Bayes Types
//=============================================================================

/**
 * @brief Naive Bayes variant
 */
typedef enum nimcp_nb_type {
    NIMCP_NB_GAUSSIAN,           /**< Gaussian (continuous features) */
    NIMCP_NB_MULTINOMIAL,        /**< Multinomial (count features) */
    NIMCP_NB_BERNOULLI,          /**< Bernoulli (binary features) */
    NIMCP_NB_COMPLEMENT          /**< Complement Naive Bayes */
} nimcp_nb_type_t;

/**
 * @brief Naive Bayes configuration
 */
typedef struct nimcp_nb_config {
    nimcp_nb_type_t type;        /**< NB variant */
    float alpha;                 /**< Laplace smoothing parameter */
    bool fit_prior;              /**< Fit class prior from data */
    float* class_prior;          /**< Manual class prior */
    uint32_t n_classes;          /**< Number of classes */
    float var_smoothing;         /**< Variance smoothing (Gaussian) */
} nimcp_nb_config_t;

/**
 * @brief Naive Bayes classifier
 */
typedef struct nimcp_nb {
    uint32_t magic;              /**< Magic number */
    nimcp_nb_type_t type;        /**< NB variant */
    uint32_t n_classes;          /**< Number of classes */
    uint32_t n_features;         /**< Number of features */

    /* Class information */
    float* class_prior;          /**< Log class priors [n_classes] */
    float* class_count;          /**< Class sample counts [n_classes] */

    /* Feature statistics (variant-specific) */
    /* Gaussian */
    float* theta;                /**< Feature means [n_classes x n_features] */
    float* var;                  /**< Feature variances [n_classes x n_features] */

    /* Multinomial/Bernoulli */
    float* feature_log_prob;     /**< Log P(feature|class) [n_classes x n_features] */
    float* feature_count;        /**< Feature counts [n_classes x n_features] */

    float alpha;                 /**< Smoothing parameter */

    /* State */
    bool is_fitted;              /**< Model fitted */
} nimcp_nb_t;

//=============================================================================
// Model Evaluation Types
//=============================================================================

/**
 * @brief Confusion matrix
 */
typedef struct nimcp_confusion_matrix {
    uint32_t n_classes;          /**< Number of classes */
    uint32_t* matrix;            /**< Confusion matrix [n_classes x n_classes] */
    uint32_t total;              /**< Total samples */
    float accuracy;              /**< Overall accuracy */
    float* precision;            /**< Per-class precision [n_classes] */
    float* recall;               /**< Per-class recall [n_classes] */
    float* f1;                   /**< Per-class F1 score [n_classes] */
    float macro_f1;              /**< Macro-averaged F1 */
    float weighted_f1;           /**< Weighted-averaged F1 */
} nimcp_confusion_matrix_t;

/**
 * @brief ROC curve data
 */
typedef struct nimcp_roc_curve {
    uint32_t n_points;           /**< Number of threshold points */
    float* fpr;                  /**< False positive rates [n_points] */
    float* tpr;                  /**< True positive rates [n_points] */
    float* thresholds;           /**< Threshold values [n_points] */
    float auc;                   /**< Area under curve */
} nimcp_roc_curve_t;

/**
 * @brief Cross-validation result
 */
typedef struct nimcp_cv_result {
    uint32_t n_folds;            /**< Number of folds */
    float* scores;               /**< Per-fold scores [n_folds] */
    float mean_score;            /**< Mean score */
    float std_score;             /**< Standard deviation */
    float* train_scores;         /**< Training scores [n_folds] */
} nimcp_cv_result_t;

//=============================================================================
// GMM API
//=============================================================================

/**
 * @brief Get default GMM configuration
 */
NIMCP_EXPORT nimcp_gmm_config_t nimcp_gmm_default_config(void);

/**
 * @brief Create GMM model
 *
 * WHAT: Allocates and initializes a Gaussian Mixture Model
 * WHY:  Density estimation, clustering, generative modeling
 * HOW:  Allocates parameter storage based on config
 *
 * @param config Configuration (NULL for defaults)
 * @return GMM model or NULL on failure
 */
NIMCP_EXPORT nimcp_gmm_t* nimcp_gmm_create(const nimcp_gmm_config_t* config);

/**
 * @brief Destroy GMM model
 *
 * @param gmm Model to destroy
 */
NIMCP_EXPORT void nimcp_gmm_destroy(nimcp_gmm_t* gmm);

/**
 * @brief Fit GMM to data using EM algorithm
 *
 * WHAT: Fits mixture model to data via Expectation-Maximization
 * WHY:  Estimate latent cluster structure in data
 * HOW:  Iterates E-step (compute responsibilities) and M-step (update params)
 *
 * @param gmm GMM model
 * @param X Data matrix [n_samples x n_features]
 * @param n_samples Number of samples
 * @param n_features Number of features
 * @param result Output fit result (optional)
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_gmm_fit(
    nimcp_gmm_t* gmm,
    const float* X,
    uint32_t n_samples,
    uint32_t n_features,
    nimcp_gmm_fit_result_t* result
);

/**
 * @brief Predict cluster labels (hard assignment)
 *
 * @param gmm Fitted GMM
 * @param X Data matrix [n_samples x n_features]
 * @param n_samples Number of samples
 * @param labels Output labels [n_samples]
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_gmm_predict(
    const nimcp_gmm_t* gmm,
    const float* X,
    uint32_t n_samples,
    uint32_t* labels
);

/**
 * @brief Predict cluster probabilities (soft assignment)
 *
 * @param gmm Fitted GMM
 * @param X Data matrix [n_samples x n_features]
 * @param n_samples Number of samples
 * @param probs Output probabilities [n_samples x n_components]
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_gmm_predict_proba(
    const nimcp_gmm_t* gmm,
    const float* X,
    uint32_t n_samples,
    float* probs
);

/**
 * @brief Compute log-likelihood score
 *
 * @param gmm Fitted GMM
 * @param X Data matrix [n_samples x n_features]
 * @param n_samples Number of samples
 * @param score Output log-likelihood
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_gmm_score(
    const nimcp_gmm_t* gmm,
    const float* X,
    uint32_t n_samples,
    float* score
);

/**
 * @brief Compute per-sample log-likelihood
 *
 * @param gmm Fitted GMM
 * @param X Data matrix [n_samples x n_features]
 * @param n_samples Number of samples
 * @param scores Output per-sample scores [n_samples]
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_gmm_score_samples(
    const nimcp_gmm_t* gmm,
    const float* X,
    uint32_t n_samples,
    float* scores
);

/**
 * @brief Compute Bayesian Information Criterion
 *
 * @param gmm Fitted GMM
 * @param X Data used for fitting
 * @param n_samples Number of samples
 * @param bic Output BIC value
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_gmm_bic(
    const nimcp_gmm_t* gmm,
    const float* X,
    uint32_t n_samples,
    float* bic
);

/**
 * @brief Compute Akaike Information Criterion
 *
 * @param gmm Fitted GMM
 * @param X Data used for fitting
 * @param n_samples Number of samples
 * @param aic Output AIC value
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_gmm_aic(
    const nimcp_gmm_t* gmm,
    const float* X,
    uint32_t n_samples,
    float* aic
);

/**
 * @brief Sample from fitted GMM
 *
 * @param gmm Fitted GMM
 * @param n_samples Number of samples to generate
 * @param samples Output samples [n_samples x n_features]
 * @param labels Output component labels (optional) [n_samples]
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_gmm_sample(
    const nimcp_gmm_t* gmm,
    uint32_t n_samples,
    float* samples,
    uint32_t* labels
);

//=============================================================================
// Gaussian Process API
//=============================================================================

/**
 * @brief Get default GP configuration
 */
NIMCP_EXPORT nimcp_gp_config_t nimcp_gp_default_config(void);

/**
 * @brief Create default kernel parameters for specific type
 */
NIMCP_EXPORT nimcp_gp_kernel_params_t nimcp_gp_kernel_default(nimcp_gp_kernel_t type);

/**
 * @brief Create Gaussian Process model
 *
 * @param config Configuration (NULL for defaults)
 * @return GP model or NULL on failure
 */
NIMCP_EXPORT nimcp_gp_t* nimcp_gp_create(const nimcp_gp_config_t* config);

/**
 * @brief Destroy Gaussian Process model
 *
 * @param gp Model to destroy
 */
NIMCP_EXPORT void nimcp_gp_destroy(nimcp_gp_t* gp);

/**
 * @brief Fit GP to data
 *
 * WHAT: Fits Gaussian Process regression to training data
 * WHY:  Non-parametric regression with uncertainty quantification
 * HOW:  Computes Cholesky of kernel matrix, solves for alpha
 *
 * @param gp GP model
 * @param X Training inputs [n_samples x n_features]
 * @param y Training targets [n_samples]
 * @param n_samples Number of samples
 * @param n_features Number of input features
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_gp_fit(
    nimcp_gp_t* gp,
    const float* X,
    const float* y,
    uint32_t n_samples,
    uint32_t n_features
);

/**
 * @brief Predict mean at test points
 *
 * @param gp Fitted GP
 * @param X_test Test inputs [n_test x n_features]
 * @param n_test Number of test points
 * @param y_pred Output predictions [n_test]
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_gp_predict(
    const nimcp_gp_t* gp,
    const float* X_test,
    uint32_t n_test,
    float* y_pred
);

/**
 * @brief Predict with uncertainty (mean and standard deviation)
 *
 * @param gp Fitted GP
 * @param X_test Test inputs [n_test x n_features]
 * @param n_test Number of test points
 * @param y_pred Output mean predictions [n_test]
 * @param y_std Output standard deviations [n_test]
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_gp_predict_with_std(
    const nimcp_gp_t* gp,
    const float* X_test,
    uint32_t n_test,
    float* y_pred,
    float* y_std
);

/**
 * @brief Compute log marginal likelihood
 *
 * @param gp Fitted GP
 * @param log_ml Output log marginal likelihood
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_gp_log_marginal_likelihood(
    const nimcp_gp_t* gp,
    float* log_ml
);

/**
 * @brief Compute kernel matrix between two sets of points
 *
 * @param kernel Kernel parameters
 * @param X1 First set [n1 x n_features]
 * @param X2 Second set [n2 x n_features]
 * @param n1 Number of points in first set
 * @param n2 Number of points in second set
 * @param n_features Number of features
 * @param K Output kernel matrix [n1 x n2]
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_gp_kernel_matrix(
    const nimcp_gp_kernel_params_t* kernel,
    const float* X1,
    const float* X2,
    uint32_t n1,
    uint32_t n2,
    uint32_t n_features,
    float* K
);

/**
 * @brief Sample from GP posterior
 *
 * @param gp Fitted GP
 * @param X_test Test inputs [n_test x n_features]
 * @param n_test Number of test points
 * @param n_samples Number of samples to draw
 * @param samples Output samples [n_samples x n_test]
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_gp_sample(
    const nimcp_gp_t* gp,
    const float* X_test,
    uint32_t n_test,
    uint32_t n_samples,
    float* samples
);

//=============================================================================
// Hidden Markov Model API
//=============================================================================

/**
 * @brief Get default HMM configuration
 */
NIMCP_EXPORT nimcp_hmm_config_t nimcp_hmm_default_config(void);

/**
 * @brief Create HMM model
 *
 * @param config Configuration (NULL for defaults)
 * @return HMM model or NULL on failure
 */
NIMCP_EXPORT nimcp_hmm_t* nimcp_hmm_create(const nimcp_hmm_config_t* config);

/**
 * @brief Destroy HMM model
 *
 * @param hmm Model to destroy
 */
NIMCP_EXPORT void nimcp_hmm_destroy(nimcp_hmm_t* hmm);

/**
 * @brief Fit HMM using Baum-Welch algorithm
 *
 * WHAT: Fits HMM parameters to observation sequences
 * WHY:  Learn temporal state dynamics from data
 * HOW:  Iterative EM using forward-backward algorithm
 *
 * @param hmm HMM model
 * @param observations Observation sequences [total_length x n_features]
 * @param seq_lengths Length of each sequence [n_sequences]
 * @param n_sequences Number of sequences
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_hmm_fit(
    nimcp_hmm_t* hmm,
    const float* observations,
    const uint32_t* seq_lengths,
    uint32_t n_sequences
);

/**
 * @brief Decode most likely state sequence (Viterbi)
 *
 * WHAT: Finds most probable hidden state sequence
 * WHY:  State segmentation, pattern recognition
 * HOW:  Dynamic programming (Viterbi algorithm)
 *
 * @param hmm Fitted HMM
 * @param observations Observation sequence [length x n_features]
 * @param length Sequence length
 * @param result Output decode result
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_hmm_decode(
    const nimcp_hmm_t* hmm,
    const float* observations,
    uint32_t length,
    nimcp_hmm_decode_result_t* result
);

/**
 * @brief Compute state probabilities (forward algorithm)
 *
 * @param hmm Fitted HMM
 * @param observations Observation sequence [length x n_features]
 * @param length Sequence length
 * @param state_probs Output state probabilities [length x n_states]
 * @param log_likelihood Output sequence log-likelihood
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_hmm_predict(
    const nimcp_hmm_t* hmm,
    const float* observations,
    uint32_t length,
    float* state_probs,
    float* log_likelihood
);

/**
 * @brief Sample from HMM
 *
 * @param hmm Fitted HMM
 * @param length Sequence length to generate
 * @param observations Output observations [length x n_features]
 * @param states Output states [length]
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_hmm_sample(
    const nimcp_hmm_t* hmm,
    uint32_t length,
    float* observations,
    uint32_t* states
);

/**
 * @brief Compute sequence log-likelihood
 *
 * @param hmm Fitted HMM
 * @param observations Observation sequence [length x n_features]
 * @param length Sequence length
 * @param log_likelihood Output log-likelihood
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_hmm_score(
    const nimcp_hmm_t* hmm,
    const float* observations,
    uint32_t length,
    float* log_likelihood
);

/**
 * @brief Free HMM decode result
 *
 * @param result Decode result to free
 */
NIMCP_EXPORT void nimcp_hmm_decode_result_free(nimcp_hmm_decode_result_t* result);

//=============================================================================
// Kernel Density Estimation API
//=============================================================================

/**
 * @brief Get default KDE configuration
 */
NIMCP_EXPORT nimcp_kde_config_t nimcp_kde_default_config(void);

/**
 * @brief Create KDE model
 *
 * @param config Configuration (NULL for defaults)
 * @return KDE model or NULL on failure
 */
NIMCP_EXPORT nimcp_kde_t* nimcp_kde_create(const nimcp_kde_config_t* config);

/**
 * @brief Destroy KDE model
 *
 * @param kde Model to destroy
 */
NIMCP_EXPORT void nimcp_kde_destroy(nimcp_kde_t* kde);

/**
 * @brief Fit KDE to data
 *
 * WHAT: Fits kernel density estimator to data
 * WHY:  Non-parametric probability density estimation
 * HOW:  Stores data points, computes optimal bandwidth
 *
 * @param kde KDE model
 * @param X Data matrix [n_samples x n_features]
 * @param n_samples Number of samples
 * @param n_features Number of features
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_kde_fit(
    nimcp_kde_t* kde,
    const float* X,
    uint32_t n_samples,
    uint32_t n_features
);

/**
 * @brief Evaluate density at points
 *
 * @param kde Fitted KDE
 * @param X Points to evaluate [n_points x n_features]
 * @param n_points Number of points
 * @param density Output log-densities [n_points]
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_kde_evaluate(
    const nimcp_kde_t* kde,
    const float* X,
    uint32_t n_points,
    float* density
);

/**
 * @brief Sample from KDE
 *
 * @param kde Fitted KDE
 * @param n_samples Number of samples to draw
 * @param samples Output samples [n_samples x n_features]
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_kde_sample(
    const nimcp_kde_t* kde,
    uint32_t n_samples,
    float* samples
);

/**
 * @brief Compute Silverman bandwidth
 *
 * @param X Data [n_samples x n_features]
 * @param n_samples Number of samples
 * @param n_features Number of features
 * @param bandwidth Output bandwidth [n_features]
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_kde_bandwidth_silverman(
    const float* X,
    uint32_t n_samples,
    uint32_t n_features,
    float* bandwidth
);

/**
 * @brief Compute Scott bandwidth
 *
 * @param X Data [n_samples x n_features]
 * @param n_samples Number of samples
 * @param n_features Number of features
 * @param bandwidth Output bandwidth [n_features]
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_kde_bandwidth_scott(
    const float* X,
    uint32_t n_samples,
    uint32_t n_features,
    float* bandwidth
);

/**
 * @brief Compute bandwidth via cross-validation
 *
 * @param kde KDE model
 * @param X Data [n_samples x n_features]
 * @param n_samples Number of samples
 * @param n_features Number of features
 * @param n_folds Number of CV folds
 * @param bandwidth Output optimal bandwidth [n_features]
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_kde_bandwidth_cv(
    nimcp_kde_t* kde,
    const float* X,
    uint32_t n_samples,
    uint32_t n_features,
    uint32_t n_folds,
    float* bandwidth
);

//=============================================================================
// Naive Bayes API
//=============================================================================

/**
 * @brief Get default Naive Bayes configuration
 */
NIMCP_EXPORT nimcp_nb_config_t nimcp_nb_default_config(nimcp_nb_type_t type);

/**
 * @brief Create Naive Bayes classifier
 *
 * @param config Configuration
 * @return NB classifier or NULL on failure
 */
NIMCP_EXPORT nimcp_nb_t* nimcp_nb_create(const nimcp_nb_config_t* config);

/**
 * @brief Destroy Naive Bayes classifier
 *
 * @param nb Classifier to destroy
 */
NIMCP_EXPORT void nimcp_nb_destroy(nimcp_nb_t* nb);

/**
 * @brief Fit Gaussian Naive Bayes
 *
 * @param nb NB classifier (must be NIMCP_NB_GAUSSIAN)
 * @param X Features [n_samples x n_features]
 * @param y Labels [n_samples]
 * @param n_samples Number of samples
 * @param n_features Number of features
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_nb_gaussian_fit(
    nimcp_nb_t* nb,
    const float* X,
    const uint32_t* y,
    uint32_t n_samples,
    uint32_t n_features
);

/**
 * @brief Predict with Gaussian Naive Bayes
 *
 * @param nb Fitted NB classifier
 * @param X Features [n_samples x n_features]
 * @param n_samples Number of samples
 * @param y_pred Output predictions [n_samples]
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_nb_gaussian_predict(
    const nimcp_nb_t* nb,
    const float* X,
    uint32_t n_samples,
    uint32_t* y_pred
);

/**
 * @brief Fit Multinomial Naive Bayes
 *
 * @param nb NB classifier (must be NIMCP_NB_MULTINOMIAL)
 * @param X Features (counts) [n_samples x n_features]
 * @param y Labels [n_samples]
 * @param n_samples Number of samples
 * @param n_features Number of features
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_nb_multinomial_fit(
    nimcp_nb_t* nb,
    const float* X,
    const uint32_t* y,
    uint32_t n_samples,
    uint32_t n_features
);

/**
 * @brief Predict with Multinomial Naive Bayes
 *
 * @param nb Fitted NB classifier
 * @param X Features [n_samples x n_features]
 * @param n_samples Number of samples
 * @param y_pred Output predictions [n_samples]
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_nb_multinomial_predict(
    const nimcp_nb_t* nb,
    const float* X,
    uint32_t n_samples,
    uint32_t* y_pred
);

/**
 * @brief Fit Bernoulli Naive Bayes
 *
 * @param nb NB classifier (must be NIMCP_NB_BERNOULLI)
 * @param X Binary features [n_samples x n_features]
 * @param y Labels [n_samples]
 * @param n_samples Number of samples
 * @param n_features Number of features
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_nb_bernoulli_fit(
    nimcp_nb_t* nb,
    const float* X,
    const uint32_t* y,
    uint32_t n_samples,
    uint32_t n_features
);

/**
 * @brief Predict with Bernoulli Naive Bayes
 *
 * @param nb Fitted NB classifier
 * @param X Features [n_samples x n_features]
 * @param n_samples Number of samples
 * @param y_pred Output predictions [n_samples]
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_nb_bernoulli_predict(
    const nimcp_nb_t* nb,
    const float* X,
    uint32_t n_samples,
    uint32_t* y_pred
);

/**
 * @brief Predict class probabilities
 *
 * @param nb Fitted NB classifier
 * @param X Features [n_samples x n_features]
 * @param n_samples Number of samples
 * @param probs Output probabilities [n_samples x n_classes]
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_nb_predict_proba(
    const nimcp_nb_t* nb,
    const float* X,
    uint32_t n_samples,
    float* probs
);

//=============================================================================
// Model Evaluation API
//=============================================================================

/**
 * @brief Perform k-fold cross-validation
 *
 * @param X Features [n_samples x n_features]
 * @param y Labels [n_samples]
 * @param n_samples Number of samples
 * @param n_features Number of features
 * @param n_folds Number of folds
 * @param model_type Type identifier for model to evaluate
 * @param model_config Model configuration
 * @param result Output CV result
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_ml_cross_validate(
    const float* X,
    const uint32_t* y,
    uint32_t n_samples,
    uint32_t n_features,
    uint32_t n_folds,
    const char* model_type,
    const void* model_config,
    nimcp_cv_result_t* result
);

/**
 * @brief Compute confusion matrix
 *
 * @param y_true True labels [n_samples]
 * @param y_pred Predicted labels [n_samples]
 * @param n_samples Number of samples
 * @param n_classes Number of classes
 * @param cm Output confusion matrix
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_ml_confusion_matrix(
    const uint32_t* y_true,
    const uint32_t* y_pred,
    uint32_t n_samples,
    uint32_t n_classes,
    nimcp_confusion_matrix_t* cm
);

/**
 * @brief Compute ROC curve
 *
 * @param y_true True binary labels [n_samples]
 * @param y_score Predicted scores/probabilities [n_samples]
 * @param n_samples Number of samples
 * @param roc Output ROC curve
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_ml_roc_curve(
    const uint32_t* y_true,
    const float* y_score,
    uint32_t n_samples,
    nimcp_roc_curve_t* roc
);

/**
 * @brief Compute Area Under ROC Curve
 *
 * @param y_true True binary labels [n_samples]
 * @param y_score Predicted scores [n_samples]
 * @param n_samples Number of samples
 * @param auc Output AUC value
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_ml_auc(
    const uint32_t* y_true,
    const float* y_score,
    uint32_t n_samples,
    float* auc
);

/**
 * @brief Compute precision-recall curve
 *
 * @param y_true True binary labels [n_samples]
 * @param y_score Predicted scores [n_samples]
 * @param n_samples Number of samples
 * @param n_points Number of curve points to compute
 * @param precision Output precision values [n_points]
 * @param recall Output recall values [n_points]
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_ml_precision_recall(
    const uint32_t* y_true,
    const float* y_score,
    uint32_t n_samples,
    uint32_t n_points,
    float* precision,
    float* recall
);

/**
 * @brief Compute F1 score (or F-beta score)
 *
 * @param y_true True labels [n_samples]
 * @param y_pred Predicted labels [n_samples]
 * @param n_samples Number of samples
 * @param beta Beta parameter (1.0 for F1)
 * @param f_score Output F-beta score
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_ml_f1_score(
    const uint32_t* y_true,
    const uint32_t* y_pred,
    uint32_t n_samples,
    float beta,
    float* f_score
);

/**
 * @brief Free confusion matrix resources
 *
 * @param cm Confusion matrix to free
 */
NIMCP_EXPORT void nimcp_ml_confusion_matrix_free(nimcp_confusion_matrix_t* cm);

/**
 * @brief Free ROC curve resources
 *
 * @param roc ROC curve to free
 */
NIMCP_EXPORT void nimcp_ml_roc_curve_free(nimcp_roc_curve_t* roc);

/**
 * @brief Free CV result resources
 *
 * @param result CV result to free
 */
NIMCP_EXPORT void nimcp_ml_cv_result_free(nimcp_cv_result_t* result);

//=============================================================================
// GPU-Accelerated Operations
//=============================================================================

/**
 * @brief Set GPU context for ML operations
 *
 * @param ctx GPU context (NULL to disable GPU)
 */
NIMCP_EXPORT void nimcp_ml_set_gpu_context(nimcp_gpu_context_t* ctx);

/**
 * @brief Get current GPU context
 *
 * @return Current GPU context or NULL
 */
NIMCP_EXPORT nimcp_gpu_context_t* nimcp_ml_get_gpu_context(void);

/**
 * @brief Check if GPU is available for ML operations
 *
 * @return true if GPU available
 */
NIMCP_EXPORT bool nimcp_ml_gpu_available(void);

//=============================================================================
// Module Lifecycle
//=============================================================================

/**
 * @brief Initialize ML statistics module
 *
 * @param gpu_ctx GPU context (optional)
 * @return NIMCP_ML_OK on success
 */
NIMCP_EXPORT nimcp_ml_error_t nimcp_ml_statistics_init(nimcp_gpu_context_t* gpu_ctx);

/**
 * @brief Shutdown ML statistics module
 */
NIMCP_EXPORT void nimcp_ml_statistics_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ML_STATISTICS_H */
