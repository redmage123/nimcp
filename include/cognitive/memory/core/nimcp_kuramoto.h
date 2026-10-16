//=============================================================================
// nimcp_kuramoto.h - Kuramoto Oscillator Bank for Module Synchronization
//=============================================================================
/**
 * @file nimcp_kuramoto.h
 * @brief Coupled oscillator synchronization for NIMCP module coordination
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Kuramoto oscillator model for synchronizing NIMCP cognitive modules
 * WHY:  Synchronized modules can communicate effectively; phase coherence
 *       enables coordinated information processing across brain regions
 * HOW:  Each module is represented as a phase oscillator; coupled dynamics
 *       evolve according to the Kuramoto model with pink noise modulation
 *
 * NEUROSCIENCE FOUNDATION:
 * =============================================================================
 * Neural Synchronization Theory:
 * - Oscillatory synchronization is a fundamental mechanism for neural
 *   communication (Fries, 2015 - "Communication through Coherence")
 * - Gamma synchronization (~40 Hz) enables binding of distributed features
 * - Cross-frequency coupling coordinates multiple timescales
 * - Phase-locked neural populations can efficiently exchange information
 *
 * Kuramoto Model:
 * +--------------------------------------------------------------------------+
 * |  dtheta_i/dt = omega_i + (K/N) * SUM_j[ K_ij * sin(theta_j - theta_i) ]  |
 * |                + eta_i(t)                                                 |
 * |                                                                           |
 * |  Where:                                                                   |
 * |    theta_i = phase of oscillator i (0 to 2*pi)                           |
 * |    omega_i = natural frequency of oscillator i (can be pink-modulated)   |
 * |    K = global coupling strength                                           |
 * |    K_ij = pairwise coupling matrix (can be negative for inhibition)      |
 * |    N = number of oscillators                                              |
 * |    eta_i(t) = noise term (1/f spectrum for biological realism)           |
 * +--------------------------------------------------------------------------+
 *
 * Order Parameter (Synchronization Measure):
 * +--------------------------------------------------------------------------+
 * |  r * exp(i*psi) = (1/N) * SUM_j[ exp(i*theta_j) ]                        |
 * |                                                                           |
 * |  r (in [0,1]) = synchronization level:                                    |
 * |    r approx 0: Incoherent (no synchronization)                            |
 * |    r approx 1: Fully synchronized (all phases aligned)                    |
 * |                                                                           |
 * |  psi = mean phase (collective rhythm)                                     |
 * +--------------------------------------------------------------------------+
 *
 * NIMCP Application:
 * - Each cognitive module (perception, memory, executive, etc.) is an oscillator
 * - Coupling strength reflects functional connectivity
 * - Synchronized modules can exchange information during coherent phases
 * - Order parameter indicates overall cognitive integration
 *
 * PERFORMANCE:
 * =============================================================================
 * - Single step (N oscillators): O(N^2) for dense coupling, O(E) for sparse
 * - Order parameter computation: O(N)
 * - RK4 integration: 4x derivative evaluations per step
 * - Typical: N=32 oscillators, step in ~5us
 *
 * MEMORY:
 * =============================================================================
 * - Oscillator array: N * sizeof(kuramoto_oscillator_t) ~ 20 bytes/oscillator
 * - Dense coupling matrix: N^2 * 4 bytes (float)
 * - Sparse coupling: E * sizeof(kuramoto_coupling_t) ~ 16 bytes/edge
 * - Pink noise generators: N * ~200 bytes each
 *
 * INTEGRATION:
 * =============================================================================
 * - Prime Resonant: Kuramoto coherence is one component of resonance score
 * - Theta-Gamma: Oscillator frequencies can be theta/gamma bands
 * - Bio-Async: Module activity maps to oscillator phases
 * - Pink Noise Bridge: 1/f modulation of natural frequencies
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KURAMOTO_H
#define NIMCP_KURAMOTO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "constants/nimcp_math_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Configuration Constants
//=============================================================================

/** Maximum number of oscillators supported */
#define KURAMOTO_MAX_OSCILLATORS        256

/** Default number of oscillators */
#define KURAMOTO_DEFAULT_OSCILLATORS    32

/** Default global coupling strength K */
#define KURAMOTO_DEFAULT_COUPLING       0.5f

/** Default integration timestep (1 ms) */
#define KURAMOTO_DEFAULT_DT             0.001f

