/**
 * @file nimcp_dynamical_systems.h
 * @brief Dynamical Systems Analysis Module - Chaos, attractors, and stability
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Dynamical systems analysis for neural network behavior understanding
 * WHY:  Characterize network dynamics: stability, chaos, attractor states
 * HOW:  Lyapunov exponents, bifurcation analysis, attractor reconstruction
 *
 * KEY CONCEPTS:
 * - Lyapunov Exponents: Measure sensitivity to initial conditions (chaos indicator)
 * - Bifurcation Analysis: Detect qualitative changes in system behavior
 * - Attractor Reconstruction: Rebuild phase space from time series (Takens embedding)
 * - Energy Landscapes: Model neural energy and stable states
 * - Slow-Fast Decomposition: Separate timescales in neural dynamics
 *
 * NIMCP INTEGRATION:
 * - KG Wiring: Registers as physics layer module
 * - Exception Handling: Immune presentation of dynamical anomalies
 * - Bio-Async: Asynchronous analysis computations
 * - Logging: Configurable dynamical systems logging
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_DYNAMICAL_SYSTEMS_H
#define NIMCP_DYNAMICAL_SYSTEMS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "common/nimcp_export.h"
#include "core/brain/nimcp_kg_module_wiring.h"
#include "core/brain/nimcp_brain_kg.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum state dimension */
#define DYNSYS_MAX_STATE_DIM            128

/** Maximum embedding dimension for Takens */
#define DYNSYS_MAX_EMBEDDING_DIM        32

/** Maximum Lyapunov exponents to compute */
#define DYNSYS_MAX_LYAPUNOV             16

/** Maximum bifurcation parameter range points */
#define DYNSYS_MAX_BIFURCATION_POINTS   1024

/** Default integration step */
#define DYNSYS_DEFAULT_DT               0.001f

/** Module name for KG registration */
#define DYNSYS_MODULE_NAME              "dynamical_systems"

/** KG Module ID (0x0460 range) */
#define DYNSYS_KG_MODULE_ID             0x0460

//=============================================================================
// Message Type IDs (0x4600 - 0x466F range)
//=============================================================================

typedef enum {
    /** Lyapunov exponent computation request */
    DYNSYS_MSG_LYAPUNOV_REQUEST         = 0x4600,
    /** Lyapunov result */
    DYNSYS_MSG_LYAPUNOV_RESULT          = 0x4601,
    /** Bifurcation analysis request */
    DYNSYS_MSG_BIFURCATION_REQUEST      = 0x4602,
    /** Bifurcation result */
    DYNSYS_MSG_BIFURCATION_RESULT       = 0x4603,
    /** Attractor reconstruction request */
    DYNSYS_MSG_ATTRACTOR_REQUEST        = 0x4604,
    /** Attractor result */
    DYNSYS_MSG_ATTRACTOR_RESULT         = 0x4605,
    /** Energy landscape request */
    DYNSYS_MSG_ENERGY_REQUEST           = 0x4606,
    /** Energy result */
    DYNSYS_MSG_ENERGY_RESULT            = 0x4607,
    /** Slow-fast decomposition request */
    DYNSYS_MSG_SLOWFAST_REQUEST         = 0x4608,
    /** Slow-fast result */
    DYNSYS_MSG_SLOWFAST_RESULT          = 0x4609,
    /** State update notification */
    DYNSYS_MSG_STATE_UPDATE             = 0x460A,
    /** Error notification */
    DYNSYS_MSG_ERROR                    = 0x460F,

    /** Bridge lifecycle */
    DYNSYS_MSG_BRIDGE_INIT              = 0x4620,
    DYNSYS_MSG_BRIDGE_SHUTDOWN          = 0x4621,
    DYNSYS_MSG_BRIDGE_STATUS            = 0x4622
} dynsys_msg_type_t;

//=============================================================================
// Exception Category
//=============================================================================

/** Exception category for dynamical systems */
#define EXCEPTION_CATEGORY_DYNSYS       0x0046

