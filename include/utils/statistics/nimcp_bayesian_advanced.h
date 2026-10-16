/**
 * @file nimcp_bayesian_advanced.h
 * @brief Advanced Bayesian Methods for NIMCP
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Advanced Bayesian inference toolkit including MCMC sampling,
 *       variational inference, hierarchical models, and model comparison
 *
 * WHY:  The brain is fundamentally Bayesian - perception, learning, and decision
 *       making all involve probabilistic inference. Advanced sampling and
 *       approximation methods enable complex posterior inference in neural models.
 *
 * HOW:  C99 implementation with GPU-accelerated MCMC, numerical stability
 *       (log-space computation), and convergence diagnostics
 *
 * NEUROSCIENCE FOUNDATION:
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │  Bayesian Brain Hypothesis:                                             │
 * │                                                                         │
 * │  Perception as Inference:                                               │
 * │    P(world | sensory) ∝ P(sensory | world) × P(world)                  │
 * │    The brain combines prior expectations with sensory likelihood        │
 * │                                                                         │
 * │  Learning as Posterior Update:                                          │
 * │    Synaptic weights encode posterior distributions                      │
 * │    Plasticity rules implement approximate Bayesian inference            │
 * │                                                                         │
 * │  Hierarchical Processing:                                               │
 * │    Cortical hierarchy implements hierarchical Bayesian models           │
 * │    Higher levels provide priors for lower levels                        │
 * │                                                                         │
 * │  Uncertainty Representation:                                            │
 * │    Neural populations encode not just means but variances               │
 * │    Dopamine signals precision-weighted prediction errors                │
 * │                                                                         │
 * │  This module provides the computational tools for these theories.       │
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 * FEATURES:
 * - MCMC: Metropolis-Hastings, Gibbs, Hamiltonian MC, No-U-Turn Sampler
 * - Variational Inference: Mean-field, ELBO optimization
 * - Hierarchical Models: Multi-level priors, random effects
 * - Model Comparison: Bayes factors, WAIC, LOO-CV, DIC
 * - Bayesian Regression: Linear and logistic with full posteriors
 *
 * PERFORMANCE:
 * - HMC/NUTS: GPU acceleration for gradient computation
 * - Parallel chains: Multi-threaded or multi-GPU
 * - Memory: Efficient sample storage with thinning
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BAYESIAN_ADVANCED_H
#define NIMCP_BAYESIAN_ADVANCED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum parameters for MCMC */
#define NIMCP_MCMC_MAX_PARAMS 1024

/** Maximum chains for parallel MCMC */
#define NIMCP_MCMC_MAX_CHAINS 16

/** Maximum hierarchy levels */
#define NIMCP_HIER_MAX_LEVELS 10

/** Default burn-in samples */
#define NIMCP_MCMC_DEFAULT_BURNIN 1000

/** Default thinning interval */
#define NIMCP_MCMC_DEFAULT_THIN 1

/** NUTS maximum tree depth */
#define NIMCP_NUTS_MAX_TREE_DEPTH 10

/** Default ELBO samples for VI */
#define NIMCP_VI_DEFAULT_SAMPLES 100

//=============================================================================
// Error Codes
//=============================================================================

/**
 * @brief Advanced Bayesian result codes
 */
typedef enum nimcp_bayes_adv_result {
    NIMCP_BAYES_OK = 0,                  /**< Success */
    NIMCP_BAYES_ERROR_NULL = -1,         /**< NULL pointer */
    NIMCP_BAYES_ERROR_SIZE = -2,         /**< Invalid size */
    NIMCP_BAYES_ERROR_MEMORY = -3,       /**< Memory allocation failed */
    NIMCP_BAYES_ERROR_PARAMS = -4,       /**< Invalid parameters */
    NIMCP_BAYES_ERROR_CONVERGE = -5,     /**< Did not converge */
    NIMCP_BAYES_ERROR_DIVERGENCE = -6,   /**< Divergent transitions (HMC) */
    NIMCP_BAYES_ERROR_GRADIENT = -7,     /**< Gradient computation failed */
    NIMCP_BAYES_ERROR_NOT_FIT = -8,      /**< Model not fitted */
    NIMCP_BAYES_ERROR_GPU = -9,          /**< GPU error */
    NIMCP_BAYES_ERROR_LOW_ESS = -10,     /**< Effective sample size too low */
    NIMCP_BAYES_ERROR_HIGH_RHAT = -11    /**< Rhat indicates non-convergence */
} nimcp_bayes_adv_result_t;