/** Default noise intensity for frequency modulation */
#define KURAMOTO_DEFAULT_NOISE_INTENSITY 0.1f

/** Two pi constant */
#ifndef NIMCP_TWO_PI_F
#endif

/** Pi constant */

/** Default synchronization threshold for is_synchronized check */
#define KURAMOTO_SYNC_THRESHOLD         0.8f

/** Critical coupling strength for phase transition (approximate) */
#define KURAMOTO_CRITICAL_COUPLING      1.59f

//=============================================================================
// Forward Declarations
//=============================================================================

/** Opaque handle from pink noise module */
typedef struct pink_noise_generator_internal_t* pink_noise_generator_t;

//=============================================================================
// Types and Structures
//=============================================================================

/**
 * @brief Single Kuramoto oscillator representing one NIMCP module
 *
 * WHAT: State and parameters for one coupled phase oscillator
 * WHY:  Each cognitive module has its own oscillatory dynamics
 * HOW:  Phase evolves according to Kuramoto equation with coupling
 *
 * BIOLOGICAL MEANING:
 * - phase: Current point in oscillation cycle (like EEG phase)
 * - natural_frequency: Intrinsic rhythm of the module
 * - frequency_offset: Moment-to-moment variation (pink noise)
 * - module_id: Links oscillator to specific NIMCP module
 */
typedef struct {
    float phase;                /**< Current phase theta in [0, 2*pi] */
    float natural_frequency;    /**< Natural frequency omega_i in rad/s */
    float frequency_offset;     /**< Pink noise modulation of frequency */
    uint32_t module_id;         /**< Associated NIMCP module identifier */
    bool active;                /**< Is this oscillator currently active? */
} kuramoto_oscillator_t;

/**
 * @brief Coupling between two oscillators (for sparse representation)
 *
 * WHAT: Directional coupling strength between oscillator pairs
 * WHY:  Not all modules are equally connected; sparse representation efficient
 * HOW:  Positive = excitatory synchronization, negative = inhibitory
 *
 * BIOLOGICAL MEANING:
 * - Positive coupling: Modules tend to synchronize (functional integration)
 * - Negative coupling: Modules tend to anti-synchronize (segregation)
 * - Zero coupling: Independent oscillators
 */
typedef struct {
    uint32_t from_idx;          /**< Source oscillator index */
    uint32_t to_idx;            /**< Target oscillator index */
    float strength;             /**< Coupling strength (can be negative) */
} kuramoto_coupling_t;

/**
 * @brief Configuration for Kuramoto oscillator system
 *
 * WHAT: Parameters controlling system behavior
 * WHY:  Allows tuning for different cognitive integration scenarios
 * HOW:  Passed to kuramoto_create() to configure system
 */
typedef struct {
    float base_coupling_strength;   /**< Global coupling K (default 0.5) */
    float noise_intensity;          /**< Pink noise amplitude (default 0.1) */
    float dt;                       /**< Integration timestep in seconds (default 0.001) */
    uint32_t max_oscillators;       /**< Maximum oscillators (default 32) */
    bool use_pink_noise;            /**< Enable 1/f frequency modulation */
    bool use_adaptive_coupling;     /**< Coupling adapts based on coherence */
    float adaptive_rate;            /**< Learning rate for adaptive coupling */
    uint32_t seed;                  /**< Random seed for noise (0 = time-based) */
} kuramoto_config_t;

/**
 * @brief Statistics about Kuramoto system state
 *
 * WHAT: Metrics describing synchronization and system health
 * WHY:  Monitor cognitive integration, detect pathological states
 * HOW:  Computed periodically or on demand
 */
typedef struct {
    float order_parameter_r;        /**< Current synchronization level [0,1] */
    float order_parameter_psi;      /**< Current mean phase */
    float mean_frequency;           /**< Mean natural frequency */
    float frequency_spread;         /**< Standard deviation of frequencies */
    float mean_coupling;            /**< Mean coupling strength */
    uint32_t active_oscillators;    /**< Number of active oscillators */
    uint64_t total_steps;           /**< Total integration steps performed */
    float total_time;               /**< Total simulated time in seconds */
    float coupling_energy;          /**< Total coupling energy (synchrony metric) */
} kuramoto_stats_t;

