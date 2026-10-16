/**
 * @file nimcp_quantum_math_engine.h
 * @brief Quantum Mathematical Engine
 *
 * Provides quantum-enhanced mathematical computations including:
 * - Quantum Monte Carlo integration
 * - Partition function estimation
 * - Path integral simulation
 * - Expectation estimation
 *
 * Builds on nimcp_quantum_monte_carlo.h with mathematical function
 * evaluation and advanced sampling techniques.
 *
 * Biological Basis:
 * - Quantum effects in neural microtubules (theoretical)
 * - Probabilistic neural computation
 * - Stochastic resonance in neural systems
 *
 * @author NIMCP Team
 * @version 2.6.3
 */

#ifndef NIMCP_QUANTUM_MATH_ENGINE_H
#define NIMCP_QUANTUM_MATH_ENGINE_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/quantum/nimcp_quantum_monte_carlo.h"
#include "optimization/quantum_annealing/nimcp_quantum_annealing.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_async.h"
#include "utils/exception/nimcp_exception_macros.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Module Constants
 * ============================================================================ */

/** Bio-async module identifier */
#define BIO_MODULE_QUANTUM_MATH_ENGINE  0x0398

/** Default number of samples */
#define QME_DEFAULT_SAMPLES             10000

/** Default burn-in period */
#define QME_DEFAULT_BURNIN              500

/** Default thinning interval */
#define QME_DEFAULT_THINNING            10

/** Maximum path integral paths */
#define QME_MAX_PATHS                   10000

/* ============================================================================
 * Function Types
 * ============================================================================ */

/**
 * @brief Mathematical function type
 */
typedef float (*qme_math_function_t)(
    const float* x,
    uint32_t dim,
    void* user_data);

/**
 * @brief Distribution function type (for sampling)
 */
typedef float (*qme_distribution_t)(
    const float* x,
    uint32_t dim,
    void* user_data);

/**
 * @brief Action functional type (for path integrals)
 */
typedef float (*qme_action_t)(
    const float* path,
    uint32_t num_points,
    uint32_t dim,
    void* user_data);

/* ============================================================================
 * Integration Domain
 * ============================================================================ */

/**
 * @brief Integration domain type
 */
typedef enum {
    QME_DOMAIN_BOX = 0,              /**< Rectangular box domain */
    QME_DOMAIN_BALL,                 /**< Ball/sphere domain */
    QME_DOMAIN_SIMPLEX,              /**< Simplex domain */
    QME_DOMAIN_CUSTOM                /**< Custom domain indicator */
} qme_domain_type_t;

/**
 * @brief Integration domain specification
 */
typedef struct qme_domain {
    qme_domain_type_t type;          /**< Domain type */
    uint32_t dim;                    /**< Dimensionality */
    float* lower_bounds;             /**< Lower bounds per dimension */
    float* upper_bounds;             /**< Upper bounds per dimension */
    float center[16];                /**< Center (for ball) */
    float radius;                    /**< Radius (for ball) */
} qme_domain_t;

/* ============================================================================
 * Simulation Configuration
 * ============================================================================ */

/**
 * @brief Configuration for QMC simulation
 */
typedef struct qme_simulation_config {
    /* Sampling parameters */
    uint32_t num_samples;            /**< Number of samples */
    uint32_t burnin;                 /**< Burn-in period */
    uint32_t thinning;               /**< Thinning interval */
    float target_acceptance;         /**< Target acceptance rate */

    /* Importance sampling */
    bool enable_importance_sampling; /**< Use importance sampling */
    qme_distribution_t proposal;     /**< Proposal distribution */
    qme_distribution_t target;       /**< Target distribution */

    /* Variance reduction */
    bool enable_antithetic;          /**< Use antithetic variates */
    bool enable_control_variates;    /**< Use control variates */
    bool enable_stratified;          /**< Use stratified sampling */
    uint32_t num_strata;             /**< Number of strata */

    /* Adaptive proposals */
    bool enable_adaptive_proposals;  /**< Adapt proposal distribution */
    float proposal_adaptation_rate;  /**< Adaptation learning rate */
    uint32_t adaptation_interval;    /**< Steps between adaptations */

    /* Random seed */
    uint32_t seed;                   /**< RNG seed (0 = time-based) */
} qme_simulation_config_t;

/* ============================================================================
 * Result Structures
 * ============================================================================ */

/**
 * @brief Integration result
 */
typedef struct qme_integration_result {
    float value;                     /**< Estimated integral value */
    float variance;                  /**< Variance estimate */
    float std_error;                 /**< Standard error */
    float relative_error;            /**< Relative error estimate */
    uint32_t samples_used;           /**< Actual samples used */
    float acceptance_rate;           /**< MCMC acceptance rate */
    uint64_t computation_time_us;    /**< Computation time */
} qme_integration_result_t;

/**
 * @brief Expectation result
 */