typedef enum {
    /** Numerical integration diverged */
    DYNSYS_EXC_INTEGRATION_DIVERGED     = 0x4601,
    /** Lyapunov computation failed */
    DYNSYS_EXC_LYAPUNOV_FAILURE         = 0x4602,
    /** Bifurcation detection failed */
    DYNSYS_EXC_BIFURCATION_FAILURE      = 0x4603,
    /** Attractor reconstruction failed */
    DYNSYS_EXC_ATTRACTOR_FAILURE        = 0x4604,
    /** Invalid state dimension */
    DYNSYS_EXC_INVALID_DIMENSION        = 0x4605,
    /** Memory allocation failure */
    DYNSYS_EXC_MEMORY_FAILURE           = 0x4606
} dynsys_exception_t;

//=============================================================================
// Error Codes
//=============================================================================

typedef enum {
    DYNSYS_OK                           = 0,
    DYNSYS_ERR_NULL_PTR                 = -1,
    DYNSYS_ERR_INVALID_DIM              = -2,
    DYNSYS_ERR_DIVERGENCE               = -3,
    DYNSYS_ERR_NOT_INITIALIZED          = -4,
    DYNSYS_ERR_ALREADY_INITIALIZED      = -5,
    DYNSYS_ERR_NO_MEMORY                = -6,
    DYNSYS_ERR_COMPUTATION              = -7,
    DYNSYS_ERR_INVALID_PARAMETER        = -8
} dynsys_error_t;

//=============================================================================
// Bifurcation Types
//=============================================================================

typedef enum {
    BIFURCATION_NONE                    = 0,
    BIFURCATION_SADDLE_NODE,            /**< Fixed point creation/destruction */
    BIFURCATION_TRANSCRITICAL,          /**< Exchange of stability */
    BIFURCATION_PITCHFORK,              /**< Symmetry breaking */
    BIFURCATION_HOPF,                   /**< Limit cycle birth */
    BIFURCATION_PERIOD_DOUBLING,        /**< Period doubling cascade */
    BIFURCATION_NEIMARK_SACKER,         /**< Torus bifurcation */
    BIFURCATION_HOMOCLINIC,             /**< Homoclinic orbit */
    BIFURCATION_HETEROCLINIC            /**< Heteroclinic orbit */
} bifurcation_type_t;

//=============================================================================
// Attractor Types
//=============================================================================

typedef enum {
    ATTRACTOR_UNKNOWN                   = 0,
    ATTRACTOR_FIXED_POINT,              /**< Stable equilibrium */
    ATTRACTOR_LIMIT_CYCLE,              /**< Periodic orbit */
    ATTRACTOR_TORUS,                    /**< Quasi-periodic */
    ATTRACTOR_STRANGE,                  /**< Chaotic attractor */
    ATTRACTOR_CHAOTIC_SADDLE            /**< Transient chaos */
} attractor_type_t;

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct nimcp_brain_struct* nimcp_brain_t;
typedef struct nimcp_bio_router_struct* nimcp_bio_router_t;
typedef struct nimcp_brain_immune_struct* nimcp_brain_immune_t;
typedef struct dynsys_exception_handler_struct* dynsys_exception_handler_t;

//=============================================================================
// Opaque Handle Types
//=============================================================================

typedef struct dynsys_system_struct* dynsys_system_t;
typedef struct dynsys_lyapunov_struct* dynsys_lyapunov_t;
typedef struct dynsys_bifurcation_struct* dynsys_bifurcation_t;
typedef struct dynsys_attractor_struct* dynsys_attractor_t;
typedef struct dynsys_energy_struct* dynsys_energy_t;
typedef struct dynsys_slowfast_struct* dynsys_slowfast_t;
typedef struct dynsys_bridge_struct* dynsys_bridge_t;

//=============================================================================
// Function Pointer Types
//=============================================================================

/**
 * @brief Dynamical system function type
 *
 * Computes dx/dt = f(x, params) for state x
 *
 * @param state Current state vector
 * @param state_dim State dimension
 * @param params Parameter vector
 * @param param_dim Parameter dimension
 * @param derivative Output derivative vector
 * @param context User context
 * @return 0 on success, negative on error
 */
typedef int (*dynsys_func_t)(
    const float* state,
    uint32_t state_dim,
    const float* params,
    uint32_t param_dim,
    float* derivative,
    void* context
);

/**
 * @brief Jacobian function type (optional)
 *
 * Computes J[i][j] = df[i]/dx[j]
 */
