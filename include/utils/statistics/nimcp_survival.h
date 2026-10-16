/**
 * @file nimcp_survival.h
 * @brief Survival Analysis Methods for NIMCP
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Comprehensive survival analysis toolkit including Kaplan-Meier,
 *       Cox proportional hazards, competing risks, and statistical tests
 *
 * WHY:  Neural systems exhibit time-to-event dynamics (synapse formation,
 *       spike timing, cell death, plasticity windows). Survival analysis
 *       provides rigorous statistical methods for analyzing these processes.
 *
 * HOW:  C99 implementation with GPU acceleration, numerical stability,
 *       and integration with NIMCP's immune system for error recovery
 *
 * NEUROSCIENCE FOUNDATION:
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │  Survival Analysis in Neural Systems:                                   │
 * │                                                                         │
 * │  Synaptic Dynamics:                                                     │
 * │    - Time to synapse formation/elimination                              │
 * │    - Plasticity window duration                                         │
 * │    - LTP/LTD decay times                                                │
 * │                                                                         │
 * │  Neural Events:                                                         │
 * │    - Interspike intervals                                               │
 * │    - Time to first spike                                                │
 * │    - Recovery time after refractory period                              │
 * │                                                                         │
 * │  Network Health:                                                        │
 * │    - Time to failure/recovery                                           │
 * │    - Module survival under stress                                       │
 * │    - Competing failure modes (competing risks)                          │
 * │                                                                         │
 * │  The Cox model relates covariates (e.g., calcium levels, activity)      │
 * │  to hazard rates: h(t|X) = h₀(t) exp(βX)                                │
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 * FEATURES:
 * - Kaplan-Meier survival estimation with confidence intervals
 * - Cox proportional hazards model with partial likelihood
 * - Log-rank and Wilcoxon tests for group comparisons
 * - Competing risks analysis (Aalen-Johansen, Fine-Gray)
 * - Schoenfeld residuals for PH assumption testing
 * - GPU acceleration for large datasets
 *
 * PERFORMANCE:
 * - Kaplan-Meier (n=10000): <10ms CPU, <1ms GPU
 * - Cox model (n=10000, p=10): <100ms CPU, <10ms GPU
 * - Memory: O(n) for KM, O(np) for Cox
 *
 * INTEGRATION:
 * - Uses nimcp_statistics for distributions
 * - Throws to immune system on severe errors
 * - Full logging support
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SURVIVAL_H
#define NIMCP_SURVIVAL_H

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

/** Maximum covariates for Cox model */
#define NIMCP_SURVIVAL_MAX_COVARIATES 256

/** Maximum competing events for CIF */
#define NIMCP_SURVIVAL_MAX_EVENTS 32

/** Default confidence level */
#define NIMCP_SURVIVAL_DEFAULT_CONFIDENCE 0.95f

/** Convergence tolerance for iterative methods */
#define NIMCP_SURVIVAL_CONVERGENCE_TOL 1e-8

/** Maximum iterations for Newton-Raphson */
#define NIMCP_SURVIVAL_MAX_ITERATIONS 1000

//=============================================================================
// Error Codes
//=============================================================================

/**
 * @brief Survival analysis result codes
 */
typedef enum nimcp_survival_result {
    NIMCP_SURVIVAL_OK = 0,                /**< Success */
    NIMCP_SURVIVAL_ERROR_NULL = -1,       /**< NULL pointer argument */
    NIMCP_SURVIVAL_ERROR_SIZE = -2,       /**< Invalid size (n=0 or too small) */
    NIMCP_SURVIVAL_ERROR_MEMORY = -3,     /**< Memory allocation failed */
    NIMCP_SURVIVAL_ERROR_PARAMS = -4,     /**< Invalid parameters */
    NIMCP_SURVIVAL_ERROR_CONVERGE = -5,   /**< Algorithm did not converge */
    NIMCP_SURVIVAL_ERROR_SINGULAR = -6,   /**< Singular matrix */
    NIMCP_SURVIVAL_ERROR_NO_EVENTS = -7,  /**< No events observed */
    NIMCP_SURVIVAL_ERROR_PH_VIOLATED = -8,/**< Proportional hazards violated */
    NIMCP_SURVIVAL_ERROR_GPU = -9,        /**< GPU computation error */
    NIMCP_SURVIVAL_ERROR_NOT_FIT = -10    /**< Model not fitted */
} nimcp_survival_result_t;