//=============================================================================
// Function Pointer Types
//=============================================================================

/**
 * @brief Log-posterior density function
 * @param params Parameter values (n_params x 1)
 * @param n_params Number of parameters
 * @param data User data pointer
 * @return Log posterior density (unnormalized)
 */
typedef double (*nimcp_log_posterior_fn)(
    const double* params,
    uint32_t n_params,
    void* data
);

/**
 * @brief Gradient of log-posterior
 * @param params Parameter values
 * @param n_params Number of parameters
 * @param data User data
 * @param grad Output gradient (n_params x 1)
 * @return 0 on success, -1 on failure
 */
typedef int (*nimcp_log_posterior_grad_fn)(
    const double* params,
    uint32_t n_params,
    void* data,
    double* grad
);

/**
 * @brief Conditional sampler for Gibbs
 * @param param_idx Index of parameter to sample
 * @param current_params Current parameter values
 * @param n_params Total parameters
 * @param data User data
 * @return Sampled value for parameter param_idx
 */
typedef double (*nimcp_gibbs_conditional_fn)(
    uint32_t param_idx,
    const double* current_params,
    uint32_t n_params,
    void* data
);

//=============================================================================
// MCMC Diagnostics
//=============================================================================

/**
 * @brief MCMC convergence diagnostics
 */
typedef struct nimcp_mcmc_diagnostics {
    double* rhat;              /**< Gelman-Rubin statistic per parameter */
    double* ess_bulk;          /**< Bulk effective sample size per parameter */
    double* ess_tail;          /**< Tail ESS per parameter */
    double* mcse;              /**< Monte Carlo standard error per parameter */
    uint32_t n_params;         /**< Number of parameters */
    uint32_t n_chains;         /**< Number of chains */
    uint32_t n_samples;        /**< Samples per chain */
    uint32_t n_divergent;      /**< Divergent transitions (HMC) */
    uint32_t n_max_treedepth;  /**< Samples hitting max tree depth (NUTS) */
    double mean_accept_prob;   /**< Mean acceptance probability */
    double mean_stepsize;      /**< Mean step size (HMC) */
    bool converged;            /**< All Rhat < 1.01 and ESS sufficient */
} nimcp_mcmc_diagnostics_t;

//=============================================================================
// MCMC Sampler
//=============================================================================

/**
 * @brief MCMC sampling algorithm
 */
typedef enum nimcp_mcmc_algorithm {
    NIMCP_MCMC_METROPOLIS_HASTINGS,   /**< Random-walk MH */
    NIMCP_MCMC_ADAPTIVE_MH,           /**< Adaptive MH (tunes proposal) */
    NIMCP_MCMC_GIBBS,                 /**< Gibbs sampling */
    NIMCP_MCMC_HMC,                   /**< Hamiltonian Monte Carlo */
    NIMCP_MCMC_NUTS                   /**< No-U-Turn Sampler (auto-tuned HMC) */
} nimcp_mcmc_algorithm_t;

/**
 * @brief MCMC configuration
 */
typedef struct nimcp_mcmc_config {
    nimcp_mcmc_algorithm_t algorithm; /**< Sampling algorithm */
    uint32_t n_samples;               /**< Number of samples to collect */
    uint32_t n_burnin;                /**< Burn-in samples to discard */
    uint32_t n_thin;                  /**< Thinning interval */
    uint32_t n_chains;                /**< Number of parallel chains */

    // MH-specific
    double* proposal_sd;              /**< Proposal standard deviations (n_params) */
    bool adapt_proposal;              /**< Adapt proposal during warmup */
    double target_accept;             /**< Target acceptance rate for adaptation */

    // HMC/NUTS-specific
    double step_size;                 /**< Leapfrog step size (0 = auto-tune) */
    uint32_t n_leapfrog;              /**< Leapfrog steps for HMC (ignored for NUTS) */
    double target_accept_hmc;         /**< Target acceptance for HMC (default 0.8) */
    uint32_t max_treedepth;           /**< Max tree depth for NUTS */

    // Gibbs-specific
    nimcp_gibbs_conditional_fn* conditionals; /**< Conditional samplers */

    // General
    uint32_t random_seed;             /**< RNG seed (0 = random) */
    bool use_gpu;                     /**< Use GPU for gradient computation */
    bool verbose;                     /**< Print progress */
} nimcp_mcmc_config_t;