/**
 * @brief Kuramoto oscillator system state
 *
 * WHAT: Complete state of the coupled oscillator system
 * WHY:  Maintains all oscillators, coupling, and cached computations
 * HOW:  Created by kuramoto_create(), destroyed by kuramoto_destroy()
 *
 * DESIGN:
 * - Supports both dense (all-to-all) and sparse coupling representations
 * - Caches order parameter to avoid redundant computation
 * - Integrates with pink noise for biological realism
 */
typedef struct kuramoto_system_t {
    /* Oscillator array */
    kuramoto_oscillator_t* oscillators;     /**< Array of oscillators */
    uint32_t num_oscillators;               /**< Current number of oscillators */
    uint32_t max_oscillators;               /**< Maximum capacity */

    /* Dense coupling matrix (N x N) */
    float* coupling_matrix;                 /**< Dense coupling K_ij */
    bool use_sparse_coupling;               /**< Use sparse instead of dense */

    /* Sparse coupling representation */
    kuramoto_coupling_t* sparse_couplings;  /**< Sparse coupling edges */
    uint32_t num_couplings;                 /**< Number of sparse edges */
    uint32_t max_couplings;                 /**< Maximum sparse edges */

    /* Pink noise generators (one per oscillator) */
    pink_noise_generator_t* pink_generators;/**< Array of noise generators */
    bool noise_enabled;                     /**< Is noise currently enabled */

    /* Configuration */
    kuramoto_config_t config;               /**< System configuration */

    /* Cached order parameter */
    float order_parameter_r;                /**< Magnitude |r| */
    float order_parameter_psi;              /**< Phase arg(r) */
    bool order_valid;                       /**< Is cached value current? */

    /* Integration workspace (for RK4) */
    float* k1;                              /**< RK4 stage 1 */
    float* k2;                              /**< RK4 stage 2 */
    float* k3;                              /**< RK4 stage 3 */
    float* k4;                              /**< RK4 stage 4 */
    float* temp_phases;                     /**< Temporary phase array */

    /* Statistics */
    uint64_t step_count;                    /**< Total steps taken */
    float total_time;                       /**< Total simulated time */

    /* Module ID to index mapping */
    uint32_t* module_to_index;              /**< module_id -> oscillator index */
    uint32_t module_map_size;               /**< Size of mapping table */
} kuramoto_system_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default Kuramoto configuration
 *
 * WHAT: Returns sensible defaults based on neuroscience literature
 * WHY:  Provides starting point for typical cognitive integration
 * HOW:  Sets coupling ~0.5, dt=1ms, noise=0.1, 32 oscillators
 *
 * @return Default configuration structure
 *
 * EXAMPLE:
 * ```c
 * kuramoto_config_t config = kuramoto_config_default();
 * config.max_oscillators = 64;  // Override as needed
 * kuramoto_system_t* sys = kuramoto_create(&config);
 * ```
 */
NIMCP_EXPORT kuramoto_config_t kuramoto_config_default(void);

/**
 * @brief Create Kuramoto oscillator system
 *
 * WHAT: Allocates and initializes complete oscillator system
 * WHY:  Prepare system for module synchronization simulation
 * HOW:  Allocates arrays, initializes state, creates noise generators
 *
 * @param config Configuration (NULL for defaults)
 * @return System handle or NULL on failure
 *
 * COMPLEXITY: O(N^2) for dense coupling allocation
 * MEMORY: ~4*N^2 bytes (dense) or ~16*E bytes (sparse) + N*200 bytes (noise)
 *
 * EXAMPLE:
 * ```c
 * kuramoto_config_t config = kuramoto_config_default();
 * config.base_coupling_strength = 0.8f;  // Strong coupling
 * kuramoto_system_t* sys = kuramoto_create(&config);
 * if (!sys) {
 *     // Handle allocation failure
 * }
 * ```
 */
NIMCP_EXPORT kuramoto_system_t* kuramoto_create(const kuramoto_config_t* config);

/**
 * @brief Destroy Kuramoto system and free resources
 *
 * WHAT: Releases all memory associated with system
 * WHY:  Prevent memory leaks
 * HOW:  Frees oscillators, coupling matrix, noise generators, workspace
 *
 * @param system System to destroy (can be NULL)
 */
NIMCP_EXPORT void kuramoto_destroy(kuramoto_system_t* system);