//=============================================================================
// Survival Data Types
//=============================================================================

/**
 * @brief Individual survival observation
 */
typedef struct nimcp_survival_obs {
    float time;           /**< Observed time (event or censoring) */
    bool event;           /**< true if event occurred, false if censored */
    uint8_t event_type;   /**< Event type for competing risks (0 = censored) */
    uint32_t group;       /**< Group membership for stratified analysis */
} nimcp_survival_obs_t;

/**
 * @brief Survival dataset
 */
typedef struct nimcp_survival_data {
    nimcp_survival_obs_t* observations;  /**< Array of observations */
    uint32_t n;                          /**< Number of observations */
    float* covariates;                   /**< Covariate matrix (n x p, row-major) */
    uint32_t n_covariates;               /**< Number of covariates (p) */
    char** covariate_names;              /**< Optional covariate names */
    uint32_t n_groups;                   /**< Number of groups for stratification */
    uint32_t n_event_types;              /**< Number of event types (1 for standard) */
} nimcp_survival_data_t;

//=============================================================================
// Kaplan-Meier Estimator
//=============================================================================

/**
 * @brief Kaplan-Meier survival point estimate
 */
typedef struct nimcp_km_point {
    float time;           /**< Time point */
    float survival;       /**< S(t) estimate */
    float std_error;      /**< Standard error (Greenwood) */
    float ci_lower;       /**< Lower confidence bound */
    float ci_upper;       /**< Upper confidence bound */
    uint32_t n_at_risk;   /**< Number at risk */
    uint32_t n_events;    /**< Number of events at this time */
    uint32_t n_censored;  /**< Number censored at this time */
} nimcp_km_point_t;

/**
 * @brief Kaplan-Meier estimator state
 */
typedef struct nimcp_km_estimator {
    nimcp_km_point_t* curve;     /**< Survival curve points */
    uint32_t n_points;           /**< Number of unique time points */
    uint32_t n_total;            /**< Total observations */
    uint32_t n_events;           /**< Total events */
    uint32_t n_censored;         /**< Total censored */
    float median_survival;       /**< Median survival time (NAN if not reached) */
    float median_ci_lower;       /**< Lower CI for median */
    float median_ci_upper;       /**< Upper CI for median */
    float restricted_mean;       /**< Restricted mean survival time */
    float confidence_level;      /**< Confidence level used */
    bool fitted;                 /**< Whether estimator has been fitted */
} nimcp_km_estimator_t;

/**
 * @brief Create Kaplan-Meier estimator
 * @return New estimator or NULL on failure
 */
NIMCP_EXPORT nimcp_km_estimator_t* nimcp_km_create(void);

/**
 * @brief Destroy Kaplan-Meier estimator
 * @param km Estimator to destroy
 */
NIMCP_EXPORT void nimcp_km_destroy(nimcp_km_estimator_t* km);

/**
 * @brief Fit Kaplan-Meier estimator to data
 * @param km Estimator
 * @param times Event/censoring times (n x 1)
 * @param events Event indicators (true = event, false = censored)
 * @param n Number of observations
 * @param confidence Confidence level for intervals (e.g., 0.95)
 * @return NIMCP_SURVIVAL_OK on success
 *
 * Computes the Kaplan-Meier product-limit estimator:
 *   S(t) = Π_{t_i ≤ t} (1 - d_i / n_i)
 *
 * where d_i = events at time t_i, n_i = at risk just before t_i.
 */
NIMCP_EXPORT nimcp_survival_result_t nimcp_km_fit(
    nimcp_km_estimator_t* km,
    const float* times,
    const bool* events,
    uint32_t n,
    float confidence
);

/**
 * @brief Get survival function value at specified time
 * @param km Fitted estimator
 * @param t Time point
 * @return S(t) survival probability, or NAN if not fitted
 */
NIMCP_EXPORT float nimcp_km_survival_at(
    const nimcp_km_estimator_t* km,
    float t
);