typedef struct qme_expectation_result {
    float mean;                      /**< E[f(X)] estimate */
    float variance;                  /**< Var[f(X)] estimate */
    float std_error;                 /**< Standard error of mean */
    float* moment_estimates;         /**< Higher moment estimates */
    uint32_t num_moments;            /**< Number of moments computed */
    float effective_sample_size;     /**< ESS for importance sampling */
} qme_expectation_result_t;

/**
 * @brief Partition function result
 */
typedef struct qme_partition_result {
    float log_Z;                     /**< log(Z) estimate */
    float Z;                         /**< Z estimate (if not overflow) */
    float free_energy;               /**< F = -T * log(Z) */
    float entropy;                   /**< S = (E - F) / T */
    float mean_energy;               /**< <E> estimate */
    float heat_capacity;             /**< C = Var(E) / T^2 */
    float std_error;                 /**< Standard error of log(Z) */
} qme_partition_result_t;

/**
 * @brief Path integral result
 */
typedef struct qme_path_integral_result {
    float value;                     /**< Path integral value */
    float variance;                  /**< Variance */
    float* dominant_path;            /**< Most probable path */
    uint32_t path_length;            /**< Path discretization */
    float action_at_dominant;        /**< Action of dominant path */
    uint32_t paths_sampled;          /**< Number of paths sampled */
} qme_path_integral_result_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Statistics for quantum math engine
 */
typedef struct qme_stats {
    uint64_t integrations_performed; /**< Total integrations */
    uint64_t expectations_computed;  /**< Total expectations */
    uint64_t path_integrals_computed; /**< Total path integrals */
    uint64_t total_samples;          /**< Total samples generated */
    float avg_acceptance_rate;       /**< Average acceptance rate */
    uint64_t total_time_us;          /**< Total computation time */
    float avg_relative_error;        /**< Average relative error */
} qme_stats_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

/**
 * @brief Opaque handle to quantum math engine
 */
typedef struct qme_math_simulation qme_math_simulation_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Create quantum math engine
 *
 * @param config Configuration (NULL for defaults)
 * @return Engine handle or NULL on failure
 */
NIMCP_API qme_math_simulation_t* qme_math_create(
    const qme_simulation_config_t* config);

/**
 * @brief Destroy quantum math engine
 *
 * @param sim Engine to destroy
 */
NIMCP_API void qme_math_destroy(qme_math_simulation_t* sim);

/**
 * @brief Reset engine state
 *
 * @param sim The engine
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t qme_math_reset(qme_math_simulation_t* sim);

/**
 * @brief Get default configuration
 *
 * @param config Configuration to fill
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t qme_math_get_default_config(
    qme_simulation_config_t* config);

/* ============================================================================
 * Monte Carlo Integration
 * ============================================================================ */

/**
 * @brief Integrate function over domain
 *
 * Computes ∫_D f(x) dx using Monte Carlo sampling.
 *
 * @param sim The engine
 * @param f Function to integrate
 * @param domain Integration domain
 * @param tolerance Desired relative error
 * @param result Output result
 * @param user_data Passed to function
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t qme_integrate(
    qme_math_simulation_t* sim,
    qme_math_function_t f,
    const qme_domain_t* domain,
    float tolerance,
    qme_integration_result_t* result,
    void* user_data);

/**
 * @brief Integrate with importance sampling
 *
 * Uses proposal distribution for variance reduction.
 *
 * @param sim The engine
 * @param f Function to integrate
 * @param proposal Proposal distribution
 * @param domain Integration domain
 * @param result Output result
 * @param user_data Passed to functions
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t qme_integrate_importance(
    qme_math_simulation_t* sim,
    qme_math_function_t f,
    qme_distribution_t proposal,
    const qme_domain_t* domain,
    qme_integration_result_t* result,
    void* user_data);

/* ============================================================================
 * Expectation Estimation
 * ============================================================================ */

/**
 * @brief Estimate expectation E[f(X)] where X ~ distribution
 *
 * @param sim The engine
 * @param f Function to compute expectation of
 * @param distribution Probability distribution
 * @param dim Dimensionality
 * @param result Output result
 * @param user_data Passed to functions
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t qme_estimate_expectation(
    qme_math_simulation_t* sim,
    qme_math_function_t f,
    qme_distribution_t distribution,
    uint32_t dim,
    qme_expectation_result_t* result,
    void* user_data);

/**
 * @brief Estimate multiple moments
 *
 * @param sim The engine
 * @param f Function
 * @param distribution Probability distribution
 * @param dim Dimensionality
 * @param max_moment Maximum moment to compute
 * @param result Output result
 * @param user_data Passed to functions
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t qme_estimate_moments(
    qme_math_simulation_t* sim,
    qme_math_function_t f,
    qme_distribution_t distribution,
    uint32_t dim,
    uint32_t max_moment,
    qme_expectation_result_t* result,
    void* user_data);

/* ============================================================================
 * Partition Function
 * ============================================================================ */