/**
 * @brief Reset all oscillator phases to random values
 *
 * WHAT: Reinitializes phases uniformly in [0, 2*pi]
 * WHY:  Start fresh simulation, break pathological sync
 * HOW:  Uses internal RNG or provided seed
 *
 * @param system Kuramoto system
 * @return true on success, false on error
 */
NIMCP_EXPORT bool kuramoto_reset(kuramoto_system_t* system);

/**
 * @brief Reset system with specific seed
 *
 * WHAT: Reinitializes with reproducible random phases
 * WHY:  Reproducible experiments and testing
 * HOW:  Seeds RNG then resets phases
 *
 * @param system Kuramoto system
 * @param seed Random seed for phase initialization
 * @return true on success, false on error
 */
NIMCP_EXPORT bool kuramoto_reset_seeded(kuramoto_system_t* system, uint32_t seed);

//=============================================================================
// Oscillator Management
//=============================================================================

/**
 * @brief Add oscillator for an NIMCP module
 *
 * WHAT: Creates new oscillator associated with a module ID
 * WHY:  Register cognitive module for synchronization tracking
 * HOW:  Allocates slot, initializes random phase, sets frequency
 *
 * @param system Kuramoto system
 * @param module_id NIMCP module identifier
 * @param natural_freq Natural frequency in rad/s (typical: 1-100 rad/s)
 * @return Oscillator index or -1 on failure
 *
 * COMPLEXITY: O(1) average for module lookup
 *
 * EXAMPLE:
 * ```c
 * // Add oscillator for visual cortex at 40 rad/s (gamma band)
 * int32_t idx = kuramoto_add_oscillator(sys, MODULE_VISUAL_CORTEX, 40.0f);
 * if (idx < 0) {
 *     // System full or module already exists
 * }
 * ```
 */
NIMCP_EXPORT int32_t kuramoto_add_oscillator(kuramoto_system_t* system,
                                              uint32_t module_id,
                                              float natural_freq);

/**
 * @brief Remove oscillator for a module
 *
 * WHAT: Deactivates oscillator and removes from coupling
 * WHY:  Module no longer participates in synchronization
 * HOW:  Marks inactive, zeros coupling row/column
 *
 * @param system Kuramoto system
 * @param module_id Module identifier to remove
 * @return true on success, false if not found
 */
NIMCP_EXPORT bool kuramoto_remove_oscillator(kuramoto_system_t* system,
                                              uint32_t module_id);

/**
 * @brief Set natural frequency of an oscillator
 *
 * WHAT: Updates intrinsic oscillation rate
 * WHY:  Modules may change frequency based on task demands
 * HOW:  Direct assignment, invalidates cached order parameter
 *
 * @param system Kuramoto system
 * @param module_id Module identifier
 * @param freq New natural frequency in rad/s
 * @return true on success, false if module not found
 */
NIMCP_EXPORT bool kuramoto_set_frequency(kuramoto_system_t* system,
                                          uint32_t module_id,
                                          float freq);

/**
 * @brief Get current phase of an oscillator
 *
 * WHAT: Returns current phase in [0, 2*pi]
 * WHY:  Query oscillator state for timing decisions
 * HOW:  Direct lookup via module ID mapping
 *
 * @param system Kuramoto system
 * @param module_id Module identifier
 * @return Phase in radians, or -1.0f if not found
 */
NIMCP_EXPORT float kuramoto_get_phase(const kuramoto_system_t* system,
                                       uint32_t module_id);

/**
 * @brief Force-set phase of an oscillator
 *
 * WHAT: Directly sets oscillator phase
 * WHY:  Force synchronization, external phase reset
 * HOW:  Wraps to [0, 2*pi] and assigns
 *
 * @param system Kuramoto system
 * @param module_id Module identifier
 * @param phase New phase in radians
 * @return true on success, false if module not found
 */
NIMCP_EXPORT bool kuramoto_set_phase(kuramoto_system_t* system,
                                      uint32_t module_id,
                                      float phase);

/**
 * @brief Get oscillator by module ID
 *
 * WHAT: Returns pointer to oscillator structure
 * WHY:  Direct access to oscillator state
 * HOW:  Lookup via module ID mapping
 *
 * @param system Kuramoto system
 * @param module_id Module identifier
 * @return Oscillator pointer or NULL if not found
 *
 * @note Returned pointer is valid until oscillator is removed
 */
NIMCP_EXPORT const kuramoto_oscillator_t* kuramoto_get_oscillator(
    const kuramoto_system_t* system,
    uint32_t module_id);