/**
 * @brief Evaluate survival function at multiple times
 * @param km Fitted estimator
 * @param times Time points to evaluate (n_times x 1)
 * @param n_times Number of time points
 * @param survival Output survival values (n_times x 1)
 * @return NIMCP_SURVIVAL_OK on success
 */
NIMCP_EXPORT nimcp_survival_result_t nimcp_km_survival_function(
    const nimcp_km_estimator_t* km,
    const float* times,
    uint32_t n_times,
    float* survival
);

/**
 * @brief Get median survival time
 * @param km Fitted estimator
 * @return Median survival time, or NAN if median not reached
 */
NIMCP_EXPORT float nimcp_km_median_survival(const nimcp_km_estimator_t* km);

/**
 * @brief Get confidence interval at specified time
 * @param km Fitted estimator
 * @param t Time point
 * @param ci_lower Output lower bound
 * @param ci_upper Output upper bound
 * @return NIMCP_SURVIVAL_OK on success
 */
NIMCP_EXPORT nimcp_survival_result_t nimcp_km_confidence_interval(
    const nimcp_km_estimator_t* km,
    float t,
    float* ci_lower,
    float* ci_upper
);

/**
 * @brief Get data for plotting survival curve
 * @param km Fitted estimator
 * @param times Output time array (n_points x 1)
 * @param survival Output survival array
 * @param ci_lower Output lower CI (can be NULL)
 * @param ci_upper Output upper CI (can be NULL)
 * @param n_points Output number of points
 * @return NIMCP_SURVIVAL_OK on success
 *
 * Returns step-function data suitable for plotting.
 */
NIMCP_EXPORT nimcp_survival_result_t nimcp_km_plot_data(
    const nimcp_km_estimator_t* km,
    float** times,
    float** survival,
    float** ci_lower,
    float** ci_upper,
    uint32_t* n_points
);

/**
 * @brief Get restricted mean survival time (RMST)
 * @param km Fitted estimator
 * @param tau Restriction time
 * @param rmst Output RMST
 * @param std_error Output standard error
 * @return NIMCP_SURVIVAL_OK on success
 *
 * RMST = ∫₀^τ S(t) dt
 */
NIMCP_EXPORT nimcp_survival_result_t nimcp_km_rmst(
    const nimcp_km_estimator_t* km,
    float tau,
    float* rmst,
    float* std_error
);

//=============================================================================
// Cox Proportional Hazards Model
//=============================================================================

/**
 * @brief Cox model coefficient with statistics
 */
typedef struct nimcp_cox_coefficient {
    float beta;           /**< Coefficient estimate */
    float std_error;      /**< Standard error */
    float hazard_ratio;   /**< exp(beta) */
    float hr_ci_lower;    /**< HR lower CI */
    float hr_ci_upper;    /**< HR upper CI */
    float z_stat;         /**< Z statistic */
    float p_value;        /**< P-value */
    const char* name;     /**< Covariate name (if provided) */
} nimcp_cox_coefficient_t;

/**
 * @brief Cox model fit statistics
 */
typedef struct nimcp_cox_stats {
    float log_likelihood;       /**< Partial log-likelihood at convergence */
    float log_likelihood_null;  /**< Log-likelihood of null model */
    float concordance;          /**< C-index (concordance) */
    float concordance_se;       /**< C-index standard error */
    float likelihood_ratio;     /**< Likelihood ratio test statistic */
    float lr_p_value;           /**< LR test p-value */
    float wald;                 /**< Wald test statistic */
    float wald_p_value;         /**< Wald test p-value */
    float score;                /**< Score (log-rank) test statistic */
    float score_p_value;        /**< Score test p-value */
    float aic;                  /**< Akaike Information Criterion */
    float bic;                  /**< Bayesian Information Criterion */
    uint32_t n_iterations;      /**< Iterations to converge */
    bool converged;             /**< Convergence achieved */
} nimcp_cox_stats_t;

/**
 * @brief Cox proportional hazards model
 */