typedef int (*dynsys_jacobian_t)(
    const float* state,
    uint32_t state_dim,
    const float* params,
    uint32_t param_dim,
    float* jacobian,
    void* context
);

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Main dynamical system configuration
 */
typedef struct {
    uint32_t state_dim;                 /**< State vector dimension */
    uint32_t param_dim;                 /**< Parameter vector dimension */
    float dt;                           /**< Integration time step */
    float transient_time;               /**< Time to discard transients */
    float analysis_time;                /**< Time for analysis */
    bool enable_logging;                /**< Enable logging */
    bool enable_metrics;                /**< Enable metrics collection */
    bool enable_kg_wiring;              /**< Enable KG registration */
    bool enable_exception_handling;     /**< Enable exception handling */
} dynsys_config_t;

/**
 * @brief Lyapunov exponent configuration
 */
typedef struct {
    uint32_t num_exponents;             /**< Number of exponents to compute */
    float orthonormalization_interval;  /**< Gram-Schmidt interval */
    uint32_t transient_steps;           /**< Steps to discard */
    uint32_t analysis_steps;            /**< Steps for computation */
    float perturbation_size;            /**< Initial perturbation magnitude */
} dynsys_lyapunov_config_t;

/**
 * @brief Bifurcation analysis configuration
 */
typedef struct {
    uint32_t param_index;               /**< Parameter to vary */
    float param_start;                  /**< Parameter range start */
    float param_end;                    /**< Parameter range end */
    uint32_t num_points;                /**< Points in parameter sweep */
    uint32_t transient_steps;           /**< Transient to discard per point */
    uint32_t sample_steps;              /**< Samples to collect per point */
    float tolerance;                    /**< Bifurcation detection tolerance */
} dynsys_bifurcation_config_t;

/**
 * @brief Attractor reconstruction configuration
 */
typedef struct {
    uint32_t embedding_dim;             /**< Embedding dimension */
    uint32_t time_delay;                /**< Time delay (in samples) */
    uint32_t observable_index;          /**< Which state component to use */
    uint32_t num_samples;               /**< Number of samples */
    bool estimate_dimension;            /**< Estimate correlation dimension */
    float epsilon_min;                  /**< Min epsilon for box counting */
    float epsilon_max;                  /**< Max epsilon for box counting */
} dynsys_attractor_config_t;

/**
 * @brief Energy landscape configuration
 */
typedef struct {
    uint32_t grid_resolution;           /**< Grid points per dimension */
    float state_min;                    /**< Minimum state value */
    float state_max;                    /**< Maximum state value */
    bool find_minima;                   /**< Find local minima */
    bool find_saddles;                  /**< Find saddle points */
    bool compute_barriers;              /**< Compute energy barriers */
} dynsys_energy_config_t;

/**
 * @brief Slow-fast decomposition configuration
 */
typedef struct {
    float epsilon;                      /**< Timescale separation parameter */
    uint32_t num_slow;                  /**< Number of slow variables */
    uint32_t num_fast;                  /**< Number of fast variables */
    uint32_t* slow_indices;             /**< Indices of slow variables */
    uint32_t* fast_indices;             /**< Indices of fast variables */
    bool compute_manifold;              /**< Compute slow manifold */
} dynsys_slowfast_config_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    bool enable_kg_wiring;
    bool enable_exception_handling;
    bool enable_bio_async;
    bool enable_immune_presentation;
    bool enable_logging;
    uint8_t log_level;
} dynsys_bridge_config_t;

//=============================================================================
// State and Result Structures
//=============================================================================

/**
 * @brief Lyapunov spectrum result
 */
typedef struct {
    float exponents[DYNSYS_MAX_LYAPUNOV];   /**< Lyapunov exponents (descending) */
    uint32_t num_exponents;                  /**< Number computed */
    float max_lyapunov;                      /**< Largest exponent */
    float sum_lyapunov;                      /**< Sum (Kaplan-Yorke dimension) */
    float kaplan_yorke_dim;                  /**< Kaplan-Yorke dimension */
    bool is_chaotic;                         /**< Has positive exponent */
    float entropy_rate;                      /**< Kolmogorov-Sinai entropy estimate */
} dynsys_lyapunov_result_t;

/**
 * @brief Bifurcation point
 */