/**
 * @brief Get oscillator index from module ID
 *
 * WHAT: Returns internal array index for a module
 * WHY:  Efficient direct array access
 * HOW:  Lookup via module ID mapping
 *
 * @param system Kuramoto system
 * @param module_id Module identifier
 * @return Index or -1 if not found
 */
NIMCP_EXPORT int32_t kuramoto_get_oscillator_index(
    const kuramoto_system_t* system,
    uint32_t module_id);

//=============================================================================
// Coupling Management
//=============================================================================

/**
 * @brief Set coupling strength between two modules
 *
 * WHAT: Sets K_ij coupling coefficient
 * WHY:  Control synchronization tendency between specific modules
 * HOW:  Updates dense matrix or sparse list
 *
 * @param system Kuramoto system
 * @param from_id Source module identifier
 * @param to_id Target module identifier
 * @param strength Coupling strength (positive=sync, negative=anti-sync)
 * @return true on success, false if modules not found
 *
 * BIOLOGICAL MEANING:
 * - Positive: Modules tend to align phases (functional integration)
 * - Negative: Modules tend to oppose phases (segregation/inhibition)
 * - Zero: No coupling (independence)
 *
 * EXAMPLE:
 * ```c
 * // Strong positive coupling between visual areas
 * kuramoto_set_coupling(sys, V1_ID, V2_ID, 1.0f);
 *
 * // Inhibitory coupling between competing modules
 * kuramoto_set_coupling(sys, LEFT_MOTOR_ID, RIGHT_MOTOR_ID, -0.5f);
 * ```
 */
NIMCP_EXPORT bool kuramoto_set_coupling(kuramoto_system_t* system,
                                         uint32_t from_id,
                                         uint32_t to_id,
                                         float strength);

/**
 * @brief Set global uniform coupling strength
 *
 * WHAT: Sets all coupling coefficients to same value (all-to-all)
 * WHY:  Simple uniform connectivity model
 * HOW:  Fills coupling matrix with constant K
 *
 * @param system Kuramoto system
 * @param strength Global coupling strength K
 * @return true on success, false on error
 */
NIMCP_EXPORT bool kuramoto_set_global_coupling(kuramoto_system_t* system,
                                                float strength);

/**
 * @brief Set complete coupling matrix
 *
 * WHAT: Replaces entire coupling matrix
 * WHY:  Load preconfigured connectivity pattern
 * HOW:  Copies N^2 values from input
 *
 * @param system Kuramoto system
 * @param matrix N x N coupling matrix (row-major)
 * @return true on success, false on error
 *
 * @note Matrix size must match num_oscillators x num_oscillators
 */
NIMCP_EXPORT bool kuramoto_set_coupling_matrix(kuramoto_system_t* system,
                                                const float* matrix);

/**
 * @brief Get coupling strength between two modules
 *
 * WHAT: Returns K_ij coupling coefficient
 * WHY:  Query current connectivity
 * HOW:  Lookup in matrix or sparse list
 *
 * @param system Kuramoto system
 * @param from_id Source module identifier
 * @param to_id Target module identifier
 * @return Coupling strength, or 0.0f if not found
 */
NIMCP_EXPORT float kuramoto_get_coupling(const kuramoto_system_t* system,
                                          uint32_t from_id,
                                          uint32_t to_id);

/**
 * @brief Enable sparse coupling mode
 *
 * WHAT: Switches from dense to sparse coupling representation
 * WHY:  Memory efficient for weakly connected networks
 * HOW:  Converts non-zero entries to edge list
 *
 * @param system Kuramoto system
 * @param max_couplings Maximum number of coupling edges
 * @return true on success, false on error
 */
NIMCP_EXPORT bool kuramoto_enable_sparse_coupling(kuramoto_system_t* system,
                                                   uint32_t max_couplings);

/**
 * @brief Add sparse coupling edge
 *
 * WHAT: Adds single coupling connection in sparse mode
 * WHY:  Build sparse network incrementally
 * HOW:  Appends to edge list
 *
 * @param system Kuramoto system (must be in sparse mode)
 * @param from_id Source module identifier
 * @param to_id Target module identifier
 * @param strength Coupling strength
 * @return true on success, false if full or not in sparse mode
 */