typedef struct nimcp_cox_model {
    nimcp_cox_coefficient_t* coefficients;  /**< Fitted coefficients */
    uint32_t n_covariates;                   /**< Number of covariates */
    nimcp_cox_stats_t stats;                 /**< Model statistics */
    float* baseline_hazard;                  /**< Baseline hazard at event times */
    float* baseline_times;                   /**< Event times for baseline */
    uint32_t n_baseline;                     /**< Number of baseline points */
    float* variance_matrix;                  /**< Variance-covariance matrix (p x p) */
    float confidence_level;                  /**< Confidence level */
    bool fitted;                             /**< Model fitted */
    bool ties_breslow;                       /**< Use Breslow (true) or Efron (false) */
} nimcp_cox_model_t;

/**
 * @brief Create Cox model
 * @param n_covariates Number of covariates
 * @return New model or NULL on failure
 */
NIMCP_EXPORT nimcp_cox_model_t* nimcp_cox_create(uint32_t n_covariates);

/**
 * @brief Destroy Cox model
 * @param cox Model to destroy
 */
NIMCP_EXPORT void nimcp_cox_destroy(nimcp_cox_model_t* cox);

/**
 * @brief Fit Cox model to data
 * @param cox Model
 * @param times Event/censoring times (n x 1)
 * @param events Event indicators
 * @param covariates Covariate matrix (n x p, row-major)
 * @param n Number of observations
 * @param covariate_names Optional names (can be NULL)
 * @param confidence Confidence level
 * @return NIMCP_SURVIVAL_OK on success
 *
 * Fits model via Newton-Raphson maximization of partial likelihood:
 *   L(β) = Π_{i: event} [exp(β'Xᵢ) / Σ_{j∈R(tᵢ)} exp(β'Xⱼ)]
 *
 * Uses Breslow method for tied event times.
 */
NIMCP_EXPORT nimcp_survival_result_t nimcp_cox_fit(
    nimcp_cox_model_t* cox,
    const float* times,
    const bool* events,
    const float* covariates,
    uint32_t n,
    const char** covariate_names,
    float confidence
);

/**
 * @brief Fit Cox model with GPU acceleration
 * @param cox Model
 * @param times Event/censoring times
 * @param events Event indicators
 * @param covariates Covariate matrix
 * @param n Number of observations
 * @param covariate_names Optional names
 * @param confidence Confidence level
 * @return NIMCP_SURVIVAL_OK on success
 */
NIMCP_EXPORT nimcp_survival_result_t nimcp_cox_fit_gpu(
    nimcp_cox_model_t* cox,
    const float* times,
    const bool* events,
    const float* covariates,
    uint32_t n,
    const char** covariate_names,
    float confidence
);

/**
 * @brief Predict hazard ratio for new observations
 * @param cox Fitted model
 * @param covariates New covariate values (n_new x p)
 * @param n_new Number of new observations
 * @param hazard_ratio Output hazard ratios relative to baseline
 * @return NIMCP_SURVIVAL_OK on success
 *
 * Computes HR = exp(β'X) for each observation.
 */
NIMCP_EXPORT nimcp_survival_result_t nimcp_cox_predict_hazard(
    const nimcp_cox_model_t* cox,
    const float* covariates,
    uint32_t n_new,
    float* hazard_ratio
);

/**
 * @brief Predict survival curve for new observation
 * @param cox Fitted model
 * @param covariates Single observation covariates (1 x p)
 * @param times Time points for prediction
 * @param n_times Number of time points
 * @param survival Output survival probabilities
 * @return NIMCP_SURVIVAL_OK on success
 *
 * Computes S(t|X) = S₀(t)^exp(β'X)
 */
NIMCP_EXPORT nimcp_survival_result_t nimcp_cox_predict_survival(
    const nimcp_cox_model_t* cox,
    const float* covariates,
    const float* times,
    uint32_t n_times,
    float* survival
);

/**
 * @brief Get model coefficients
 * @param cox Fitted model
 * @param coefficients Output coefficient array
 * @param n_coefficients Output number of coefficients
 * @return NIMCP_SURVIVAL_OK on success
 */
NIMCP_EXPORT nimcp_survival_result_t nimcp_cox_coefficients(
    const nimcp_cox_model_t* cox,
    nimcp_cox_coefficient_t** coefficients,
    uint32_t* n_coefficients
);