typedef struct {
    float parameter_value;              /**< Parameter value at bifurcation */
    bifurcation_type_t type;            /**< Type of bifurcation */
    float state_before[DYNSYS_MAX_STATE_DIM];   /**< State before bifurcation */
    float state_after[DYNSYS_MAX_STATE_DIM];    /**< State after bifurcation */
    float eigenvalue_real;              /**< Critical eigenvalue (real part) */
    float eigenvalue_imag;              /**< Critical eigenvalue (imaginary part) */
} dynsys_bifurcation_point_t;

/**
 * @brief Bifurcation diagram result
 */
typedef struct {
    float* parameter_values;            /**< Parameter values sampled */
    float* state_samples;               /**< State samples (flattened) */
    uint32_t num_param_points;          /**< Number of parameter points */
    uint32_t samples_per_point;         /**< Samples collected per point */
    dynsys_bifurcation_point_t* bifurcations;  /**< Detected bifurcations */
    uint32_t num_bifurcations;          /**< Number of bifurcations found */
} dynsys_bifurcation_result_t;

/**
 * @brief Reconstructed attractor result
 */
typedef struct {
    float* embedded_points;             /**< Embedded trajectory */
    uint32_t num_points;                /**< Number of embedded points */
    uint32_t embedding_dim;             /**< Embedding dimension used */
    attractor_type_t type;              /**< Detected attractor type */
    float correlation_dim;              /**< Correlation dimension estimate */
    float box_dim;                      /**< Box-counting dimension estimate */
    float recurrence_rate;              /**< Recurrence quantification */
    float determinism;                  /**< Determinism measure */
} dynsys_attractor_result_t;

/**
 * @brief Energy landscape result
 */
typedef struct {
    float* energy_values;               /**< Energy values on grid */
    float* gradient;                    /**< Gradient field */
    uint32_t grid_size;                 /**< Total grid points */
    float* minima_locations;            /**< Local minima locations */
    float* minima_energies;             /**< Energies at minima */
    uint32_t num_minima;                /**< Number of local minima */
    float* saddle_locations;            /**< Saddle point locations */
    uint32_t num_saddles;               /**< Number of saddle points */
    float global_minimum;               /**< Global minimum energy */
    float* barriers;                    /**< Energy barriers between minima */
} dynsys_energy_result_t;

/**
 * @brief Slow-fast decomposition result
 */
typedef struct {
    float* slow_manifold;               /**< Slow manifold approximation */
    float* fast_manifold;               /**< Fast manifold approximation */
    float* slow_flow;                   /**< Slow flow on manifold */
    float timescale_ratio;              /**< Fast/slow timescale ratio */
    uint32_t manifold_points;           /**< Number of manifold points */
    bool manifold_exists;               /**< True if manifold well-defined */
} dynsys_slowfast_result_t;

/**
 * @brief System statistics
 */
typedef struct {
    uint64_t integrations;              /**< Total integration steps */
    uint64_t lyapunov_computations;     /**< Lyapunov computations */
    uint64_t bifurcation_scans;         /**< Bifurcation parameter sweeps */
    uint64_t attractor_reconstructions; /**< Attractor reconstructions */
    uint64_t energy_analyses;           /**< Energy landscape analyses */
    float avg_integration_time_us;      /**< Average integration time */
    uint64_t divergences;               /**< Number of diverged integrations */
    uint64_t recoveries;                /**< Successful recovery count */
} dynsys_stats_t;

//=============================================================================
// Main Dynamical System API
//=============================================================================

/**
 * @brief Get default configuration
 */
NIMCP_EXPORT dynsys_config_t dynsys_default_config(void);

/**
 * @brief Create dynamical system
 *
 * @param config System configuration
 * @param func System dynamics function
 * @param jacobian Jacobian function (optional, NULL for numerical)
 * @param context User context passed to functions
 * @return System handle or NULL on error
 */
NIMCP_EXPORT dynsys_system_t dynsys_create(
    const dynsys_config_t* config,
    dynsys_func_t func,
    dynsys_jacobian_t jacobian,
    void* context
);

/**
 * @brief Destroy dynamical system
 */
NIMCP_EXPORT void dynsys_destroy(dynsys_system_t sys);

/**
 * @brief Initialize with brain connection
 */
