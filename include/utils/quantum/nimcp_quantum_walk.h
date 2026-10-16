//=============================================================================
// nimcp_quantum_walk.h - Quantum Walk for Neuromodulator Diffusion
//=============================================================================
/**
 * @file nimcp_quantum_walk.h
 * @brief Quantum random walk for accelerated neuromodulator diffusion
 *
 * WHAT: Quantum walker on neural network graph for neuromodulation
 * WHY: √N speedup over classical diffusion (O(N) vs O(N²))
 * HOW: Quantum coin + shift operators on graph structure
 *
 * MATHEMATICAL FOUNDATION:
 *
 * Classical random walk: Distance d in O(d²) steps
 * Quantum walk: Distance d in O(d) steps  → QUADRATIC SPEEDUP!
 *
 * ALGORITHM:
 * ```
 * State: |ψ⟩ = Σᵢ αᵢ|i⟩ (complex amplitude at each node)
 *
 * Quantum walk operator: U = S × C
 * C = Coin operator (Hadamard gate): superposition at each node
 * S = Shift operator: Move amplitude along edges
 *
 * Update: |ψ(t+1)⟩ = U|ψ(t)⟩
 * Probability: P(i) = |αᵢ|²
 * ```
 *
 * INTEGRATION WITH NIMCP:
 * - Neuromodulator diffusion (dopamine, serotonin, ACh, NE)
 * - Attention spread across network
 * - Information propagation
 * - Synchronization waves
 *
 * PERFORMANCE:
 * - Quantum walk step: O(E) where E = number of edges
 * - Classical diffusion step: O(N²) for dense graphs
 * - Speedup: O(N²/E) = O(N) for sparse graphs
 * - Memory overhead: 2× (complex amplitudes vs real values)
 *
 * SYNERGIES:
 * - Part A2.1 (Spatial diffusion): Hybrid quantum-classical dynamics
 * - Part C3.1 (MPS): Compress quantum state for memory efficiency
 * - Neuromodulator system: Faster dopamine/serotonin propagation
 *
 * BIOLOGICAL REALISM:
 * - While not literally quantum, captures fast information spread
 * - Models "non-local" neuromodulator effects
 * - Explains rapid mood/attention shifts
 *
 * EXAMPLE:
 * ```c
 * // Create quantum walker on neural network
 * quantum_walk_config_t config = quantum_walk_default_config();
 * quantum_walker_t* walker = quantum_walk_create(network, NEUROMOD_DOPAMINE, &config);
 *
 * // Initialize at source neuron (reward signal)
 * quantum_walk_initialize(walker, source_neuron_id);
 *
 * // Evolve quantum state
 * for (int steps = 0; steps < config.num_steps; steps++) {
 *     quantum_walk_step(walker);
 * }
 *
 * // Extract probability distribution → neuromodulator concentration
 * quantum_walk_get_distribution(walker, concentration_field);
 *
 * // Result: Dopamine spreads to distance d in O(d) vs O(d²) classical!
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-11-11
 * @version 2.9.0 Phase C2.1
 */

#ifndef NIMCP_QUANTUM_WALK_H
#define NIMCP_QUANTUM_WALK_H

#include <stdint.h>
#include <stdbool.h>

// Complex number support for both C and C++
#ifdef __cplusplus
extern "C++" {  // Temporarily use C++ linkage for C++ headers
#endif
    #ifdef __cplusplus
        #include <complex>
        typedef std::complex<float> quantum_amplitude_t;
    #else
        #include <complex.h>
        typedef float complex quantum_amplitude_t;
    #endif
#ifdef __cplusplus
}  // Close C++ linkage block
#endif

// Forward declarations
typedef struct neural_network_struct* neural_network_t;

//=============================================================================
// Complex Number Utilities
//=============================================================================

/**
 * @brief Complex number (quantum amplitude)
 *
 * WHAT: Complex amplitude αᵢ = real + i×imag
 * WHY: Quantum states require complex amplitudes
 * HOW: C99 complex.h (C) or std::complex (C++)
 */