/**
 * @brief MCMC sampler state
 */
typedef struct nimcp_mcmc_sampler {
    double* samples;                  /**< Samples (n_chains x n_samples x n_params) */
    double* log_posterior;            /**< Log posterior at each sample */
    uint32_t n_params;                /**< Number of parameters */
    uint32_t n_samples;               /**< Actual samples collected */
    uint32_t n_chains;                /**< Number of chains */
    nimcp_mcmc_config_t config;       /**< Configuration used */
    nimcp_mcmc_diagnostics_t diag;    /**< Convergence diagnostics */
    bool fitted;                      /**< Sampling completed */
} nimcp_mcmc_sampler_t;

/**
 * @brief Create MCMC sampler
 * @param n_params Number of parameters
 * @param config Sampler configuration
 * @return New sampler or NULL
 */
NIMCP_EXPORT nimcp_mcmc_sampler_t* nimcp_mcmc_create(
    uint32_t n_params,
    const nimcp_mcmc_config_t* config
);

/**
 * @brief Destroy MCMC sampler
 * @param mcmc Sampler to destroy
 */
NIMCP_EXPORT void nimcp_mcmc_destroy(nimcp_mcmc_sampler_t* mcmc);

/**
 * @brief Get default MCMC configuration
 * @param algorithm Desired algorithm
 * @return Default configuration
 */
NIMCP_EXPORT nimcp_mcmc_config_t nimcp_mcmc_default_config(
    nimcp_mcmc_algorithm_t algorithm
);

/**
 * @brief Run Metropolis-Hastings sampler
 * @param mcmc Sampler
 * @param log_posterior Log posterior function
 * @param initial_params Starting values (n_params x 1)
 * @param data User data passed to log_posterior
 * @return NIMCP_BAYES_OK on success
 *
 * Standard random-walk MH with optional adaptive proposal.
 * Proposal: q(θ'|θ) = N(θ, Σ)
 * Accept with probability min(1, π(θ')q(θ|θ') / π(θ)q(θ'|θ))
 */
NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_mcmc_metropolis_hastings(
    nimcp_mcmc_sampler_t* mcmc,
    nimcp_log_posterior_fn log_posterior,
    const double* initial_params,
    void* data
);

/**
 * @brief Run Gibbs sampler
 * @param mcmc Sampler (must have conditional samplers configured)
 * @param initial_params Starting values
 * @param data User data
 * @return NIMCP_BAYES_OK on success
 *
 * Samples each parameter from its full conditional distribution.
 * Requires user to provide conditional samplers for each parameter.
 */
NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_mcmc_gibbs(
    nimcp_mcmc_sampler_t* mcmc,
    const double* initial_params,
    void* data
);

/**
 * @brief Run Hamiltonian Monte Carlo
 * @param mcmc Sampler
 * @param log_posterior Log posterior function
 * @param grad_log_posterior Gradient function
 * @param initial_params Starting values
 * @param data User data
 * @return NIMCP_BAYES_OK on success
 *
 * HMC uses Hamiltonian dynamics for efficient exploration:
 * H(θ, p) = -log π(θ) + p'M⁻¹p/2
 *
 * Parameters are updated via leapfrog integration.
 */
NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_mcmc_hamiltonian(
    nimcp_mcmc_sampler_t* mcmc,
    nimcp_log_posterior_fn log_posterior,
    nimcp_log_posterior_grad_fn grad_log_posterior,
    const double* initial_params,
    void* data
);

/**
 * @brief Run No-U-Turn Sampler (NUTS)
 * @param mcmc Sampler
 * @param log_posterior Log posterior function
 * @param grad_log_posterior Gradient function
 * @param initial_params Starting values
 * @param data User data
 * @return NIMCP_BAYES_OK on success
 *
 * NUTS automatically tunes the number of leapfrog steps by
 * building a binary tree until a "U-turn" is detected.
 * Also auto-tunes step size during warmup.
 */
NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_mcmc_nuts(
    nimcp_mcmc_sampler_t* mcmc,
    nimcp_log_posterior_fn log_posterior,
    nimcp_log_posterior_grad_fn grad_log_posterior,
    const double* initial_params,
    void* data
);

