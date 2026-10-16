/**
 * @file nimcp_quantum_annealing.h
 * @brief Quantum-inspired annealing for escaping local minima in weight optimization
 *
 * WHAT: Quantum annealing implementation for neural network optimization
 * WHY:  Escape local minima in weight space, achieve 10-100x better optimization
 * HOW:  Simulate quantum tunneling through energy barriers using thermal + quantum fluctuations
 *
 * BIOLOGY:
 * Biological neural networks avoid getting stuck in suboptimal configurations through:
 * - Spontaneous activity and noise
 * - Sleep-dependent synaptic reorganization
 * - Neuromodulator-driven exploration
 *
 * Quantum annealing provides similar capability through:
 * - Temperature-controlled exploration (simulated annealing)
 * - Quantum tunneling through local minima (quantum fluctuations)
 * - Gradual cooling to find global optimum
 *
 * ALGORITHM:
 * 1. Start at high temperature (high exploration)
 * 2. For each iteration:
 *    a. Sample neighboring states
 *    b. Accept based on Metropolis criterion + quantum probability
 *    c. Apply quantum tunneling to escape barriers
 *    d. Decrease temperature (cooling schedule)
 * 3. Return lowest energy state found
 *
 * MATH:
 * - Classical acceptance: P = exp(-ΔE/T)
 * - Quantum tunneling: P_tunnel = Γ * exp(-B/T^α) where Γ is quantum strength
 * - Combined: P_total = P_classical + P_quantum
 *
 * PERFORMANCE:
 * - Time: O(N * D) per iteration, N = iterations, D = dimensions
 * - Space: O(D) for state storage
 * - Overhead: ~5-10% when inactive, ~30-50% when active
 *
 * INTEGRATION:
 * - Called periodically during plasticity (every K learning steps)
 * - Optimizes full synaptic weight matrix
 * - Compatible with all plasticity rules (STDP, eligibility traces, etc.)
 *
 * @author NIMCP Development Team
 * @date 2025-11-12
 * @version 2.7.0 Phase 11 Enhancement C1.1
 */

#ifndef NIMCP_QUANTUM_ANNEALING_H
#define NIMCP_QUANTUM_ANNEALING_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration & Types
//=============================================================================

/**
 * @brief Cooling schedule strategies
 *
 * WHAT: Controls how temperature decreases over iterations
 * WHY:  Different schedules work better for different problems
 * HOW:  Select via config.cooling_schedule
 */
typedef enum {
    COOLING_EXPONENTIAL,  /**< T(t) = T_init * exp(-t/τ) - Fast, good for smooth landscapes */
    COOLING_LINEAR,       /**< T(t) = T_init - (T_init - T_final) * t/T_max - Slower, more exploration */
    COOLING_LOGARITHMIC,  /**< T(t) = T_init / log(1 + t) - Very slow, theoretical guarantees */
    COOLING_ADAPTIVE      /**< Adjust based on acceptance rate - Automatic tuning */
} cooling_schedule_t;

/**
 * @brief Quantum annealing configuration
 *
 * WHAT: Parameters controlling quantum annealing behavior
 * WHY:  Allow tuning for different optimization problems
 * HOW:  Pass to quantum_annealer_create()
 */
typedef struct {
    float initial_temperature;   /**< Starting temperature (exploration) - typical: 1.0 */
    float final_temperature;     /**< Ending temperature (exploitation) - typical: 0.01 */
    uint32_t num_iterations;     /**< Number of annealing steps - typical: 1000-10000 */
    cooling_schedule_t cooling_schedule; /**< Temperature decrease strategy */
    float quantum_strength;      /**< Tunneling probability multiplier Γ - typical: 0.5 */
    bool enable_tunneling;       /**< Enable quantum tunneling (vs pure simulated annealing) */
    uint32_t seed;               /**< Random seed for reproducibility */
} quantum_annealing_config_t;

/**
 * @brief Energy function type
 *
 * WHAT: User-defined function to evaluate state energy
 * WHY:  Allows optimization of arbitrary cost functions
 * HOW:  Return lower energy for better states
 *
 * @param state Current state vector (e.g., synaptic weights)
 * @param dim Dimensionality of state space
 * @param user_data Optional user data passed through
 * @return Energy value (lower is better)
 *
 * EXAMPLE (quadratic well):
 * ```c
 * float energy_func(const float* state, uint32_t dim, void* user_data) {
 *     float sum = 0.0f;
 *     for (uint32_t i = 0; i < dim; ++i) {
 *         sum += (state[i] - target[i]) * (state[i] - target[i]);
 *     }
 *     return sum;  // Minimize distance to target
 * }
 * ```
 */