NIMCP_EXPORT dynsys_error_t dynsys_init(
    dynsys_system_t sys,
    nimcp_brain_t brain
);

/**
 * @brief Shutdown system
 */
NIMCP_EXPORT dynsys_error_t dynsys_shutdown(dynsys_system_t sys);

/**
 * @brief Set system parameters
 */
NIMCP_EXPORT dynsys_error_t dynsys_set_params(
    dynsys_system_t sys,
    const float* params,
    uint32_t param_dim
);

/**
 * @brief Integrate system forward in time
 *
 * @param sys System handle
 * @param state Initial state (modified in place)
 * @param num_steps Number of integration steps
 * @param trajectory Output trajectory (optional, NULL to skip)
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT dynsys_error_t dynsys_integrate(
    dynsys_system_t sys,
    float* state,
    uint32_t num_steps,
    float* trajectory
);

/**
 * @brief Single RK4 integration step
 */
NIMCP_EXPORT dynsys_error_t dynsys_step_rk4(
    dynsys_system_t sys,
    float* state,
    float dt
);

/**
 * @brief Get system statistics
 */
NIMCP_EXPORT dynsys_error_t dynsys_get_stats(
    dynsys_system_t sys,
    dynsys_stats_t* stats
);

//=============================================================================
// Lyapunov Exponent API
//=============================================================================

/**
 * @brief Get default Lyapunov config
 */
NIMCP_EXPORT dynsys_lyapunov_config_t dynsys_lyapunov_default_config(void);

/**
 * @brief Create Lyapunov exponent computer
 */
NIMCP_EXPORT dynsys_lyapunov_t dynsys_lyapunov_create(
    const dynsys_lyapunov_config_t* config,
    dynsys_system_t sys
);

/**
 * @brief Destroy Lyapunov computer
 */
NIMCP_EXPORT void dynsys_lyapunov_destroy(dynsys_lyapunov_t lyap);

/**
 * @brief Compute Lyapunov spectrum
 *
 * @param lyap Lyapunov computer
 * @param initial_state Initial state for trajectory
 * @param result Output result structure
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT dynsys_error_t dynsys_lyapunov_compute(
    dynsys_lyapunov_t lyap,
    const float* initial_state,
    dynsys_lyapunov_result_t* result
);

/**
 * @brief Compute maximum Lyapunov exponent only (faster)
 */
NIMCP_EXPORT dynsys_error_t dynsys_lyapunov_max(
    dynsys_lyapunov_t lyap,
    const float* initial_state,
    float* max_exponent
);

//=============================================================================
// Bifurcation Analysis API
//=============================================================================

/**
 * @brief Get default bifurcation config
 */
NIMCP_EXPORT dynsys_bifurcation_config_t dynsys_bifurcation_default_config(void);

/**
 * @brief Create bifurcation analyzer
 */
NIMCP_EXPORT dynsys_bifurcation_t dynsys_bifurcation_create(
    const dynsys_bifurcation_config_t* config,
    dynsys_system_t sys
);

/**
 * @brief Destroy bifurcation analyzer
 */
NIMCP_EXPORT void dynsys_bifurcation_destroy(dynsys_bifurcation_t bif);

/**
 * @brief Scan parameter and build bifurcation diagram
 *
 * @param bif Bifurcation analyzer
 * @param initial_state Initial state
 * @param result Output result (caller must free arrays)
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT dynsys_error_t dynsys_bifurcation_scan(
    dynsys_bifurcation_t bif,
    const float* initial_state,
    dynsys_bifurcation_result_t* result
);

/**
 * @brief Free bifurcation result arrays
 */
NIMCP_EXPORT void dynsys_bifurcation_result_free(dynsys_bifurcation_result_t* result);

/**
 * @brief Get bifurcation type name
 */
NIMCP_EXPORT const char* dynsys_bifurcation_type_name(bifurcation_type_t type);

//=============================================================================
// Attractor Reconstruction API
//=============================================================================

/**
 * @brief Get default attractor config
 */
NIMCP_EXPORT dynsys_attractor_config_t dynsys_attractor_default_config(void);

/**
 * @brief Create attractor reconstructor
 */
NIMCP_EXPORT dynsys_attractor_t dynsys_attractor_create(
    const dynsys_attractor_config_t* config
);