/**
 * @brief Run MCMC with GPU acceleration
 * @param mcmc Sampler
 * @param log_posterior Log posterior (called on CPU)
 * @param grad_log_posterior Gradient (can use GPU internally)
 * @param initial_params Starting values
 * @param data User data
 * @return NIMCP_BAYES_OK on success
 *
 * For HMC/NUTS, gradient computations are batched on GPU.
 */
NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_mcmc_run_gpu(
    nimcp_mcmc_sampler_t* mcmc,
    nimcp_log_posterior_fn log_posterior,
    nimcp_log_posterior_grad_fn grad_log_posterior,
    const double* initial_params,
    void* data
);

/**
 * @brief Get posterior samples
 * @param mcmc Fitted sampler
 * @param chain Chain index (or -1 for all chains combined)
 * @param param_idx Parameter index (or -1 for all parameters)
 * @param samples Output samples
 * @param n_samples Output number of samples
 * @return NIMCP_BAYES_OK on success
 */
NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_mcmc_get_samples(
    const nimcp_mcmc_sampler_t* mcmc,
    int32_t chain,
    int32_t param_idx,
    double** samples,
    uint32_t* n_samples
);

/**
 * @brief Compute MCMC diagnostics
 * @param mcmc Fitted sampler
 * @param diagnostics Output diagnostics
 * @return NIMCP_BAYES_OK on success
 *
 * Computes Rhat, ESS, MCSE for all parameters.
 */
NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_mcmc_diagnostics(
    const nimcp_mcmc_sampler_t* mcmc,
    nimcp_mcmc_diagnostics_t* diagnostics
);

/**
 * @brief Get posterior summary statistics
 * @param mcmc Fitted sampler
 * @param param_idx Parameter index
 * @param mean Output mean
 * @param std_dev Output standard deviation
 * @param ci_lower Output lower credible bound (2.5%)
 * @param ci_upper Output upper credible bound (97.5%)
 * @return NIMCP_BAYES_OK on success
 */
NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_mcmc_summary(
    const nimcp_mcmc_sampler_t* mcmc,
    uint32_t param_idx,
    double* mean,
    double* std_dev,
    double* ci_lower,
    double* ci_upper
);

/**
 * @brief Free diagnostics structure
 * @param diag Diagnostics to free
 */
NIMCP_EXPORT void nimcp_mcmc_diagnostics_free(nimcp_mcmc_diagnostics_t* diag);

//=============================================================================
// Variational Inference
//=============================================================================

/**
 * @brief Variational family type
 */
typedef enum nimcp_vi_family {
    NIMCP_VI_MEAN_FIELD,         /**< Independent Gaussian for each parameter */
    NIMCP_VI_FULL_RANK,          /**< Full-covariance Gaussian */
    NIMCP_VI_LOW_RANK            /**< Low-rank + diagonal covariance */
} nimcp_vi_family_t;

/**
 * @brief Variational inference configuration
 */
typedef struct nimcp_vi_config {
    nimcp_vi_family_t family;     /**< Variational family */
    uint32_t max_iterations;      /**< Maximum optimization iterations */
    double tol;                   /**< Convergence tolerance (ELBO change) */
    double learning_rate;         /**< Initial learning rate */
    bool adapt_lr;                /**< Adapt learning rate */
    uint32_t n_elbo_samples;      /**< MC samples for ELBO estimation */
    uint32_t low_rank;            /**< Rank for low-rank family */
    uint32_t random_seed;         /**< RNG seed */
    bool use_gpu;                 /**< GPU acceleration */
    bool verbose;                 /**< Print progress */
} nimcp_vi_config_t;

/**
 * @brief Variational inference state
 */
typedef struct nimcp_vi_optimizer {
    double* mean;                 /**< Variational mean (n_params x 1) */
    double* variance;             /**< Variational variance (diagonal) */
    double* covariance;           /**< Full covariance (if full_rank) */
    double* low_rank_factors;     /**< Low-rank factors (if low_rank) */
    uint32_t n_params;            /**< Number of parameters */
    double elbo;                  /**< Final ELBO value */
    double* elbo_history;         /**< ELBO over iterations */
    uint32_t n_iterations;        /**< Actual iterations */
    nimcp_vi_config_t config;     /**< Configuration */
    bool fitted;                  /**< Optimization completed */
} nimcp_vi_optimizer_t;

/**
 * @brief Create variational inference optimizer
 * @param n_params Number of parameters
 * @param config Configuration
 * @return New optimizer or NULL
 */