/**
 * @brief Estimate partition function Z = ∫ exp(-βE(x)) dx
 *
 * @param sim The engine
 * @param energy_func Energy function E(x)
 * @param dim Dimensionality
 * @param temperature Temperature T (β = 1/T)
 * @param result Output result
 * @param user_data Passed to energy function
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t qme_partition_function(
    qme_math_simulation_t* sim,
    qme_math_function_t energy_func,
    uint32_t dim,
    float temperature,
    qme_partition_result_t* result,
    void* user_data);

/**
 * @brief Estimate free energy F = -T log(Z)
 *
 * @param sim The engine
 * @param energy_func Energy function
 * @param dim Dimensionality
 * @param temperature Temperature
 * @param free_energy Output free energy
 * @param user_data Passed to energy function
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t qme_free_energy(
    qme_math_simulation_t* sim,
    qme_math_function_t energy_func,
    uint32_t dim,
    float temperature,
    float* free_energy,
    void* user_data);

/* ============================================================================
 * Path Integrals
 * ============================================================================ */

/**
 * @brief Compute Feynman path integral
 *
 * Computes ∫ D[x] exp(-S[x]) where S is the action functional.
 *
 * @param sim The engine
 * @param action Action functional S[x]
 * @param dim Dimensionality of paths
 * @param num_time_steps Discretization of path
 * @param initial Initial point
 * @param final Final point
 * @param result Output result
 * @param user_data Passed to action
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t qme_path_integral(
    qme_math_simulation_t* sim,
    qme_action_t action,
    uint32_t dim,
    uint32_t num_time_steps,
    const float* initial,
    const float* final,
    qme_path_integral_result_t* result,
    void* user_data);

/**
 * @brief Find dominant (classical) path
 *
 * Finds the path that minimizes the action (stationary phase).
 *
 * @param sim The engine
 * @param action Action functional
 * @param dim Dimensionality
 * @param num_time_steps Discretization
 * @param initial Initial point
 * @param final Final point
 * @param path Output path
 * @param user_data Passed to action
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t qme_find_classical_path(
    qme_math_simulation_t* sim,
    qme_action_t action,
    uint32_t dim,
    uint32_t num_time_steps,
    const float* initial,
    const float* final,
    float* path,
    void* user_data);

/* ============================================================================
 * Domain Management
 * ============================================================================ */

/**
 * @brief Create box domain
 *
 * @param dim Dimensionality
 * @param lower Lower bounds
 * @param upper Upper bounds
 * @return Domain or NULL on failure
 */
NIMCP_API qme_domain_t* qme_domain_create_box(
    uint32_t dim,
    const float* lower,
    const float* upper);

/**
 * @brief Create ball domain
 *
 * @param dim Dimensionality
 * @param center Center point
 * @param radius Radius
 * @return Domain or NULL on failure
 */
NIMCP_API qme_domain_t* qme_domain_create_ball(
    uint32_t dim,
    const float* center,
    float radius);

/**
 * @brief Destroy domain
 *
 * @param domain Domain to destroy
 */
NIMCP_API void qme_domain_destroy(qme_domain_t* domain);

/**
 * @brief Compute domain volume
 *
 * @param domain The domain
 * @return Volume
 */
NIMCP_API float qme_domain_volume(const qme_domain_t* domain);

/* ============================================================================
 * Result Management
 * ============================================================================ */

/**
 * @brief Initialize path integral result
 *
 * @param result Result to initialize
 * @param path_length Path discretization
 * @param dim Dimensionality
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t qme_path_integral_result_init(
    qme_path_integral_result_t* result,
    uint32_t path_length,
    uint32_t dim);

/**
 * @brief Clean up path integral result
 *
 * @param result Result to clean up
 */
NIMCP_API void qme_path_integral_result_cleanup(
    qme_path_integral_result_t* result);

/**
 * @brief Initialize expectation result
 *
 * @param result Result to initialize
 * @param max_moments Maximum moments
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t qme_expectation_result_init(
    qme_expectation_result_t* result,
    uint32_t max_moments);

/**
 * @brief Clean up expectation result
 *
 * @param result Result to clean up
 */
NIMCP_API void qme_expectation_result_cleanup(
    qme_expectation_result_t* result);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Register with bio-async router
 *
 * @param sim The engine
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t qme_math_register_bio_async(
    qme_math_simulation_t* sim);

/**
 * @brief Unregister from bio-async router
 *
 * @param sim The engine
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t qme_math_unregister_bio_async(
    qme_math_simulation_t* sim);

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param sim The engine
 * @param stats Output statistics
 * @return NIMCP_OK on success
 */
NIMCP_API nimcp_error_t qme_math_get_stats(
    const qme_math_simulation_t* sim,
    qme_stats_t* stats);

/**
 * @brief Print diagnostics
 *
 * @param sim The engine
 */
NIMCP_API void qme_math_print_diagnostics(const qme_math_simulation_t* sim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_QUANTUM_MATH_ENGINE_H */