/**
 * @brief Destroy attractor reconstructor
 */
NIMCP_EXPORT void dynsys_attractor_destroy(dynsys_attractor_t attr);

/**
 * @brief Reconstruct attractor from time series using Takens embedding
 *
 * @param attr Attractor reconstructor
 * @param time_series Input time series
 * @param series_length Length of time series
 * @param result Output result (caller must free arrays)
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT dynsys_error_t dynsys_attractor_reconstruct(
    dynsys_attractor_t attr,
    const float* time_series,
    uint32_t series_length,
    dynsys_attractor_result_t* result
);

/**
 * @brief Free attractor result arrays
 */
NIMCP_EXPORT void dynsys_attractor_result_free(dynsys_attractor_result_t* result);

/**
 * @brief Estimate optimal embedding parameters
 *
 * @param attr Attractor reconstructor
 * @param time_series Input time series
 * @param series_length Length of time series
 * @param optimal_dim Output optimal embedding dimension
 * @param optimal_delay Output optimal time delay
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT dynsys_error_t dynsys_attractor_estimate_params(
    dynsys_attractor_t attr,
    const float* time_series,
    uint32_t series_length,
    uint32_t* optimal_dim,
    uint32_t* optimal_delay
);

/**
 * @brief Get attractor type name
 */
NIMCP_EXPORT const char* dynsys_attractor_type_name(attractor_type_t type);

//=============================================================================
// Energy Landscape API
//=============================================================================

/**
 * @brief Get default energy config
 */
NIMCP_EXPORT dynsys_energy_config_t dynsys_energy_default_config(void);

/**
 * @brief Create energy landscape analyzer
 */
NIMCP_EXPORT dynsys_energy_t dynsys_energy_create(
    const dynsys_energy_config_t* config,
    dynsys_system_t sys
);

/**
 * @brief Destroy energy analyzer
 */
NIMCP_EXPORT void dynsys_energy_destroy(dynsys_energy_t energy);

/**
 * @brief Compute energy landscape
 *
 * @param energy Energy analyzer
 * @param result Output result (caller must free arrays)
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT dynsys_error_t dynsys_energy_compute(
    dynsys_energy_t energy,
    dynsys_energy_result_t* result
);

/**
 * @brief Free energy result arrays
 */
NIMCP_EXPORT void dynsys_energy_result_free(dynsys_energy_result_t* result);

/**
 * @brief Find energy minimum using gradient descent
 *
 * @param energy Energy analyzer
 * @param initial_state Starting state
 * @param minimum_state Output minimum state
 * @param minimum_energy Output minimum energy
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT dynsys_error_t dynsys_energy_find_minimum(
    dynsys_energy_t energy,
    const float* initial_state,
    float* minimum_state,
    float* minimum_energy
);

/**
 * @brief Compute energy barrier between two states
 */
NIMCP_EXPORT dynsys_error_t dynsys_energy_barrier(
    dynsys_energy_t energy,
    const float* state_a,
    const float* state_b,
    float* barrier_height,
    float* saddle_state
);

//=============================================================================
// Slow-Fast Decomposition API
//=============================================================================

/**
 * @brief Get default slow-fast config
 */
NIMCP_EXPORT dynsys_slowfast_config_t dynsys_slowfast_default_config(void);

/**
 * @brief Create slow-fast decomposer
 */
NIMCP_EXPORT dynsys_slowfast_t dynsys_slowfast_create(
    const dynsys_slowfast_config_t* config,
    dynsys_system_t sys
);

/**
 * @brief Destroy slow-fast decomposer
 */
NIMCP_EXPORT void dynsys_slowfast_destroy(dynsys_slowfast_t sf);

/**
 * @brief Compute slow manifold approximation
 *
 * @param sf Slow-fast decomposer
 * @param result Output result (caller must free arrays)
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT dynsys_error_t dynsys_slowfast_compute(
    dynsys_slowfast_t sf,
    dynsys_slowfast_result_t* result
);

/**
 * @brief Free slow-fast result arrays
 */
NIMCP_EXPORT void dynsys_slowfast_result_free(dynsys_slowfast_result_t* result);

/**
 * @brief Project state onto slow manifold
 */