NIMCP_EXPORT nimcp_vi_optimizer_t* nimcp_vi_create(
    uint32_t n_params,
    const nimcp_vi_config_t* config
);

/**
 * @brief Destroy VI optimizer
 * @param vi Optimizer to destroy
 */
NIMCP_EXPORT void nimcp_vi_destroy(nimcp_vi_optimizer_t* vi);

/**
 * @brief Get default VI configuration
 * @param family Variational family
 * @return Default configuration
 */
NIMCP_EXPORT nimcp_vi_config_t nimcp_vi_default_config(nimcp_vi_family_t family);

/**
 * @brief Fit variational approximation
 * @param vi Optimizer
 * @param log_posterior Log posterior function
 * @param grad_log_posterior Gradient function
 * @param initial_mean Initial variational mean
 * @param data User data
 * @return NIMCP_BAYES_OK on success
 *
 * Maximizes ELBO = E_q[log p(θ,y)] - E_q[log q(θ)]
 * using stochastic gradient ascent with reparameterization trick.
 */
NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_vi_fit(
    nimcp_vi_optimizer_t* vi,
    nimcp_log_posterior_fn log_posterior,
    nimcp_log_posterior_grad_fn grad_log_posterior,
    const double* initial_mean,
    void* data
);

/**
 * @brief Get current ELBO value
 * @param vi Fitted optimizer
 * @return ELBO value
 */
NIMCP_EXPORT double nimcp_vi_elbo(const nimcp_vi_optimizer_t* vi);

/**
 * @brief Fit mean-field approximation (convenience wrapper)
 * @param vi Optimizer (must be configured with MEAN_FIELD)
 * @param log_posterior Log posterior function
 * @param grad_log_posterior Gradient function
 * @param initial_mean Initial mean
 * @param data User data
 * @return NIMCP_BAYES_OK on success
 *
 * Fits q(θ) = Π_i N(θ_i | μ_i, σ_i²)
 */
NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_vi_mean_field(
    nimcp_vi_optimizer_t* vi,
    nimcp_log_posterior_fn log_posterior,
    nimcp_log_posterior_grad_fn grad_log_posterior,
    const double* initial_mean,
    void* data
);

/**
 * @brief Sample from variational posterior
 * @param vi Fitted optimizer
 * @param n_samples Number of samples
 * @param samples Output samples (n_samples x n_params, row-major)
 * @return NIMCP_BAYES_OK on success
 */
NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_vi_sample(
    const nimcp_vi_optimizer_t* vi,
    uint32_t n_samples,
    double* samples
);

/**
 * @brief Get variational mean
 * @param vi Fitted optimizer
 * @param mean Output mean (n_params x 1)
 * @return NIMCP_BAYES_OK on success
 */
NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_vi_get_mean(
    const nimcp_vi_optimizer_t* vi,
    double* mean
);

/**
 * @brief Get variational variance
 * @param vi Fitted optimizer
 * @param variance Output variance (n_params x 1 for mean-field)
 * @return NIMCP_BAYES_OK on success
 */
NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_vi_get_variance(
    const nimcp_vi_optimizer_t* vi,
    double* variance
);

//=============================================================================
// Hierarchical Models
//=============================================================================

/**
 * @brief Hierarchical level specification
 */
typedef struct nimcp_hier_level {
    char* name;                   /**< Level name (e.g., "subject", "group") */
    uint32_t n_units;             /**< Number of units at this level */
    uint32_t* parent_idx;         /**< Parent unit index for each unit (n_units x 1) */
    uint32_t n_params;            /**< Parameters per unit */
    double* prior_mean;           /**< Prior mean for unit parameters */
    double* prior_precision;      /**< Prior precision (can be hyperparameter) */
    bool estimate_hyperparams;    /**< Estimate hyperparameters from data */
} nimcp_hier_level_t;

/**
 * @brief Hierarchical model
 */
typedef struct nimcp_hier_model {
    nimcp_hier_level_t* levels;   /**< Array of levels (lowest to highest) */
    uint32_t n_levels;            /**< Number of levels */
    double* hyperparams;          /**< Hyperparameters (population level) */
    uint32_t n_hyperparams;       /**< Number of hyperparameters */
    double* random_effects;       /**< Random effects (all levels combined) */
    uint32_t n_random_effects;    /**< Total random effects */
    double* variance_components;  /**< Variance at each level */
    nimcp_mcmc_sampler_t* sampler;/**< Internal MCMC sampler */
    bool fitted;                  /**< Model fitted */
} nimcp_hier_model_t;