/**
 * @brief Compute probability from amplitude
 *
 * WHAT: P = |α|² = real² + imag²
 * WHY: Born rule: probability = amplitude squared
 * HOW: Use std::abs for C++, cabsf for C
 *
 * @param amplitude Quantum amplitude
 * @return Probability [0, 1]
 */
static inline float quantum_probability(quantum_amplitude_t amplitude) {
#ifdef __cplusplus
    float re = amplitude.real();
    float im = amplitude.imag();
#else
    float re = crealf(amplitude);
    float im = cimagf(amplitude);
#endif
    return re * re + im * im;
}

/**
 * @brief Normalize quantum state
 *
 * WHAT: Scale amplitudes so Σ|αᵢ|² = 1
 * WHY: Probability conservation
 * HOW: Compute norm, divide all amplitudes
 *
 * @param amplitudes Array of amplitudes
 * @param size Number of amplitudes
 */
void quantum_normalize(quantum_amplitude_t* amplitudes, uint32_t size);

//=============================================================================
// Quantum Walk Configuration
//=============================================================================

/**
 * @brief Quantum coin operator types
 *
 * WHAT: Operator applied at each node before shift
 * WHY: Create quantum superposition
 * HOW: Unitary matrix acting on walker state
 */
typedef enum {
    COIN_HADAMARD,      /**< Hadamard coin: balanced superposition */
    COIN_GROVER,        /**< Grover coin: biased toward uniform */
    COIN_FOURIER,       /**< Fourier coin: phase-dependent mixing */
    COIN_IDENTITY       /**< Identity coin: no mixing (classical limit) */
} quantum_coin_type_t;

/**
 * @brief Quantum walk configuration
 *
 * WHAT: Parameters controlling quantum walk dynamics
 * WHY: Tune speedup vs accuracy tradeoff
 * HOW: Adjust coin type, steps, hybrid mixing
 */
typedef struct {
    quantum_coin_type_t coin_type;    /**< Coin operator type */
    uint32_t num_steps;                /**< Number of quantum walk steps */
    float hybrid_mixing;               /**< Mix quantum + classical [0=pure quantum, 1=classical] */
    bool normalize_each_step;          /**< Normalize after each step (default: true) */
    float decoherence_rate;            /**< Decoherence toward classical [0=none, 1=instant] */
    uint32_t measurement_interval;     /**< Measure probability every N steps (0=only at end) */
    bool enable_boundary_conditions;   /**< Apply boundary conditions at network edges */
} quantum_walk_config_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get default quantum walk configuration
 *
 * WHAT: Balanced configuration for general use
 * WHY: Good starting point (√N speedup, stable)
 * HOW: Hadamard coin, 100 steps, minimal decoherence
 *
 * @return Default configuration
 */
quantum_walk_config_t quantum_walk_default_config(void);

/**
 * @brief Get fast quantum walk configuration
 *
 * WHAT: Optimized for maximum speedup
 * WHY: Pure quantum dynamics, no classical mixing
 * HOW: Grover coin, 50 steps, zero decoherence
 *
 * @return Fast configuration
 */
quantum_walk_config_t quantum_walk_fast_config(void);

/**
 * @brief Get hybrid classical-quantum configuration
 *
 * WHAT: Mix quantum and classical diffusion
 * WHY: Balance speedup and biological realism
 * HOW: 50% quantum + 50% classical mixing
 *
 * @return Hybrid configuration
 */
quantum_walk_config_t quantum_walk_hybrid_config(void);

//=============================================================================
// Quantum Walker Structure
//=============================================================================

/**
 * @brief Quantum walker statistics
 *
 * WHAT: Diagnostic information about quantum walk
 * WHY: Monitor convergence, performance, accuracy
 * HOW: Computed during walk evolution
 */