/**
 * @brief Get concordance index (C-statistic)
 * @param cox Fitted model
 * @return C-index in [0.5, 1.0], or NAN if not fitted
 *
 * C-index measures discriminative ability:
 *   C = P(longer survival has lower risk score)
 */
NIMCP_EXPORT float nimcp_cox_concordance(const nimcp_cox_model_t* cox);

/**
 * @brief Get partial log-likelihood
 * @param cox Fitted model
 * @return Log-likelihood value
 */
NIMCP_EXPORT float nimcp_cox_log_likelihood(const nimcp_cox_model_t* cox);

/**
 * @brief Compute Schoenfeld residuals for PH assumption test
 * @param cox Fitted model
 * @param times Original event times
 * @param events Original event indicators
 * @param covariates Original covariates
 * @param n Number of observations
 * @param residuals Output residuals (n_events x p)
 * @param event_times Output event times (n_events x 1)
 * @param n_events Output number of events
 * @return NIMCP_SURVIVAL_OK on success
 *
 * Schoenfeld residuals are used to test the proportional hazards assumption.
 * Plot residuals vs time - correlation indicates PH violation.
 */
NIMCP_EXPORT nimcp_survival_result_t nimcp_cox_schoenfeld_residuals(
    const nimcp_cox_model_t* cox,
    const float* times,
    const bool* events,
    const float* covariates,
    uint32_t n,
    float** residuals,
    float** event_times,
    uint32_t* n_events
);

/**
 * @brief Compute martingale residuals
 * @param cox Fitted model
 * @param times Original times
 * @param events Original events
 * @param covariates Original covariates
 * @param n Number of observations
 * @param residuals Output residuals (n x 1)
 * @return NIMCP_SURVIVAL_OK on success
 *
 * M_i = δ_i - H(t_i|X_i) where H is cumulative hazard.
 * Used for assessing functional form of covariates.
 */
NIMCP_EXPORT nimcp_survival_result_t nimcp_cox_martingale_residuals(
    const nimcp_cox_model_t* cox,
    const float* times,
    const bool* events,
    const float* covariates,
    uint32_t n,
    float* residuals
);

/**
 * @brief Compute deviance residuals
 * @param cox Fitted model
 * @param times Original times
 * @param events Original events
 * @param covariates Original covariates
 * @param n Number of observations
 * @param residuals Output residuals (n x 1)
 * @return NIMCP_SURVIVAL_OK on success
 *
 * d_i = sign(M_i) * sqrt(-2[M_i + δ_i*log(δ_i - M_i)])
 * Should be approximately N(0,1) if model fits well.
 */
NIMCP_EXPORT nimcp_survival_result_t nimcp_cox_deviance_residuals(
    const nimcp_cox_model_t* cox,
    const float* times,
    const bool* events,
    const float* covariates,
    uint32_t n,
    float* residuals
);

//=============================================================================
// Survival Tests
//=============================================================================

/**
 * @brief Log-rank test result
 */
typedef struct nimcp_logrank_result {
    float chi_squared;    /**< Chi-squared statistic */
    float p_value;        /**< P-value */
    float df;             /**< Degrees of freedom */
    uint32_t n_groups;    /**< Number of groups compared */
    float* observed;      /**< Observed events per group */
    float* expected;      /**< Expected events per group */
} nimcp_logrank_result_t;

/**
 * @brief Log-rank test for comparing survival curves
 * @param times Event/censoring times (n x 1)
 * @param events Event indicators
 * @param groups Group assignments (0, 1, 2, ...)
 * @param n Number of observations
 * @param n_groups Number of groups
 * @param result Output test result
 * @return NIMCP_SURVIVAL_OK on success
 *
 * Tests H0: All groups have the same survival distribution.
 * The log-rank test is most powerful when hazards are proportional.
 */
NIMCP_EXPORT nimcp_survival_result_t nimcp_logrank_test(
    const float* times,
    const bool* events,
    const uint32_t* groups,
    uint32_t n,
    uint32_t n_groups,
    nimcp_logrank_result_t* result
);

/**
 * @brief Peto-Peto (Wilcoxon) survival test
 * @param times Event/censoring times
 * @param events Event indicators
 * @param groups Group assignments
 * @param n Number of observations
 * @param n_groups Number of groups
 * @param result Output test result (same structure as log-rank)
 * @return NIMCP_SURVIVAL_OK on success
 *
 * Like log-rank but weights earlier events more heavily.
 * More powerful when survival curves cross or diverge late.
 */