/**
 * @brief Create hierarchical model
 * @param n_levels Number of hierarchy levels
 * @return New model or NULL
 */
NIMCP_EXPORT nimcp_hier_model_t* nimcp_hier_create(uint32_t n_levels);

/**
 * @brief Destroy hierarchical model
 * @param hier Model to destroy
 */
NIMCP_EXPORT void nimcp_hier_destroy(nimcp_hier_model_t* hier);

/**
 * @brief Define a hierarchical level
 * @param hier Model
 * @param level_idx Level index (0 = lowest)
 * @param spec Level specification
 * @return NIMCP_BAYES_OK on success
 *
 * Levels are defined from lowest (observations) to highest (population).
 */
NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_hier_define_level(
    nimcp_hier_model_t* hier,
    uint32_t level_idx,
    const nimcp_hier_level_t* spec
);

/**
 * @brief Fit hierarchical model
 * @param hier Model
 * @param y Response data (n_obs x 1)
 * @param X Covariate matrix (n_obs x p)
 * @param n_obs Number of observations
 * @param unit_idx Unit indices for lowest level (n_obs x 1)
 * @param mcmc_config MCMC configuration for fitting
 * @return NIMCP_BAYES_OK on success
 *
 * Fits the model:
 *   y_i ~ f(X_i β + Z_i u, σ²)
 *   u_j ~ N(μ_g(j), Σ_level)
 * where g(j) maps units to their parents.
 */
NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_hier_fit(
    nimcp_hier_model_t* hier,
    const double* y,
    const double* X,
    uint32_t n_obs,
    const uint32_t* unit_idx,
    const nimcp_mcmc_config_t* mcmc_config
);

/**
 * @brief Predict with hierarchical model
 * @param hier Fitted model
 * @param X_new New covariates (n_new x p)
 * @param n_new Number of new observations
 * @param unit_idx_new Unit indices for new obs (NULL for population average)
 * @param predictions Output predictions (n_new x 1)
 * @param prediction_sd Output prediction SD (n_new x 1, can be NULL)
 * @return NIMCP_BAYES_OK on success
 *
 * If unit_idx_new is NULL, returns population-average predictions.
 * Otherwise uses unit-specific random effects.
 */
NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_hier_predict(
    const nimcp_hier_model_t* hier,
    const double* X_new,
    uint32_t n_new,
    const uint32_t* unit_idx_new,
    double* predictions,
    double* prediction_sd
);

/**
 * @brief Get random effects
 * @param hier Fitted model
 * @param level_idx Level index
 * @param unit_idx Unit index
 * @param effects Output effects (n_params_level x 1)
 * @param effects_sd Output standard deviations (can be NULL)
 * @return NIMCP_BAYES_OK on success
 */
NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_hier_random_effects(
    const nimcp_hier_model_t* hier,
    uint32_t level_idx,
    uint32_t unit_idx,
    double* effects,
    double* effects_sd
);

//=============================================================================
// Model Comparison
//=============================================================================

/**
 * @brief Compute Bayes factor between two models
 * @param log_ml_model1 Log marginal likelihood of model 1
 * @param log_ml_model2 Log marginal likelihood of model 2
 * @return Bayes factor BF₁₂ = p(data|M1) / p(data|M2)
 *
 * Interpretation:
 *   BF > 100:  Decisive evidence for M1
 *   30-100:    Very strong evidence
 *   10-30:     Strong evidence
 *   3-10:      Moderate evidence
 *   1-3:       Weak evidence
 *   < 1:       Evidence for M2
 */
NIMCP_EXPORT double nimcp_bayes_factor(double log_ml_model1, double log_ml_model2);

/**
 * @brief Compute WAIC (Widely Applicable Information Criterion)
 * @param mcmc Fitted MCMC sampler
 * @param log_likelihood_fn Function computing log p(y|θ) for each observation
 * @param y Observed data
 * @param n_obs Number of observations
 * @param data User data
 * @param waic Output WAIC
 * @param p_waic Output effective number of parameters
 * @param se Output standard error
 * @return NIMCP_BAYES_OK on success
 *
 * WAIC = -2(lppd - p_waic) where lppd is log pointwise predictive density
 * Lower is better. Comparable to AIC/BIC but fully Bayesian.
 */