typedef struct {
    float total_probability;           /**< Σ|αᵢ|² (should be ≈1.0) */
    float max_amplitude;               /**< max |αᵢ| */
    float entropy;                     /**< Shannon entropy of probability distribution */
    float spreading_distance;          /**< Average distance from initial position */
    uint32_t num_nonzero_amplitudes;   /**< Number of nodes with |αᵢ| > threshold */
    float speedup_vs_classical;        /**< Estimated speedup factor */
    uint64_t evolution_time_us;        /**< Time to evolve (microseconds) */
} quantum_walk_stats_t;

/**
 * @brief Quantum walker on neural network graph
 *
 * WHAT: Quantum state |ψ⟩ = Σᵢ αᵢ|i⟩ evolving on network
 * WHY: Accelerated diffusion for neuromodulators
 * HOW: Store complex amplitudes + graph connectivity
 */
typedef struct quantum_walker_struct {
    quantum_amplitude_t* amplitudes;   /**< Quantum amplitudes αᵢ at each neuron */
    float* probabilities;              /**< Cached probabilities P(i) = |αᵢ|² */
    uint32_t num_nodes;                /**< Number of nodes (neurons) */

    // Graph structure (borrowed from neural network)
    neural_network_t network;          /**< Neural network (graph structure) */
    uint32_t** adjacency_list;         /**< Adjacency list for fast neighbor lookup */
    uint32_t* node_degrees;            /**< Degree of each node */

    // Quantum walk state
    quantum_walk_config_t config;      /**< Configuration parameters */
    uint32_t current_step;             /**< Current evolution step */
    uint32_t initial_node;             /**< Starting node ID */

    // Working buffers (reused to avoid allocations)
    quantum_amplitude_t* temp_amplitudes; /**< Temporary buffer for updates */

    // Statistics
    quantum_walk_stats_t stats;        /**< Walk statistics */
} quantum_walker_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create quantum walker on neural network
 *
 * WHAT: Allocate and initialize quantum walker
 * WHY: Set up quantum state on network graph
 * HOW: Allocate amplitudes, build adjacency lists
 *
 * COMPLEXITY: O(N + E) where N=nodes, E=edges
 *
 * @param network Neural network (provides graph structure)
 * @param config Walk configuration
 * @return Quantum walker, or NULL on failure
 */
quantum_walker_t* quantum_walk_create(
    neural_network_t network,
    const quantum_walk_config_t* config
);

/**
 * @brief Destroy quantum walker
 *
 * WHAT: Free all allocated memory
 * WHY: Prevent memory leaks
 * HOW: Free amplitudes, adjacency lists, walker structure
 *
 * @param walker Quantum walker to destroy
 */
void quantum_walk_destroy(quantum_walker_t* walker);

/**
 * @brief Clone quantum walker
 *
 * WHAT: Create independent copy
 * WHY: Parallel processing or checkpointing
 * HOW: Deep copy all arrays and state
 *
 * @param walker Source walker
 * @return Cloned walker, or NULL on failure
 */
quantum_walker_t* quantum_walk_clone(const quantum_walker_t* walker);

//=============================================================================
// Quantum Walk Operations
//=============================================================================

/**
 * @brief Initialize quantum walker at specific node
 *
 * WHAT: Set |ψ⟩ = |i⟩ (walker at node i)
 * WHY: Starting point for diffusion (e.g., reward source)
 * HOW: amplitude[i] = 1.0, all others = 0.0
 *
 * EXAMPLE:
 * ```c
 * quantum_walk_initialize(walker, reward_neuron_id);
 * // Result: Dopamine starts at reward neuron
 * ```
 *
 * @param walker Quantum walker
 * @param node_id Initial node ID
 * @return true on success, false on invalid node_id
 */
bool quantum_walk_initialize(quantum_walker_t* walker, uint32_t node_id);

/**
 * @brief Initialize with superposition across multiple nodes
 *
 * WHAT: Set |ψ⟩ = Σⱼ βⱼ|j⟩ (custom superposition)
 * WHY: Multiple sources (e.g., distributed reward)
 * HOW: Set amplitudes from input array, normalize
 *
 * @param walker Quantum walker
 * @param initial_amplitudes Initial amplitude for each node (can be NULL = uniform)
 * @return true on success, false on failure
 */