NIMCP_EXPORT bool kuramoto_add_sparse_coupling(kuramoto_system_t* system,
                                                uint32_t from_id,
                                                uint32_t to_id,
                                                float strength);

//=============================================================================
// Simulation Functions
//=============================================================================

/**
 * @brief Single integration step using RK4
 *
 * WHAT: Advances all oscillator phases by one timestep
 * WHY:  Core simulation primitive for phase evolution
 * HOW:  Fourth-order Runge-Kutta integration of Kuramoto equations
 *
 * ALGORITHM (RK4):
 *   For each oscillator i:
 *     k1 = f(theta_i, t)
 *     k2 = f(theta_i + dt/2 * k1, t + dt/2)
 *     k3 = f(theta_i + dt/2 * k2, t + dt/2)
 *     k4 = f(theta_i + dt * k3, t + dt)
 *     theta_i_new = theta_i + dt/6 * (k1 + 2*k2 + 2*k3 + k4)
 *
 *   Where f(theta_i, t) = omega_i + (K/N) * SUM_j[K_ij * sin(theta_j - theta_i)]
 *                        + eta_i(t)
 *
 * @param system Kuramoto system
 * @param dt Timestep in seconds (use 0 for system default)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(N^2) for dense coupling, O(N + E) for sparse
 */
NIMCP_EXPORT bool kuramoto_step(kuramoto_system_t* system, float dt);

/**
 * @brief Multiple integration steps
 *
 * WHAT: Performs n_steps of integration
 * WHY:  Batch simulation for efficiency
 * HOW:  Calls kuramoto_step() n_steps times
 *
 * @param system Kuramoto system
 * @param dt Timestep per step (use 0 for system default)
 * @param n_steps Number of steps to perform
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n_steps * N^2) for dense
 */
NIMCP_EXPORT bool kuramoto_step_n(kuramoto_system_t* system,
                                   float dt,
                                   uint32_t n_steps);

/**
 * @brief Evolve system for specified duration
 *
 * WHAT: Simulates system for duration seconds using config.dt
 * WHY:  Convenience for time-based simulation
 * HOW:  Computes steps = duration / dt, calls kuramoto_step_n()
 *
 * @param system Kuramoto system
 * @param duration Total simulation duration in seconds
 * @return true on success, false on error
 */
NIMCP_EXPORT bool kuramoto_evolve(kuramoto_system_t* system, float duration);

//=============================================================================
// Order Parameter and Coherence
//=============================================================================

/**
 * @brief Compute order parameter (synchronization measure)
 *
 * WHAT: Calculates complex order parameter r * exp(i*psi)
 * WHY:  Primary measure of collective synchronization
 * HOW:  r * exp(i*psi) = (1/N) * SUM_j[exp(i*theta_j)]
 *
 * @param system Kuramoto system
 * @return true on success, false on error
 *
 * @note Results cached in system->order_parameter_r and _psi
 *
 * COMPLEXITY: O(N)
 */
NIMCP_EXPORT bool kuramoto_compute_order_parameter(kuramoto_system_t* system);

/**
 * @brief Get cached order parameter magnitude
 *
 * WHAT: Returns synchronization level r in [0, 1]
 * WHY:  Fast access to synchronization metric
 * HOW:  Returns cached value (recomputes if invalid)
 *
 * @param system Kuramoto system
 * @return Order parameter r, or -1.0f on error
 *
 * INTERPRETATION:
 * - r ~ 0: Incoherent, phases uniformly distributed
 * - r ~ 0.5: Partial synchronization
 * - r ~ 1: Full synchronization, phases aligned
 */
NIMCP_EXPORT float kuramoto_get_order_parameter(kuramoto_system_t* system);

/**
 * @brief Get mean phase (collective rhythm)
 *
 * WHAT: Returns mean phase psi of order parameter
 * WHY:  Identifies collective oscillation phase
 * HOW:  Returns cached value (recomputes if invalid)
 *
 * @param system Kuramoto system
 * @return Mean phase psi in [0, 2*pi], or -1.0f on error
 */
NIMCP_EXPORT float kuramoto_get_mean_phase(kuramoto_system_t* system);