NIMCP_EXPORT nimcp_survival_result_t nimcp_wilcoxon_survival_test(
    const float* times,
    const bool* events,
    const uint32_t* groups,
    uint32_t n,
    uint32_t n_groups,
    nimcp_logrank_result_t* result
);

/**
 * @brief Proportional hazards assumption test result
 */
typedef struct nimcp_ph_test_result {
    float global_chi_squared;   /**< Global test statistic */
    float global_p_value;       /**< Global p-value */
    float* covariate_chi_sq;    /**< Per-covariate chi-squared */
    float* covariate_p_value;   /**< Per-covariate p-values */
    float* covariate_rho;       /**< Correlation of residuals with time */
    uint32_t n_covariates;      /**< Number of covariates */
    bool ph_assumption_holds;   /**< true if PH assumption is reasonable */
} nimcp_ph_test_result_t;

/**
 * @brief Test proportional hazards assumption
 * @param cox Fitted Cox model
 * @param times Original times
 * @param events Original events
 * @param covariates Original covariates
 * @param n Number of observations
 * @param alpha Significance level (e.g., 0.05)
 * @param result Output test result
 * @return NIMCP_SURVIVAL_OK on success
 *
 * Uses scaled Schoenfeld residuals correlated with time.
 * Significant correlation indicates PH violation for that covariate.
 */
NIMCP_EXPORT nimcp_survival_result_t nimcp_ph_test(
    const nimcp_cox_model_t* cox,
    const float* times,
    const bool* events,
    const float* covariates,
    uint32_t n,
    float alpha,
    nimcp_ph_test_result_t* result
);

/**
 * @brief Free PH test result
 * @param result Result to free
 */
NIMCP_EXPORT void nimcp_ph_test_free(nimcp_ph_test_result_t* result);

/**
 * @brief Free log-rank result
 * @param result Result to free
 */
NIMCP_EXPORT void nimcp_logrank_free(nimcp_logrank_result_t* result);

//=============================================================================
// Competing Risks
//=============================================================================

/**
 * @brief Cumulative incidence function (CIF) point
 */
typedef struct nimcp_cif_point {
    float time;           /**< Time point */
    float* cif;           /**< CIF values per event type */
    float* std_error;     /**< Standard errors per event type */
    float* ci_lower;      /**< Lower CI per event type */
    float* ci_upper;      /**< Upper CI per event type */
    uint32_t n_events;    /**< Number of event types */
} nimcp_cif_point_t;

/**
 * @brief Cumulative incidence function estimator
 */
typedef struct nimcp_cif_estimator {
    nimcp_cif_point_t* curve;    /**< CIF curve points */
    uint32_t n_points;           /**< Number of time points */
    uint32_t n_event_types;      /**< Number of competing event types */
    uint32_t n_total;            /**< Total observations */
    uint32_t* n_events;          /**< Events per type */
    uint32_t n_censored;         /**< Total censored */
    float confidence_level;      /**< Confidence level */
    bool fitted;                 /**< Fitted flag */
} nimcp_cif_estimator_t;

/**
 * @brief Create CIF estimator
 * @param n_event_types Number of competing event types
 * @return New estimator or NULL on failure
 */
NIMCP_EXPORT nimcp_cif_estimator_t* nimcp_cif_create(uint32_t n_event_types);

/**
 * @brief Destroy CIF estimator
 * @param cif Estimator to destroy
 */
NIMCP_EXPORT void nimcp_cif_destroy(nimcp_cif_estimator_t* cif);

/**
 * @brief Fit CIF using Aalen-Johansen estimator
 * @param cif Estimator
 * @param times Event/censoring times
 * @param event_types Event type indicators (0 = censored, 1..k = event types)
 * @param n Number of observations
 * @param confidence Confidence level
 * @return NIMCP_SURVIVAL_OK on success
 *
 * The Aalen-Johansen estimator handles competing risks properly:
 *   CIF_k(t) = Σ_{t_i ≤ t} S(t_i⁻) × (d_ik / n_i)
 *
 * where S(t) is the Kaplan-Meier estimate using all event types.
 */