bool quantum_walk_initialize_superposition(
    quantum_walker_t* walker,
    const quantum_amplitude_t* initial_amplitudes
);

/**
 * @brief Single quantum walk step
 *
 * WHAT: Apply U = S × C (shift and coin operators)
 * WHY: Evolve quantum state one timestep
 * HOW: Coin operation, then shift along edges
 *
 * ALGORITHM:
 * ```
 * 1. Apply coin operator C at each node:
 *    α'ᵢ = C(αᵢ)  (Hadamard/Grover/Fourier)
 *
 * 2. Apply shift operator S along edges:
 *    α''ⱼ = Σᵢ∈neighbors(j) α'ᵢ / √degree(i)
 *
 * 3. Optional: Mix with classical diffusion
 *    α_final = (1-λ)×α''_quantum + λ×α_classical
 *
 * 4. Normalize: Σ|αᵢ|² = 1
 * ```
 *
 * COMPLEXITY: O(E) where E = number of edges
 *
 * @param walker Quantum walker
 * @return true on success, false on failure
 */
bool quantum_walk_step(quantum_walker_t* walker);

/**
 * @brief Evolve quantum walk for N steps
 *
 * WHAT: Apply quantum_walk_step() N times
 * WHY: Convenience function for full evolution
 * HOW: Loop over steps
 *
 * COMPLEXITY: O(N_steps × E)
 *
 * @param walker Quantum walker
 * @param num_steps Number of evolution steps
 * @return true on success, false on failure
 */
bool quantum_walk_evolve(quantum_walker_t* walker, uint32_t num_steps);

/**
 * @brief Reset walker to initial state
 *
 * WHAT: Clear amplitudes, reset to initial node
 * WHY: Reuse walker for new diffusion
 * HOW: Re-initialize at initial_node
 *
 * @param walker Quantum walker
 * @return true on success, false on failure
 */
bool quantum_walk_reset(quantum_walker_t* walker);

//=============================================================================
// Measurement and Output
//=============================================================================

/**
 * @brief Get probability distribution (measurement)
 *
 * WHAT: Extract P(i) = |αᵢ|² for all nodes
 * WHY: Convert quantum amplitudes to classical probabilities
 * HOW: Square magnitude of each amplitude
 *
 * USE CASE: Apply as neuromodulator concentration field
 *
 * @param walker Quantum walker
 * @param probabilities Output array (size: num_nodes, allocated by caller)
 * @return true on success, false on failure
 */
bool quantum_walk_get_distribution(
    const quantum_walker_t* walker,
    float* probabilities
);

/**
 * @brief Get quantum amplitudes (full state)
 *
 * WHAT: Return complex amplitudes αᵢ
 * WHY: Full quantum state (for visualization, analysis)
 * HOW: Copy amplitude array
 *
 * @param walker Quantum walker
 * @param amplitudes Output array (size: num_nodes, allocated by caller)
 * @return true on success, false on failure
 */
bool quantum_walk_get_amplitudes(
    const quantum_walker_t* walker,
    quantum_amplitude_t* amplitudes
);

/**
 * @brief Measure walker position (collapse to single node)
 *
 * WHAT: Sample from probability distribution
 * WHY: Classical measurement (collapses quantum state)
 * HOW: Random sample weighted by |αᵢ|²
 *
 * NOTE: This MODIFIES walker state (collapses to measured position)
 *
 * @param walker Quantum walker (state modified!)
 * @return Measured node ID
 */
uint32_t quantum_walk_measure(quantum_walker_t* walker);

//=============================================================================
// Statistics and Diagnostics
//=============================================================================

/**
 * @brief Compute quantum walk statistics
 *
 * WHAT: Calculate entropy, spreading, speedup estimate
 * WHY: Monitor walk quality and performance
 * HOW: Analyze probability distribution
 *
 * @param walker Quantum walker
 * @param stats Output statistics structure
 * @return true on success, false on failure
 */