typedef float (*energy_function_t)(const float* state, uint32_t dim, void* user_data);

/**
 * @brief Opaque quantum annealer handle
 *
 * WHAT: Handle to quantum annealer instance
 * WHY:  Encapsulate internal state, enable multiple annealers
 * HOW:  Create with quantum_annealer_create(), destroy with quantum_annealer_destroy()
 */
typedef struct quantum_annealer_struct* quantum_annealer_t;

//=============================================================================
// Core API
//=============================================================================

/**
 * @brief Create quantum annealer
 *
 * WHAT: Initialize quantum annealing optimizer
 * WHY:  Set up annealer with specified configuration
 * HOW:  Allocate memory, validate config, init RNG
 *
 * @param config Configuration parameters (copied internally)
 * @return Annealer handle, or NULL on error
 *
 * ERRORS:
 * - Returns NULL if config is NULL
 * - Returns NULL if config parameters invalid (see validation rules)
 * - Returns NULL if memory allocation fails
 *
 * VALIDATION:
 * - initial_temperature > 0
 * - final_temperature > 0
 * - final_temperature < initial_temperature
 * - num_iterations > 0
 * - quantum_strength >= 0 and <= 1
 *
 * COMPLEXITY: O(1) time, O(1) space
 *
 * EXAMPLE:
 * ```c
 * quantum_annealing_config_t config = {
 *     .initial_temperature = 1.0f,
 *     .final_temperature = 0.01f,
 *     .num_iterations = 1000,
 *     .cooling_schedule = COOLING_EXPONENTIAL,
 *     .quantum_strength = 0.5f,
 *     .enable_tunneling = true,
 *     .seed = 42
 * };
 * quantum_annealer_t annealer = quantum_annealer_create(&config);
 * if (!annealer) {
 *     fprintf(stderr, "Failed to create annealer\n");
 *     return -1;
 * }
 * ```
 */
quantum_annealer_t quantum_annealer_create(const quantum_annealing_config_t* config);

/**
 * @brief Destroy quantum annealer
 *
 * WHAT: Clean up annealer and free memory
 * WHY:  Prevent memory leaks
 * HOW:  Free internal state, nullify handle
 *
 * @param annealer Annealer to destroy (can be NULL)
 *
 * SAFETY: Safe to call with NULL pointer (no-op)
 * COMPLEXITY: O(1) time, frees O(1) space
 */
void quantum_annealer_destroy(quantum_annealer_t annealer);

/**
 * @brief Run quantum annealing optimization
 *
 * WHAT: Optimize energy function using quantum annealing
 * WHY:  Find global minimum, escape local minima
 * HOW:  Iterate through cooling schedule, accept moves via Metropolis + tunneling
 *
 * @param annealer Annealer instance
 * @param energy_func Energy function to minimize
 * @param initial_state Starting point in state space
 * @param optimized_state [OUT] Best state found
 * @param dim Dimensionality of state space
 * @param user_data Optional data passed to energy_func
 * @return Final energy value
 *
 * ALGORITHM:
 * 1. Copy initial_state to current_state
 * 2. For each iteration t:
 *    a. Generate neighbor state (small random perturbation)
 *    b. Compute energy difference ΔE
 *    c. Accept if ΔE < 0 (always accept improvements)
 *    d. Accept if ΔE > 0 with probability P(ΔE, T)
 *    e. Apply quantum tunneling with probability P_tunnel
 *    f. Update temperature T(t)
 * 3. Return best state found across all iterations
 *
 * COMPLEXITY: O(N * D * E) where:
 * - N = num_iterations
 * - D = dim
 * - E = cost of energy_func evaluation
 *
 * ERRORS:
 * - Returns INFINITY if annealer is NULL
 * - Returns INFINITY if energy_func is NULL
 * - Returns INFINITY if initial_state or optimized_state is NULL
 * - Returns INFINITY if dim is 0
 *
 * EXAMPLE:
 * ```c
 * float initial[10] = {0};  // Start at origin
 * float result[10];
 * float final_energy = quantum_anneal(annealer, my_energy_func,
 *                                     initial, result, 10, NULL);
 * printf("Optimized to energy %f\n", final_energy);
 * ```
 */