/**
 * @brief Compute pairwise phase coherence
 *
 * WHAT: Phase locking value between two oscillators
 * WHY:  Measure synchronization between specific module pair
 * HOW:  coherence = cos(theta_i - theta_j)
 *
 * @param system Kuramoto system
 * @param module_id1 First module identifier
 * @param module_id2 Second module identifier
 * @return Coherence in [-1, 1] (1 = in-phase, -1 = anti-phase), or 0 on error
 *
 * EXAMPLE:
 * ```c
 * float coherence = kuramoto_coherence(sys, V1_ID, V2_ID);
 * if (coherence > 0.9f) {
 *     // V1 and V2 are highly synchronized
 * }
 * ```
 */
NIMCP_EXPORT float kuramoto_coherence(const kuramoto_system_t* system,
                                       uint32_t module_id1,
                                       uint32_t module_id2);

/**
 * @brief Check if system is synchronized
 *
 * WHAT: Tests if order parameter exceeds threshold
 * WHY:  Simple yes/no synchronization check
 * HOW:  Returns r > threshold
 *
 * @param system Kuramoto system
 * @param threshold Synchronization threshold (default 0.8)
 * @return true if synchronized (r > threshold)
 */
NIMCP_EXPORT bool kuramoto_is_synchronized(kuramoto_system_t* system,
                                            float threshold);

/**
 * @brief Compute phase coherence matrix
 *
 * WHAT: Returns N x N matrix of pairwise coherences
 * WHY:  Full picture of synchronization structure
 * HOW:  coherence[i][j] = cos(theta_i - theta_j)
 *
 * @param system Kuramoto system
 * @param out Output matrix (N x N, caller allocated)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(N^2)
 */
NIMCP_EXPORT bool kuramoto_get_coherence_matrix(const kuramoto_system_t* system,
                                                 float* out);

//=============================================================================
// Pink Noise Integration
//=============================================================================

/**
 * @brief Update frequency offsets from pink noise
 *
 * WHAT: Generates new pink noise sample for each oscillator's frequency
 * WHY:  Biological realism - neural oscillations have 1/f variability
 * HOW:  Samples pink noise generators, updates frequency_offset
 *
 * @param system Kuramoto system
 * @return true on success, false if noise disabled or error
 */
NIMCP_EXPORT bool kuramoto_update_noise(kuramoto_system_t* system);

/**
 * @brief Set noise intensity
 *
 * WHAT: Changes pink noise modulation amplitude
 * WHY:  Control variability level
 * HOW:  Updates noise amplitude for all generators
 *
 * @param system Kuramoto system
 * @param intensity Noise intensity (0 = none, 1 = high)
 * @return true on success, false on error
 */
NIMCP_EXPORT bool kuramoto_set_noise_intensity(kuramoto_system_t* system,
                                                float intensity);

/**
 * @brief Enable or disable noise modulation
 *
 * WHAT: Toggles pink noise frequency modulation
 * WHY:  Compare with/without noise for experiments
 * HOW:  Sets internal flag
 *
 * @param system Kuramoto system
 * @param enabled true to enable, false to disable
 * @return true on success, false on error
 */
NIMCP_EXPORT bool kuramoto_set_noise_enabled(kuramoto_system_t* system,
                                              bool enabled);

//=============================================================================
// Module Integration (NIMCP-specific)
//=============================================================================

/**
 * @brief Register NIMCP module as oscillator
 *
 * WHAT: Convenience wrapper for kuramoto_add_oscillator
 * WHY:  Clear module-centric API for NIMCP integration
 * HOW:  Adds oscillator with module-appropriate defaults
 *
 * @param system Kuramoto system
 * @param module_id NIMCP module identifier
 * @param freq Natural frequency in rad/s
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool kuramoto_register_module(kuramoto_system_t* system,
                                            uint32_t module_id,
                                            float freq);

/**
 * @brief Unregister NIMCP module
 *
 * WHAT: Removes oscillator for module
 * WHY:  Module shutdown or deactivation
 * HOW:  Calls kuramoto_remove_oscillator
 *
 * @param system Kuramoto system
 * @param module_id NIMCP module identifier
 * @return true on success, false if not found
 */
NIMCP_EXPORT bool kuramoto_unregister_module(kuramoto_system_t* system,
                                              uint32_t module_id);

/**
 * @brief Get module's current phase
 *
 * WHAT: Returns phase for registered module
 * WHY:  Query timing information for module coordination
 * HOW:  Calls kuramoto_get_phase
 *
 * @param system Kuramoto system
 * @param module_id NIMCP module identifier
 * @return Phase in [0, 2*pi] or -1.0f if not found
 */