bool quantum_walk_compute_stats(
    quantum_walker_t* walker,
    quantum_walk_stats_t* stats
);

/**
 * @brief Print quantum walk statistics
 *
 * WHAT: Human-readable output of walker state
 * WHY: Debugging and analysis
 * HOW: Print stats to stdout
 *
 * @param walker Quantum walker
 */
void quantum_walk_print_stats(const quantum_walker_t* walker);

/**
 * @brief Verify quantum walk integrity
 *
 * WHAT: Check probability conservation, normalization
 * WHY: Detect numerical errors or bugs
 * HOW: Verify Σ|αᵢ|² ≈ 1.0
 *
 * @param walker Quantum walker
 * @return true if valid, false if corrupted
 */
bool quantum_walk_verify(const quantum_walker_t* walker);

//=============================================================================
// Advanced Operations
//=============================================================================

/**
 * @brief Apply custom coin operator
 *
 * WHAT: User-defined unitary transformation
 * WHY: Experiment with custom quantum gates
 * HOW: Apply matrix multiplication at each node
 *
 * @param walker Quantum walker
 * @param coin_matrix Coin operator matrix (size: degree × degree)
 * @return true on success, false on failure
 */
bool quantum_walk_apply_custom_coin(
    quantum_walker_t* walker,
    const quantum_amplitude_t** coin_matrix
);

/**
 * @brief Hybrid quantum-classical step
 *
 * WHAT: Mix quantum walk + classical diffusion
 * WHY: Balance speedup and biological realism
 * HOW: α_new = (1-λ)×α_quantum + λ×α_classical
 *
 * @param walker Quantum walker
 * @param classical_weights Classical diffusion weights
 * @param mixing_ratio Mixing coefficient [0=quantum, 1=classical]
 * @return true on success, false on failure
 */
bool quantum_walk_hybrid_step(
    quantum_walker_t* walker,
    const float* classical_weights,
    float mixing_ratio
);

/**
 * @brief Apply decoherence (quantum → classical transition)
 *
 * WHAT: Add noise to quantum amplitudes
 * WHY: Model environmental decoherence
 * HOW: Mix with random phases, reduce coherence
 *
 * @param walker Quantum walker
 * @param decoherence_strength Decoherence rate [0=none, 1=complete]
 * @return true on success, false on failure
 */
bool quantum_walk_apply_decoherence(
    quantum_walker_t* walker,
    float decoherence_strength
);

//=============================================================================
// Monte Carlo Integration API
//=============================================================================

/* Forward declarations for QMC types */
struct qmc_measurement_result;
struct qmc_entropy_result;

/**
 * @brief Measure quantum walk using Monte Carlo importance sampling
 *
 * More efficient than linear cumulative search for large networks.
 * Uses binary search on cumulative distribution.
 *
 * @param walker Quantum walker
 * @return Measured node index
 */
uint32_t quantum_walk_measure_mc(quantum_walker_t* walker);

/**
 * @brief Simulate finite-shot measurement on quantum walk
 *
 * Models realistic quantum hardware with shot noise.
 *
 * @param walker Quantum walker
 * @param num_shots Number of measurements
 * @param result Output result (caller frees with qmc_measurement_result_free)
 * @return true on success
 */
bool quantum_walk_measure_finite_shots(
    quantum_walker_t* walker,
    uint32_t num_shots,
    struct qmc_measurement_result* result
);

/**
 * @brief Estimate entropy of quantum walk using Monte Carlo
 *
 * Faster than O(N) direct computation for large networks.
 *
 * @param walker Quantum walker
 * @param num_samples MC samples (0 = auto)
 * @param result Output entropy estimate
 * @return true on success
 */
bool quantum_walk_estimate_entropy_mc(
    quantum_walker_t* walker,
    uint32_t num_samples,
    struct qmc_entropy_result* result
);

/**
 * @brief Get thread-local MC seed for quantum walk operations
 *
 * @return Pointer to seed
 */
uint32_t* quantum_walk_get_mc_seed(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_QUANTUM_WALK_H