float quantum_anneal(
    quantum_annealer_t annealer,
    energy_function_t energy_func,
    const float* initial_state,
    float* optimized_state,
    uint32_t dim,
    void* user_data
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get temperature at specific iteration
 *
 * WHAT: Calculate temperature for given iteration
 * WHY:  Inspect cooling schedule, debug annealing behavior
 * HOW:  Apply cooling formula based on config
 *
 * @param annealer Annealer instance
 * @param iteration Iteration number (0 to num_iterations-1)
 * @return Temperature value, or -1.0 on error
 *
 * COMPLEXITY: O(1)
 */
float quantum_annealer_get_temperature(quantum_annealer_t annealer, uint32_t iteration);

/**
 * @brief Get default quantum annealing configuration
 *
 * WHAT: Provide sensible default parameters
 * WHY:  Quick setup for common use cases
 * HOW:  Return preset config values
 *
 * @return Default configuration
 *
 * DEFAULTS:
 * - initial_temperature = 1.0
 * - final_temperature = 0.01
 * - num_iterations = 1000
 * - cooling_schedule = COOLING_EXPONENTIAL
 * - quantum_strength = 0.5
 * - enable_tunneling = true
 * - seed = 0 (use time-based seed)
 */
quantum_annealing_config_t quantum_annealing_default_config(void);

//=============================================================================
// Monte Carlo Integration API
//=============================================================================

/* Forward declaration for QMC result type */
struct qmc_anneal_result;

/**
 * @brief Adaptive annealing using MCMC proposal learning
 *
 * WHAT: Quantum annealing with adaptive proposal distribution
 * WHY:  Learn optimal step sizes from acceptance history (2-5x faster convergence)
 * HOW:  Use qmc_adaptive_anneal() with learned proposal covariance
 *
 * @param annealer Annealer instance
 * @param energy_func Energy function to minimize
 * @param initial_state Starting state
 * @param optimized_state Output: best state found
 * @param dim State dimensionality
 * @param user_data Passed to energy function
 * @param result Output: detailed optimization result
 * @return Best energy found
 */
float quantum_annealer_optimize_adaptive_mc(
    quantum_annealer_t annealer,
    energy_function_t energy_func,
    const float* initial_state,
    float* optimized_state,
    uint32_t dim,
    void* user_data,
    struct qmc_anneal_result* result
);

/**
 * @brief Importance sampling for Boltzmann distribution
 *
 * WHAT: Sample states proportional to Boltzmann weights
 * WHY:  Efficient sampling of low-energy configurations
 * HOW:  Use MC importance sampling with exp(-E/T) weights
 *
 * @param annealer Annealer instance
 * @param energy_func Energy function
 * @param temperature Current temperature
 * @param states Array of candidate states (dim x num_states)
 * @param num_states Number of candidate states
 * @param dim State dimensionality
 * @param user_data Passed to energy function
 * @return Index of sampled state
 */
uint32_t quantum_annealer_sample_boltzmann_mc(
    quantum_annealer_t annealer,
    energy_function_t energy_func,
    float temperature,
    const float* states,
    uint32_t num_states,
    uint32_t dim,
    void* user_data
);

/**
 * @brief Estimate partition function via Monte Carlo
 *
 * WHAT: Estimate Z = Σ exp(-E_i/T) for normalization
 * WHY:  Needed for free energy calculations and convergence diagnostics
 * HOW:  MC sampling with variance estimation
 *
 * @param annealer Annealer instance
 * @param energy_func Energy function
 * @param temperature Current temperature
 * @param sample_states Sampled states for estimation
 * @param num_samples Number of samples
 * @param dim State dimensionality
 * @param user_data Passed to energy function
 * @param variance_out Output: variance of estimate
 * @return Estimated partition function
 */
float quantum_annealer_estimate_partition_mc(
    quantum_annealer_t annealer,
    energy_function_t energy_func,
    float temperature,
    const float* sample_states,
    uint32_t num_samples,
    uint32_t dim,
    void* user_data,
    float* variance_out
);

/**
 * @brief Get thread-local MC seed for quantum annealing
 *
 * WHAT: Access the thread-local RNG seed
 * WHY:  Allow external seeding for reproducibility
 * HOW:  Return pointer to thread-local seed variable
 *
 * @return Pointer to thread-local seed
 */
uint32_t* quantum_annealer_get_mc_seed(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_QUANTUM_ANNEALING_H