NIMCP_EXPORT float kuramoto_get_module_phase(const kuramoto_system_t* system,
                                              uint32_t module_id);

/**
 * @brief Force synchronization between two modules
 *
 * WHAT: Sets both modules to same phase
 * WHY:  Force immediate synchronization for coordination
 * HOW:  Sets both phases to their mean
 *
 * @param system Kuramoto system
 * @param module_id1 First module identifier
 * @param module_id2 Second module identifier
 * @return true on success, false if modules not found
 */
NIMCP_EXPORT bool kuramoto_sync_modules(kuramoto_system_t* system,
                                         uint32_t module_id1,
                                         uint32_t module_id2);

/**
 * @brief Check if two modules are coherent
 *
 * WHAT: Tests if modules have similar phases
 * WHY:  Determine if modules can communicate effectively
 * HOW:  Returns kuramoto_coherence > threshold
 *
 * @param system Kuramoto system
 * @param module_id1 First module identifier
 * @param module_id2 Second module identifier
 * @param threshold Coherence threshold (default 0.8)
 * @return true if coherent
 */
NIMCP_EXPORT bool kuramoto_modules_coherent(const kuramoto_system_t* system,
                                             uint32_t module_id1,
                                             uint32_t module_id2,
                                             float threshold);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get system statistics
 *
 * WHAT: Fills statistics structure with current metrics
 * WHY:  Monitoring, debugging, analysis
 * HOW:  Computes various summary statistics
 *
 * @param system Kuramoto system
 * @param stats Output statistics structure
 * @return true on success, false on error
 */
NIMCP_EXPORT bool kuramoto_get_stats(const kuramoto_system_t* system,
                                      kuramoto_stats_t* stats);

/**
 * @brief Print system state (debug)
 *
 * WHAT: Outputs human-readable state description
 * WHY:  Debugging and development
 * HOW:  Prints to stdout or provided stream
 *
 * @param system Kuramoto system
 */
NIMCP_EXPORT void kuramoto_print_state(const kuramoto_system_t* system);

/**
 * @brief Get all oscillator phases
 *
 * WHAT: Copies all phases to output array
 * WHY:  Bulk access for analysis
 * HOW:  Copies from oscillator array
 *
 * @param system Kuramoto system
 * @param phases Output array (must hold num_oscillators floats)
 * @return Number of phases copied
 */
NIMCP_EXPORT uint32_t kuramoto_get_all_phases(const kuramoto_system_t* system,
                                               float* phases);

/**
 * @brief Set all oscillator phases
 *
 * WHAT: Bulk set all phases
 * WHY:  Restore state, set specific configuration
 * HOW:  Copies to oscillator array
 *
 * @param system Kuramoto system
 * @param phases Input array of phases
 * @param count Number of phases to set
 * @return true on success, false on error
 */
NIMCP_EXPORT bool kuramoto_set_all_phases(kuramoto_system_t* system,
                                           const float* phases,
                                           uint32_t count);

/**
 * @brief Get number of active oscillators
 *
 * WHAT: Returns count of active oscillators
 * WHY:  Query system size
 *
 * @param system Kuramoto system
 * @return Number of active oscillators
 */
NIMCP_EXPORT uint32_t kuramoto_get_num_oscillators(
    const kuramoto_system_t* system);

/**
 * @brief Wrap phase to [0, 2*pi]
 *
 * WHAT: Normalizes phase angle
 * WHY:  Utility for phase arithmetic
 * HOW:  Uses fmod and adjustment
 *
 * @param phase Input phase
 * @return Wrapped phase in [0, 2*pi]
 */
NIMCP_EXPORT float kuramoto_wrap_phase(float phase);

/**
 * @brief Compute phase difference in [-pi, pi]
 *
 * WHAT: Shortest angular difference between phases
 * WHY:  Utility for coherence computation
 * HOW:  Wraps difference to [-pi, pi]
 *
 * @param phase1 First phase
 * @param phase2 Second phase
 * @return Phase difference in [-pi, pi]
 */
NIMCP_EXPORT float kuramoto_phase_difference(float phase1, float phase2);

/**
 * @brief Get last error message
 *
 * WHAT: Returns human-readable error description
 * WHY:  Debugging failed operations
 *
 * @return Error string or NULL if no error
 */
NIMCP_EXPORT const char* kuramoto_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KURAMOTO_H */