typedef double (*nimcp_log_likelihood_obs_fn)(
    const double* params,
    uint32_t n_params,
    uint32_t obs_idx,
    const double* y,
    void* data
);

NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_waic(
    const nimcp_mcmc_sampler_t* mcmc,
    nimcp_log_likelihood_obs_fn log_likelihood_fn,
    const double* y,
    uint32_t n_obs,
    void* data,
    double* waic,
    double* p_waic,
    double* se
);

/**
 * @brief Compute LOO-CV (Leave-One-Out Cross-Validation) via PSIS
 * @param mcmc Fitted MCMC sampler
 * @param log_likelihood_fn Per-observation log-likelihood function
 * @param y Observed data
 * @param n_obs Number of observations
 * @param data User data
 * @param loo_cv Output LOO-CV score (elpd_loo)
 * @param p_loo Output effective parameters
 * @param se Output standard error
 * @param k_pareto Output Pareto k diagnostics (n_obs x 1, can be NULL)
 * @return NIMCP_BAYES_OK on success
 *
 * Uses Pareto Smoothed Importance Sampling (PSIS) for efficiency.
 * k > 0.7 indicates unreliable estimate for that observation.
 */
NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_loo_cv(
    const nimcp_mcmc_sampler_t* mcmc,
    nimcp_log_likelihood_obs_fn log_likelihood_fn,
    const double* y,
    uint32_t n_obs,
    void* data,
    double* loo_cv,
    double* p_loo,
    double* se,
    double* k_pareto
);

/**
 * @brief Compute DIC (Deviance Information Criterion)
 * @param mcmc Fitted MCMC sampler
 * @param log_likelihood_fn Full log-likelihood function
 * @param y Observed data
 * @param data User data
 * @param dic Output DIC
 * @param p_dic Output effective parameters
 * @return NIMCP_BAYES_OK on success
 *
 * DIC = D(θ_bar) + 2*p_DIC
 * where D = -2*log p(y|θ) and θ_bar is posterior mean.
 */
typedef double (*nimcp_log_likelihood_full_fn)(
    const double* params,
    uint32_t n_params,
    const double* y,
    uint32_t n_obs,
    void* data
);

NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_dic(
    const nimcp_mcmc_sampler_t* mcmc,
    nimcp_log_likelihood_full_fn log_likelihood_fn,
    const double* y,
    uint32_t n_obs,
    void* data,
    double* dic,
    double* p_dic
);

//=============================================================================
// Bayesian Regression
//=============================================================================

/**
 * @brief Bayesian linear regression model
 */
typedef struct nimcp_bayes_linreg {
    double* beta_mean;            /**< Posterior mean of coefficients */
    double* beta_cov;             /**< Posterior covariance (p x p) */
    double sigma_mean;            /**< Posterior mean of error SD */
    double sigma_var;             /**< Posterior variance of error SD */
    double* beta_samples;         /**< MCMC samples if fitted with MCMC */
    double* sigma_samples;        /**< Sigma samples */
    uint32_t n_samples;           /**< Number of samples */
    uint32_t n_predictors;        /**< Number of predictors (including intercept) */
    double r_squared;             /**< Bayesian R² */
    double log_marginal_like;     /**< Log marginal likelihood */
    bool fitted;                  /**< Model fitted */
} nimcp_bayes_linreg_t;

/**
 * @brief Fit Bayesian linear regression
 * @param X Design matrix (n x p, row-major, first column = 1 for intercept)
 * @param y Response vector (n x 1)
 * @param n Number of observations
 * @param p Number of predictors
 * @param prior_mean Prior mean for coefficients (p x 1, NULL for zeros)
 * @param prior_precision Prior precision matrix (p x p, NULL for diffuse)
 * @param sigma_alpha Prior shape for σ² (Gamma)
 * @param sigma_beta Prior rate for σ²
 * @param model Output model
 * @return NIMCP_BAYES_OK on success
 *
 * Model:
 *   y ~ N(Xβ, σ²I)
 *   β ~ N(μ₀, Σ₀)
 *   σ² ~ InvGamma(α, β)
 *
 * Uses conjugate updates for closed-form posterior when possible.
 */
NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_bayes_linreg_fit(
    const double* X,
    const double* y,
    uint32_t n,
    uint32_t p,
    const double* prior_mean,
    const double* prior_precision,
    double sigma_alpha,
    double sigma_beta,
    nimcp_bayes_linreg_t* model
);