NIMCP_EXPORT dynsys_error_t dynsys_slowfast_project(
    dynsys_slowfast_t sf,
    const float* state,
    float* projected_state
);

//=============================================================================
// Bridge API
//=============================================================================

/**
 * @brief Get default bridge config
 */
NIMCP_EXPORT dynsys_bridge_config_t dynsys_bridge_default_config(void);

/**
 * @brief Create dynamical systems bridge
 */
NIMCP_EXPORT dynsys_bridge_t dynsys_bridge_create(
    const dynsys_bridge_config_t* config,
    dynsys_system_t sys
);

/**
 * @brief Destroy bridge
 */
NIMCP_EXPORT void dynsys_bridge_destroy(dynsys_bridge_t bridge);

/**
 * @brief Initialize bridge with brain connection
 */
NIMCP_EXPORT int dynsys_bridge_init(
    dynsys_bridge_t bridge,
    nimcp_brain_t brain,
    nimcp_bio_router_t router,
    nimcp_brain_immune_t immune
);

/**
 * @brief Shutdown bridge
 */
NIMCP_EXPORT int dynsys_bridge_shutdown(dynsys_bridge_t bridge);

/**
 * @brief Create KG module wiring descriptor
 */
NIMCP_EXPORT kg_module_wiring_t* dynsys_bridge_create_wiring(
    dynsys_bridge_t bridge
);

/**
 * @brief Register with brain KG
 */
//=============================================================================
// Wave W15: Runtime bifurcation/attractor event emission + read path
//=============================================================================

/* Forward-declared brain handle (full struct in brain_internal.h). */
#ifndef NIMCP_BRAIN_T_DEFINED
#define NIMCP_BRAIN_T_DEFINED
typedef struct brain_struct* brain_t;
#endif

/**
 * @brief Register brain handle for admin-token self-elevation on emit.
 */
NIMCP_EXPORT void dynsys_kg_register_brain(
    dynsys_bridge_t bridge,
    brain_t brain
);

/**
 * @brief Emit an aggregated bifurcation / attractor-switch event.
 *
 * Rate-limit: call only on qualitative state change, never every integration
 * step. `kind` examples: "bifurcation_detected", "attractor_switch",
 * "chaos_onset", "stability_loss".
 */
NIMCP_EXPORT void dynsys_kg_trigger_bifurcation_event(
    dynsys_bridge_t bridge,
    const char* kind,
    float param_value,
    float lyapunov_max,
    uint64_t ts_us
);

/**
 * @brief Read-path: count dynsys event nodes matching a kind substring.
 */
NIMCP_EXPORT uint32_t dynsys_kg_count_events(
    dynsys_bridge_t bridge,
    const char* kind_substr
);

NIMCP_EXPORT int dynsys_bridge_register_kg(
    dynsys_bridge_t bridge,
    brain_kg_t* kg
);

/**
 * @brief Register exception handler
 */
NIMCP_EXPORT int dynsys_bridge_register_exception_handler(
    dynsys_bridge_t bridge,
    dynsys_exception_handler_t handler
);

/**
 * @brief Raise exception
 */
NIMCP_EXPORT int dynsys_bridge_raise_exception(
    dynsys_bridge_t bridge,
    dynsys_exception_t exception,
    const char* message,
    void* context
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error string
 */
NIMCP_EXPORT const char* dynsys_error_string(dynsys_error_t err);

/**
 * @brief Compute numerical Jacobian
 *
 * @param sys Dynamical system
 * @param state Current state
 * @param jacobian Output Jacobian matrix (row-major)
 * @param epsilon Finite difference step
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT dynsys_error_t dynsys_numerical_jacobian(
    dynsys_system_t sys,
    const float* state,
    float* jacobian,
    float epsilon
);

/**
 * @brief Compute eigenvalues of matrix
 *
 * @param matrix Input matrix (row-major)
 * @param dim Matrix dimension
 * @param eigenvalues_real Output real parts
 * @param eigenvalues_imag Output imaginary parts
 * @return 0 on success, negative on error
 */
NIMCP_EXPORT dynsys_error_t dynsys_eigenvalues(
    const float* matrix,
    uint32_t dim,
    float* eigenvalues_real,
    float* eigenvalues_imag
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DYNAMICAL_SYSTEMS_H */