NIMCP_EXPORT nimcp_survival_result_t nimcp_cif_fit(
    nimcp_cif_estimator_t* cif,
    const float* times,
    const uint8_t* event_types,
    uint32_t n,
    float confidence
);

/**
 * @brief Get CIF value at specified time for event type
 * @param cif Fitted estimator
 * @param t Time point
 * @param event_type Event type (1 to n_event_types)
 * @return CIF value, or NAN if not fitted
 */
NIMCP_EXPORT float nimcp_cif_at(
    const nimcp_cif_estimator_t* cif,
    float t,
    uint8_t event_type
);

/**
 * @brief Fine-Gray subdistribution hazard model
 */
typedef struct nimcp_fine_gray_model {
    nimcp_cox_coefficient_t* coefficients;  /**< Fitted coefficients */
    uint32_t n_covariates;                   /**< Number of covariates */
    float* variance_matrix;                  /**< Variance-covariance matrix */
    uint8_t target_event;                    /**< Event type being modeled */
    float confidence_level;                  /**< Confidence level */
    bool fitted;                             /**< Fitted flag */
} nimcp_fine_gray_model_t;

/**
 * @brief Create Fine-Gray model
 * @param n_covariates Number of covariates
 * @param target_event Event type to model
 * @return New model or NULL
 */
NIMCP_EXPORT nimcp_fine_gray_model_t* nimcp_fine_gray_create(
    uint32_t n_covariates,
    uint8_t target_event
);

/**
 * @brief Destroy Fine-Gray model
 * @param fg Model to destroy
 */
NIMCP_EXPORT void nimcp_fine_gray_destroy(nimcp_fine_gray_model_t* fg);

/**
 * @brief Fit Fine-Gray subdistribution hazards model
 * @param fg Model
 * @param times Event/censoring times
 * @param event_types Event type indicators
 * @param covariates Covariate matrix (n x p)
 * @param n Number of observations
 * @param covariate_names Optional names
 * @param confidence Confidence level
 * @return NIMCP_SURVIVAL_OK on success
 *
 * Models the subdistribution hazard of the target event,
 * treating other event types as a distinct form of censoring.
 * Coefficients relate to cumulative incidence, not cause-specific hazard.
 */
NIMCP_EXPORT nimcp_survival_result_t nimcp_fine_gray_fit(
    nimcp_fine_gray_model_t* fg,
    const float* times,
    const uint8_t* event_types,
    const float* covariates,
    uint32_t n,
    const char** covariate_names,
    float confidence
);

/**
 * @brief Predict CIF from Fine-Gray model
 * @param fg Fitted model
 * @param covariates Single observation covariates
 * @param times Time points
 * @param n_times Number of time points
 * @param cif Output CIF values
 * @return NIMCP_SURVIVAL_OK on success
 */
NIMCP_EXPORT nimcp_survival_result_t nimcp_fine_gray_predict(
    const nimcp_fine_gray_model_t* fg,
    const float* covariates,
    const float* times,
    uint32_t n_times,
    float* cif
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Create survival data structure
 * @param n Number of observations
 * @param n_covariates Number of covariates (0 if none)
 * @return New data structure or NULL
 */
NIMCP_EXPORT nimcp_survival_data_t* nimcp_survival_data_create(
    uint32_t n,
    uint32_t n_covariates
);

/**
 * @brief Destroy survival data structure
 * @param data Data to destroy
 */
NIMCP_EXPORT void nimcp_survival_data_destroy(nimcp_survival_data_t* data);

/**
 * @brief Get error message for result code
 * @param result Result code
 * @return Human-readable message
 */
NIMCP_EXPORT const char* nimcp_survival_error_string(nimcp_survival_result_t result);

/**
 * @brief Check if GPU acceleration is available
 * @return true if GPU can be used
 */
NIMCP_EXPORT bool nimcp_survival_gpu_available(void);

/**
 * @brief Enable/disable GPU acceleration
 * @param enable true to enable GPU
 */
NIMCP_EXPORT void nimcp_survival_set_gpu(bool enable);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SURVIVAL_H