/**
 * @brief Predict from Bayesian linear regression
 * @param model Fitted model
 * @param X_new New design matrix (n_new x p)
 * @param n_new Number of new observations
 * @param predictions Output posterior predictive mean (n_new x 1)
 * @param prediction_sd Output prediction SD (n_new x 1, can be NULL)
 * @param credible_lower Output lower 95% credible bound (can be NULL)
 * @param credible_upper Output upper 95% credible bound (can be NULL)
 * @return NIMCP_BAYES_OK on success
 *
 * Returns posterior predictive distribution:
 *   y_new | y ~ t(ν, X_new β̂, s² (1 + X_new (X'X)⁻¹ X_new'))
 */
NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_bayes_linreg_predict(
    const nimcp_bayes_linreg_t* model,
    const double* X_new,
    uint32_t n_new,
    double* predictions,
    double* prediction_sd,
    double* credible_lower,
    double* credible_upper
);

/**
 * @brief Free Bayesian linear regression model
 * @param model Model to free
 */
NIMCP_EXPORT void nimcp_bayes_linreg_free(nimcp_bayes_linreg_t* model);

/**
 * @brief Bayesian logistic regression model
 */
typedef struct nimcp_bayes_logreg {
    double* beta_mean;            /**< Posterior mean of coefficients */
    double* beta_cov;             /**< Posterior covariance (p x p) */
    double* beta_samples;         /**< MCMC samples */
    uint32_t n_samples;           /**< Number of samples */
    uint32_t n_predictors;        /**< Number of predictors */
    double log_marginal_like;     /**< Approximate log marginal likelihood */
    nimcp_mcmc_sampler_t* sampler;/**< Internal MCMC sampler */
    bool fitted;                  /**< Model fitted */
} nimcp_bayes_logreg_t;

/**
 * @brief Fit Bayesian logistic regression
 * @param X Design matrix (n x p)
 * @param y Binary response (n x 1, 0 or 1)
 * @param n Number of observations
 * @param p Number of predictors
 * @param prior_mean Prior mean (NULL for zeros)
 * @param prior_precision Prior precision (NULL for diffuse)
 * @param mcmc_config MCMC configuration
 * @param model Output model
 * @return NIMCP_BAYES_OK on success
 *
 * Model:
 *   y_i ~ Bernoulli(p_i)
 *   logit(p_i) = X_i β
 *   β ~ N(μ₀, Σ₀)
 *
 * Uses Polya-Gamma augmentation for efficient Gibbs sampling,
 * or NUTS for general sampling.
 */
NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_bayes_logreg_fit(
    const double* X,
    const uint8_t* y,
    uint32_t n,
    uint32_t p,
    const double* prior_mean,
    const double* prior_precision,
    const nimcp_mcmc_config_t* mcmc_config,
    nimcp_bayes_logreg_t* model
);

/**
 * @brief Predict probabilities from Bayesian logistic regression
 * @param model Fitted model
 * @param X_new New design matrix
 * @param n_new Number of new observations
 * @param prob_mean Output posterior mean probability (n_new x 1)
 * @param prob_sd Output probability SD (can be NULL)
 * @param credible_lower Output lower 95% credible (can be NULL)
 * @param credible_upper Output upper 95% credible (can be NULL)
 * @return NIMCP_BAYES_OK on success
 */
NIMCP_EXPORT nimcp_bayes_adv_result_t nimcp_bayes_logreg_predict(
    const nimcp_bayes_logreg_t* model,
    const double* X_new,
    uint32_t n_new,
    double* prob_mean,
    double* prob_sd,
    double* credible_lower,
    double* credible_upper
);

/**
 * @brief Free Bayesian logistic regression model
 * @param model Model to free
 */
NIMCP_EXPORT void nimcp_bayes_logreg_free(nimcp_bayes_logreg_t* model);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error message
 * @param result Result code
 * @return Human-readable message
 */
NIMCP_EXPORT const char* nimcp_bayes_adv_error_string(nimcp_bayes_adv_result_t result);

/**
 * @brief Check if GPU is available for Bayesian methods
 * @return true if GPU available
 */
NIMCP_EXPORT bool nimcp_bayes_adv_gpu_available(void);

/**
 * @brief Set random seed for reproducibility
 * @param seed Seed value (0 for random)
 */
NIMCP_EXPORT void nimcp_bayes_adv_set_seed(uint32_t seed);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BAYESIAN_ADVANCED_H
