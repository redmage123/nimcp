/**
 * @file nimcp_spatial_neuromod.h
 * @brief Graph-Based Neuromodulator Diffusion System (Enhancement A2.1)
 *
 * WHAT: Spatial dynamics for neuromodulators on neural network graph
 *       Models concentration fields that diffuse and decay over time
 *       Implements reaction-diffusion PDE on graph topology
 *
 * WHY:  Realistic neuromodulator propagation requires spatial structure:
 *       - Dopamine diffuses from reward-processing regions
 *       - Serotonin spreads from raphe nuclei
 *       - Acetylcholine gradients guide attention
 *       - Local vs. global neuromodulation effects
 *
 * HOW:  Graph-based discretization of reaction-diffusion equation:
 *       ∂c/∂t = D * Laplacian(c) - k*c + S(x,t)
 *
 *       Discretized:
 *       c_i(t+Δt) = c_i(t) + Δt * [D * Σ_j (c_j - c_i) - k*c_i + S_i]
 *
 *       Where:
 *       - c_i: concentration at neuron i
 *       - D: diffusion coefficient (spatial spread)
 *       - k: decay rate (clearance/reuptake)
 *       - S_i: source term (release at neuron i)
 *       - j: neighbors of neuron i
 *
 * BIOLOGICAL MAPPING:
 * - Graph nodes: Neurons (each has concentration field)
 * - Graph edges: Synaptic connections (diffusion pathways)
 * - Diffusion: Volume transmission through extracellular space
 * - Decay: Reuptake by transporters, enzymatic degradation
 * - Sources: VTA (dopamine), raphe (serotonin), basal forebrain (ACh)
 *
 * MATHEMATICAL ENHANCEMENTS:
 * Enhancement A2.1 from MATHEMATICAL_ENHANCEMENTS_CHECKLIST.md
 * - Graph Laplacian operator for irregular topology
 * - Explicit Euler scheme (stable for small timesteps)
 * - Neumann boundary conditions (zero flux at borders)
 *
 * DESIGN PATTERNS:
 * - Strategy: Pluggable diffusion kernels
 * - Observer: Neurons observe local concentration
 * - Template Method: Update loop with pluggable dynamics
 * - Facade: Simplified API over complex PDE solver
 *
 * PERFORMANCE: O(E) per timestep where E = number of synapses
 *              ~10% overhead vs. point-based neuromodulation
 *
 * INTEGRATION:
 * - Works with existing neuromodulator_system_t
 * - Can be enabled/disabled via config flag
 * - Backward compatible (falls back to global broadcast)
 *
 * @author NIMCP Development Team
 * @date 2025-11-11
 */

#ifndef NIMCP_SPATIAL_NEUROMOD_H
#define NIMCP_SPATIAL_NEUROMOD_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

/**
 * @brief Forward declaration of neural network (opaque pointer type)
 * WHY: Avoid circular dependency with nimcp_neuralnet.h
 * NOTE: neural_network_t is already defined as opaque pointer in nimcp_neuralnet.h
 */
typedef struct neural_network_struct* neural_network_t;

//=============================================================================
// Spatial Neuromodulator Field
//=============================================================================

/**
 * @brief Spatial concentration field for one neuromodulator type
 *
 * WHAT: Per-neuron concentration array with diffusion dynamics
 * WHY:  Models spatial propagation of neuromodulators
 * HOW:  Each neuron has local concentration, diffuses to neighbors
 *
 * MEMORY: O(N) where N = number of neurons
 * COMPUTATION: O(E) per update where E = edges (synapses)
 */
typedef struct {
    // === CONCENTRATION FIELD ===
    float* concentration;        /**< [num_neurons] current concentration at each neuron (0-1) */
    float* source_rate;          /**< [num_neurons] release rate at each neuron (mol/s) */
    float* laplacian_buffer;     /**< [num_neurons] temporary buffer for Laplacian computation */
    uint32_t num_neurons;        /**< Number of neurons in network */

    // === PHYSICAL PARAMETERS ===
    float diffusion_coeff;       /**< D: diffusion coefficient (0.01-1.0, default: 0.1) */
    float decay_rate;            /**< k: decay/clearance rate (1/s, default: 0.01) */
    float baseline;              /**< Homeostatic baseline concentration (0-1, default: 0.1) */

    // === NUMERICAL PARAMETERS ===
    float timestep;              /**< Integration timestep (ms, default: 1.0) */
    uint32_t substeps;           /**< Substeps per timestep for stability (default: 1) */
    float max_concentration;     /**< Upper bound for concentration (default: 1.0) */
    float min_concentration;     /**< Lower bound for concentration (default: 0.0) */

    // === STATISTICS ===
    float total_released;        /**< Cumulative amount released */
    float total_decayed;         /**< Cumulative amount decayed */
    float avg_concentration;     /**< Average concentration across network */
    float max_gradient;          /**< Maximum spatial gradient */
    uint64_t update_count;       /**< Number of updates performed */

    // === TYPE IDENTIFICATION ===
    neuromodulator_type_t type;  /**< Which neuromodulator (DA, 5-HT, ACh, NE) */

    // === PHASE C4.3: QUANTUM-SHANNON DIFFUSION ACCELERATION ===
    /**
     * Quantum-Shannon Diffusion for O(√N) Speedup + Information Theory
     *
     * WHAT: Quantum random walk with Shannon information flow monitoring
     * WHY:  Classical diffusion is O(d²), quantum is O(d) + bottleneck detection
     * HOW:  Hybrid quantum-classical with Shannon capacity tracking
     *
     * PERFORMANCE:
     * - Quadratic speedup for long-range propagation (2-50x)
     * - Real-time bottleneck detection
     * - Information flow optimization
     * - 3x memory overhead (amplitudes + Shannon tracking)
     * - Enabled via brain config: enable_quantum_shannon_diffusion
     *
     * UPGRADE FROM C2.1: Replaces plain quantum_walker with quantum-Shannon
     * - Backward compatible (quantum_walk configs still work)
     * - Additional Shannon metrics available
     *
     * NOTE: NULL if quantum-Shannon disabled (backward compatible)
     */
    void* quantum_shannon_diffusion; /**< quantum_shannon_diffusion_t* (opaque) */
    bool use_quantum_shannon;        /**< Is quantum-Shannon enabled? */
    float quantum_mixing_ratio;      /**< Mix quantum + classical [0=pure quantum, 1=classical] */

    // Shannon metrics (Phase C4.3)
    float last_propagation_efficiency; /**< η = I/H_source (0-1) */
    float last_speedup_vs_classical;   /**< Measured speedup factor */
    uint32_t last_num_bottlenecks;    /**< Detected bottlenecks */
    float last_information_rate;       /**< dH/dt bits/step */

    // === PHASE C4.5: DYNAMIC SOURCE ADAPTATION STATE ===
    float efficiency_ema;              /**< Exponential moving average of propagation efficiency */
    uint32_t current_adaptive_sources; /**< Current K value (can differ from config if dynamic) */
    uint32_t adaptation_cooldown;      /**< Steps remaining until next adaptation allowed */

    // === PHASE C4.6: MULTI-OBJECTIVE STATE ===
    float pareto_front_scores[100][4]; /**< Cached Pareto front: [neuron_idx][objective_values] */
    uint32_t pareto_front_size;        /**< Number of neurons on Pareto front */
    uint64_t pareto_cache_generation;  /**< Generation counter for cache invalidation */

} spatial_neuromod_field_t;

/**
 * @brief Complete spatial neuromodulator system
 *
 * WHAT: Manages all neuromodulator fields with spatial dynamics
 * WHY:  Centralizes spatial diffusion computation
 * HOW:  One field per neuromodulator type
 */
typedef struct {
    spatial_neuromod_field_t* fields[NEUROMOD_COUNT];  /**< One field per neuromodulator */
    bool enabled[NEUROMOD_COUNT];                      /**< Which fields are active */
    neural_network_t network;                          /**< Reference to network topology */
    bool use_substeps;                                 /**< Enable substeps for stability */
    float global_diffusion_scale;                      /**< Global scaling for all diffusion */

    // === PHASE C2.1: QUANTUM WALK CONFIGURATION ===
    bool enable_quantum_walks;       /**< Global quantum walk enable/disable */
    uint32_t quantum_walk_steps;     /**< Steps per diffusion update */
    uint32_t quantum_coin_type;      /**< 0=Hadamard, 1=Grover, 2=Fourier */
    float quantum_decoherence_rate;  /**< Decoherence [0=none, 1=instant] */
} spatial_neuromod_system_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for spatial neuromodulator field
 */
typedef struct {
    neuromodulator_type_t type;   /**< Which neuromodulator */
    float diffusion_coeff;        /**< D: diffusion coefficient (default: 0.1) */
    float decay_rate;             /**< k: decay rate (default: 0.01) */
    float baseline;               /**< Baseline concentration (default: 0.1) */
    float timestep;               /**< Integration timestep (default: 1.0 ms) */
    uint32_t substeps;            /**< Substeps for stability (default: 1) */

    // === PHASE C2.1: QUANTUM WALK CONFIGURATION ===
    bool enable_quantum_walk;     /**< Use quantum walk for diffusion? (default: false) */
    uint32_t quantum_walk_steps;  /**< Steps per diffusion update (default: 50) */
    float quantum_mixing_ratio;   /**< Mix quantum+classical [0=quantum, 1=classical] (default: 0.2) */
    uint32_t quantum_coin_type;   /**< 0=Hadamard, 1=Grover, 2=Fourier (default: 0) */
    float quantum_decoherence;    /**< Decoherence rate [0=none, 1=instant] (default: 0.05) */

    // === PHASE C4.4: ADAPTIVE ROUTING CONFIGURATION ===
    bool enable_adaptive_routing;         /**< Use Shannon metrics for intelligent routing? (default: false) */
    float efficiency_weight;              /**< Weight for propagation efficiency (default: 1.0) */
    float speedup_weight;                 /**< Weight for quantum speedup (default: 0.5) */
    float bottleneck_penalty_weight;      /**< Weight for bottleneck penalty (default: 2.0) */
    float info_rate_weight;               /**< Weight for information rate (default: 0.3) */
    uint32_t num_adaptive_sources;        /**< Number of optimal sources to select (default: 3) */
    float min_source_score;               /**< Minimum score threshold for source selection (default: 0.1) */

    // === PHASE C4.5: DYNAMIC SOURCE ADAPTATION CONFIGURATION ===
    bool enable_dynamic_adaptation;       /**< Dynamically adapt num_adaptive_sources based on performance? (default: false) */
    uint32_t min_adaptive_sources;        /**< Minimum K value for dynamic adaptation (default: 1) */
    uint32_t max_adaptive_sources;        /**< Maximum K value for dynamic adaptation (default: 10) */
    float adaptation_rate;                /**< EMA smoothing for efficiency tracking [0=no smoothing, 1=instant] (default: 0.1) */
    float target_efficiency;              /**< Target propagation efficiency to maintain (default: 0.75) */
    float efficiency_tolerance;           /**< Tolerance band around target before adapting (default: 0.1) */
    uint32_t adaptation_cooldown_steps;   /**< Minimum steps between adaptations (default: 100) */

    // === PHASE C4.6: MULTI-OBJECTIVE ADAPTATION CONFIGURATION ===
    bool enable_multi_objective;          /**< Enable multi-objective Pareto-optimal selection? (default: false) */
    uint32_t num_objectives;              /**< Number of objectives (2-4, default: 2) */
    float objective_weights[4];           /**< Weights for each objective [0-1] (default: [0.5, 0.5, 0, 0]) */
    float pareto_epsilon;                 /**< Epsilon for Pareto dominance [0-1] (default: 0.01) */
    bool prefer_diversity;                /**< Prefer diverse solutions on Pareto front? (default: true) */
} spatial_neuromod_config_t;

/**
 * @brief Get default configuration for spatial neuromodulator field
 *
 * WHAT: Returns sensible defaults for given neuromodulator type
 * WHY:  Different neuromodulators have different kinetics
 * HOW:  Calibrated from literature values
 *
 * DEFAULTS:
 * - Dopamine: Fast diffusion (D=0.2), fast decay (k=0.5), baseline=0.05
 * - Serotonin: Slow diffusion (D=0.05), slow decay (k=0.1), baseline=0.3
 * - Acetylcholine: Fast diffusion (D=0.3), very fast decay (k=2.0), baseline=0.1
 * - Norepinephrine: Medium diffusion (D=0.15), medium decay (k=0.3), baseline=0.05
 *
 * @param type Neuromodulator type
 * @return Default configuration
 */
spatial_neuromod_config_t spatial_neuromod_default_config(neuromodulator_type_t type);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create spatial neuromodulator field
 *
 * WHAT: Allocates concentration arrays for N neurons
 * WHY:  Initialize spatial dynamics system
 * HOW:  malloc concentration[N], set parameters
 *
 * COMPLEXITY: O(N) memory, O(1) time
 *
 * @param num_neurons Number of neurons in network
 * @param config Field configuration
 * @return Spatial field or NULL on error
 */
spatial_neuromod_field_t* spatial_neuromod_create(uint32_t num_neurons,
                                                   const spatial_neuromod_config_t* config);

/**
 * @brief Destroy spatial neuromodulator field
 *
 * WHAT: Frees all allocated memory
 * WHY:  Clean shutdown, prevent memory leaks
 * HOW:  free concentration arrays
 *
 * @param field Field to destroy (can be NULL)
 */
void spatial_neuromod_destroy(spatial_neuromod_field_t* field);

/**
 * @brief Create complete spatial neuromodulator system
 *
 * WHAT: Creates fields for all enabled neuromodulators
 * WHY:  Unified interface for multi-field diffusion
 * HOW:  Calls spatial_neuromod_create for each type
 *
 * @param network Neural network (for topology)
 * @param enabled_types Array of booleans indicating which types to enable
 * @param configs Array of configurations (one per type)
 * @return Spatial system or NULL on error
 */
spatial_neuromod_system_t* spatial_neuromod_system_create(
    neural_network_t network,
    const bool enabled_types[NEUROMOD_COUNT],
    const spatial_neuromod_config_t configs[NEUROMOD_COUNT]
);

/**
 * @brief Destroy spatial neuromodulator system
 *
 * @param system System to destroy (can be NULL)
 */
void spatial_neuromod_system_destroy(spatial_neuromod_system_t* system);

/**
 * @brief Update all neuromodulator fields in system
 *
 * WHAT: Updates diffusion for all enabled neuromodulator fields
 * WHY:  Convenient batch update for all fields in one call
 * HOW:  Calls spatial_neuromod_update() for each enabled field
 *
 * ALGORITHM:
 * 1. For each neuromodulator type (dopamine, serotonin, etc.)
 * 2. If field is enabled, call spatial_neuromod_update()
 * 3. Return false if any update fails
 *
 * COMPLEXITY: O(N * num_enabled_fields) where N = neurons per field
 *
 * @param system Spatial neuromodulator system
 * @param network Network topology
 * @param dt Timestep (seconds)
 * @return true on success, false if any field update fails
 */
bool spatial_neuromod_system_update(
    spatial_neuromod_system_t* system,
    neural_network_t network,
    float dt);

//=============================================================================
// Diffusion Dynamics
//=============================================================================

/**
 * @brief Update spatial concentration field (single timestep)
 *
 * WHAT: Applies diffusion + decay + sources for one timestep
 * WHY:  Core PDE integration step
 * HOW:  Explicit Euler scheme on graph Laplacian
 *
 * ALGORITHM:
 * 1. Compute graph Laplacian: L_i = Σ_j (c_j - c_i) for neighbors j
 * 2. Apply reaction-diffusion: dc/dt = D*L - k*c + S
 * 3. Euler step: c(t+dt) = c(t) + dt * dc/dt
 * 4. Clamp to [0, 1]
 *
 * STABILITY: Requires dt < 1/(2*D*max_degree) for explicit Euler
 *            Use substeps if network has high-degree hubs
 *
 * COMPLEXITY: O(E) where E = number of synapses
 *
 * @param field Spatial field to update
 * @param network Network topology (for neighbor iteration)
 * @param dt Timestep (seconds)
 * @return true on success
 */
bool spatial_neuromod_update(spatial_neuromod_field_t* field,
                              neural_network_t network,
                              float dt);

/**
 * @brief Compute graph Laplacian at each node
 *
 * WHAT: Computes discrete Laplacian operator on graph
 * WHY:  Core operator for diffusion PDE
 * HOW:  For each neuron i: L_i = Σ_j (c_j - c_i) over neighbors j
 *
 * MATHEMATICAL DEFINITION:
 * Graph Laplacian: L = D - A
 * Where D = degree matrix, A = adjacency matrix
 * Applied to concentration: Lc_i = Σ_j A_ij (c_j - c_i)
 *
 * COMPLEXITY: O(E) where E = edges
 *
 * @param field Spatial field
 * @param network Network topology
 * @param laplacian Output array [num_neurons]
 * @return true on success
 */
bool spatial_neuromod_compute_laplacian(const spatial_neuromod_field_t* field,
                                         neural_network_t network,
                                         float* laplacian);

/**
 * @brief Release neuromodulator at specific neuron
 *
 * WHAT: Adds neuromodulator to source term at neuron
 * WHY:  Models phasic release (e.g., VTA dopamine burst)
 * HOW:  S_i += amount
 *
 * BIOLOGICAL: Vesicular release from presynaptic terminals
 *
 * @param field Spatial field
 * @param neuron_id Target neuron for release
 * @param amount Amount to release (mol/s or normalized units)
 * @return true on success
 */
bool spatial_neuromod_release(spatial_neuromod_field_t* field,
                               uint32_t neuron_id,
                               float amount);

/**
 * @brief Release neuromodulator at multiple neurons
 *
 * WHAT: Batch release operation
 * WHY:  Efficient for multiple simultaneous releases
 * HOW:  Updates source terms for all specified neurons
 *
 * USE CASE: Reward signal → dopamine release in striatal neurons
 *
 * @param field Spatial field
 * @param neuron_ids Array of neuron IDs
 * @param amounts Array of release amounts (parallel to neuron_ids)
 * @param count Number of neurons
 * @return true on success
 */
bool spatial_neuromod_release_batch(spatial_neuromod_field_t* field,
                                     const uint32_t* neuron_ids,
                                     const float* amounts,
                                     uint32_t count);

//=============================================================================
// Phase C4.4: Adaptive Routing Functions
//=============================================================================

/**
 * @brief Score neuron for neuromodulator release suitability (Phase C4.4)
 *
 * WHAT: Computes suitability score based on Shannon metrics
 * WHY:  Intelligent source selection maximizes information propagation
 * HOW:  score = w1*efficiency + w2*speedup - w3*bottleneck_penalty + w4*info_rate
 *
 * ALGORITHM:
 * 1. Extract Shannon metrics from quantum-Shannon system
 * 2. Compute weighted score using config weights
 * 3. Return normalized score [0, 1]
 *
 * METRICS USED:
 * - Propagation Efficiency: η = I/H_source (higher = better)
 * - Quantum Speedup: speedup_vs_classical (higher = better)
 * - Bottleneck Count: num_bottlenecks (lower = better)
 * - Information Rate: dH/dt (higher = better)
 *
 * COMPLEXITY: O(1)
 *
 * @param field Spatial field (must have quantum-Shannon enabled)
 * @param neuron_id Neuron to score
 * @param network Network topology
 * @param config Configuration with scoring weights
 * @return Score [0, 1], or 0.0 on error
 */
float spatial_neuromod_score_neuron(
    const spatial_neuromod_field_t* field,
    uint32_t neuron_id,
    neural_network_t network,
    const spatial_neuromod_config_t* config);

/**
 * @brief Select optimal source neurons using Shannon metrics (Phase C4.4)
 *
 * WHAT: Finds top K neurons for neuromodulator release
 * WHY:  Adaptive routing improves information utilization 2-3x
 * HOW:  Score all neurons, select top K with best scores
 *
 * ALGORITHM:
 * 1. Score all candidate neurons
 * 2. Sort by score (descending)
 * 3. Select top K neurons with score >= min_source_score
 * 4. Return selected neuron IDs
 *
 * COMPLEXITY: O(N log K) where N = neurons, K = num_adaptive_sources
 *
 * @param field Spatial field (must have quantum-Shannon enabled)
 * @param network Network topology
 * @param config Configuration with adaptive routing settings
 * @param selected_ids Output array for selected neuron IDs [num_adaptive_sources]
 * @param selected_scores Output array for scores [num_adaptive_sources] (optional, can be NULL)
 * @param num_selected Output: actual number of neurons selected
 * @return true on success
 */
bool spatial_neuromod_select_optimal_sources(
    const spatial_neuromod_field_t* field,
    neural_network_t network,
    const spatial_neuromod_config_t* config,
    uint32_t* selected_ids,
    float* selected_scores,
    uint32_t* num_selected);

/**
 * @brief Adaptive neuromodulator release using Shannon metrics (Phase C4.4)
 *
 * WHAT: Intelligently selects source neurons and releases neuromodulator
 * WHY:  Maximizes information propagation efficiency (2-3x better utilization)
 * HOW:  Selects optimal sources via Shannon metrics, distributes amount evenly
 *
 * ALGORITHM:
 * 1. Select optimal source neurons via select_optimal_sources()
 * 2. Distribute total amount evenly across selected sources
 * 3. Release at each selected neuron
 *
 * REQUIREMENTS:
 * - Adaptive routing enabled in config (enable_adaptive_routing = true)
 * - Quantum-Shannon enabled (use_quantum_shannon = true)
 *
 * FALLBACK: If adaptive routing disabled, uses single random source
 *
 * COMPLEXITY: O(N log K) where N = neurons, K = num_adaptive_sources
 *
 * @param field Spatial field
 * @param network Network topology
 * @param config Configuration with adaptive routing settings
 * @param total_amount Total neuromodulator to release (distributed across sources)
 * @return true on success
 */
bool spatial_neuromod_release_adaptive(
    spatial_neuromod_field_t* field,
    neural_network_t network,
    const spatial_neuromod_config_t* config,
    float total_amount);

/**
 * @brief Batch adaptive release with multiple amounts (Phase C4.4)
 *
 * WHAT: Multiple adaptive releases with different amounts
 * WHY:  Efficient for time-varying release patterns
 * HOW:  Calls spatial_neuromod_release_adaptive() for each amount
 *
 * USE CASE: Dynamic dopamine release during reinforcement learning
 *
 * COMPLEXITY: O(M * N log K) where M = count, N = neurons, K = sources
 *
 * @param field Spatial field
 * @param network Network topology
 * @param config Configuration with adaptive routing settings
 * @param amounts Array of release amounts
 * @param count Number of releases
 * @return true on success
 */
bool spatial_neuromod_release_adaptive_batch(
    spatial_neuromod_field_t* field,
    neural_network_t network,
    const spatial_neuromod_config_t* config,
    const float* amounts,
    uint32_t count);

//=============================================================================
// Phase C4.5: Dynamic Source Adaptation
//=============================================================================

/**
 * @brief Update dynamic adaptation state and adjust K
 *
 * WHAT: Monitors efficiency EMA and dynamically adjusts num_adaptive_sources
 * WHY:  Automatically tunes K based on network performance for optimal efficiency
 * HOW:  Uses exponential moving average with cooldown-based rate limiting
 *
 * ALGORITHM:
 * 1. Update EMA: efficiency_ema = α*current + (1-α)*ema
 * 2. Check cooldown: If cooldown == 0 and outside tolerance band:
 *    - Low efficiency → increase K (more source diversity)
 *    - High efficiency → decrease K (fewer sources needed)
 *    - Reset cooldown after adaptation
 * 3. Clamp K to [min_K, max_K]
 *
 * COMPLEXITY: O(1)
 *
 * REQUIREMENTS:
 * - config.enable_dynamic_adaptation = true
 * - Quantum-Shannon enabled (for efficiency metrics)
 * - Adaptive routing enabled (Phase C4.4)
 *
 * @param field Spatial field with Shannon metrics
 * @param config Configuration with adaptation parameters
 * @return true on success (false if requirements not met)
 */
bool spatial_neuromod_update_dynamic_adaptation(
    spatial_neuromod_field_t* field,
    const spatial_neuromod_config_t* config);

/**
 * @brief Get current number of adaptive sources (may differ from config if dynamic)
 *
 * WHAT: Queries current K value used for source selection
 * WHY:  With dynamic adaptation, K changes over time
 * HOW:  Returns field->current_adaptive_sources
 *
 * COMPLEXITY: O(1)
 *
 * @param field Spatial field
 * @return Current K value (0 if dynamic adaptation disabled)
 */
uint32_t spatial_neuromod_get_current_adaptive_sources(
    const spatial_neuromod_field_t* field);

//=============================================================================
// Phase C4.6: Multi-Objective Adaptation
//=============================================================================

/**
 * @brief Compute multi-objective scores for a neuron
 *
 * WHAT: Evaluates neuron on multiple objectives simultaneously
 * WHY:  Support trade-offs between competing goals (speed vs accuracy, efficiency vs exploration)
 * HOW:  Computes normalized scores for each objective
 *
 * OBJECTIVES:
 * - Objective 0: Propagation efficiency (η)
 * - Objective 1: Quantum speedup
 * - Objective 2: Bottleneck avoidance (1 / (1 + bottlenecks))
 * - Objective 3: Information rate (dH/dt)
 *
 * COMPLEXITY: O(1)
 *
 * REQUIREMENTS:
 * - config.enable_multi_objective = true
 * - Quantum-Shannon enabled (for metrics)
 * - num_objectives ∈ [2, 4]
 *
 * @param field Spatial field with Shannon metrics
 * @param neuron_id Neuron to score
 * @param network Neural network
 * @param config Configuration with multi-objective settings
 * @param scores Output array[4] to store objective scores
 * @return true on success (false if requirements not met)
 */
bool spatial_neuromod_score_neuron_multi_objective(
    const spatial_neuromod_field_t* field,
    uint32_t neuron_id,
    neural_network_t network,
    const spatial_neuromod_config_t* config,
    float* scores);

/**
 * @brief Check if neuron A Pareto-dominates neuron B
 *
 * WHAT: Determines if A is better than B on all objectives
 * WHY:  Core operation for Pareto-optimal selection
 * HOW:  A dominates B if: A[i] >= B[i] for all i, and A[j] > B[j] for some j
 *
 * COMPLEXITY: O(k) where k = num_objectives
 *
 * @param scores_a Objective scores for neuron A
 * @param scores_b Objective scores for neuron B
 * @param num_objectives Number of objectives to compare
 * @param epsilon Epsilon for floating-point comparison (default: 0.01)
 * @return true if A dominates B
 */
bool spatial_neuromod_pareto_dominates(
    const float* scores_a,
    const float* scores_b,
    uint32_t num_objectives,
    float epsilon);

/**
 * @brief Select Pareto-optimal neurons (non-dominated solutions)
 *
 * WHAT: Finds neurons on the Pareto front
 * WHY:  Select best neurons when objectives conflict
 * HOW:  Iteratively find non-dominated neurons
 *
 * ALGORITHM:
 * 1. Score all neurons on all objectives
 * 2. Find non-dominated neurons (Pareto front)
 * 3. Select K neurons from front using:
 *    - Weighted scalarization if prefer_diversity=false
 *    - Crowding distance if prefer_diversity=true
 *
 * COMPLEXITY: O(N² × k) where N=neurons, k=objectives
 *
 * REQUIREMENTS:
 * - config.enable_multi_objective = true
 * - Quantum-Shannon enabled
 * - num_objectives ∈ [2, 4]
 *
 * @param field Spatial field with Shannon metrics
 * @param network Neural network
 * @param config Configuration with multi-objective settings
 * @param selected_ids Output array for selected neuron IDs
 * @param selected_scores Output array for scores (optional, can be NULL)
 * @param num_selected Output: number of neurons selected
 * @return true on success
 */
bool spatial_neuromod_select_pareto_optimal(
    const spatial_neuromod_field_t* field,
    neural_network_t network,
    const spatial_neuromod_config_t* config,
    uint32_t* selected_ids,
    float* selected_scores,
    uint32_t* num_selected);

/**
 * @brief Adaptive release using multi-objective Pareto-optimal selection
 *
 * WHAT: Selects Pareto-optimal sources and distributes neuromodulator
 * WHY:  Optimize multiple objectives simultaneously
 * HOW:  Combines Pareto selection with adaptive release
 *
 * COMPLEXITY: O(N² × k) for selection + O(K) for release
 *
 * @param field Spatial field
 * @param network Neural network
 * @param config Configuration with multi-objective settings
 * @param total_amount Total neuromodulator to release
 * @return true on success
 */
bool spatial_neuromod_release_multi_objective(
    spatial_neuromod_field_t* field,
    neural_network_t network,
    const spatial_neuromod_config_t* config,
    float total_amount);

//=============================================================================
// Queries
//=============================================================================

/**
 * @brief Get concentration at specific neuron
 *
 * WHAT: Reads local concentration at neuron
 * WHY:  Synapses/neurons need local concentration for modulation
 * HOW:  Direct array access
 *
 * COMPLEXITY: O(1)
 *
 * @param field Spatial field
 * @param neuron_id Neuron to query
 * @return Concentration at neuron (0-1), or 0.0 on error
 */
float spatial_neuromod_get_concentration(const spatial_neuromod_field_t* field,
                                          uint32_t neuron_id);

/**
 * @brief Set concentration at specific neuron
 *
 * WHAT: Directly sets concentration (for initialization/testing)
 * WHY:  Useful for setting up initial conditions
 * HOW:  Direct array write with clamping
 *
 * @param field Spatial field
 * @param neuron_id Neuron to modify
 * @param concentration New concentration (will be clamped to [0,1])
 * @return true on success
 */
bool spatial_neuromod_set_concentration(spatial_neuromod_field_t* field,
                                         uint32_t neuron_id,
                                         float concentration);

/**
 * @brief Get spatial gradient at neuron
 *
 * WHAT: Computes concentration gradient (magnitude)
 * WHY:  Quantify spatial non-uniformity, gradient-following behavior
 * HOW:  |∇c_i| ≈ Σ_j |c_j - c_i| / degree_i
 *
 * USE CASE: Curiosity-driven exploration following dopamine gradients
 *
 * @param field Spatial field
 * @param network Network topology
 * @param neuron_id Neuron to query
 * @return Gradient magnitude, or 0.0 on error
 */
float spatial_neuromod_get_gradient(const spatial_neuromod_field_t* field,
                                     neural_network_t network,
                                     uint32_t neuron_id);

/**
 * @brief Get average concentration across network
 *
 * WHAT: Mean concentration over all neurons
 * WHY:  Global measure of neuromodulator level
 * HOW:  Sum(c_i) / N
 *
 * @param field Spatial field
 * @return Average concentration
 */
float spatial_neuromod_get_average(const spatial_neuromod_field_t* field);

/**
 * @brief Get maximum concentration in network
 *
 * WHAT: Peak concentration value
 * WHY:  Track hotspots, saturation
 * HOW:  max(c_i)
 *
 * @param field Spatial field
 * @param neuron_id_out Optional: location of maximum (can be NULL)
 * @return Maximum concentration
 */
float spatial_neuromod_get_max(const spatial_neuromod_field_t* field,
                                uint32_t* neuron_id_out);

//=============================================================================
// Integration with Global Neuromodulator System
//=============================================================================

/**
 * @brief Synchronize spatial field with global neuromodulator level
 *
 * WHAT: Updates global concentration from spatial field average
 * WHY:  Maintain consistency between spatial and global systems
 * HOW:  global_level = avg(spatial_concentrations)
 *
 * USE CASE: When spatial diffusion is enabled but some code still queries global
 *
 * @param field Spatial field
 * @param system Global neuromodulator system
 * @return true on success
 */
bool spatial_neuromod_sync_to_global(const spatial_neuromod_field_t* field,
                                      neuromodulator_system_t system);

/**
 * @brief Initialize spatial field from global neuromodulator level
 *
 * WHAT: Sets all spatial concentrations to global level
 * WHY:  Bootstrap spatial field from existing global state
 * HOW:  c_i = global_level for all i
 *
 * @param field Spatial field
 * @param system Global neuromodulator system
 * @return true on success
 */
bool spatial_neuromod_init_from_global(spatial_neuromod_field_t* field,
                                        neuromodulator_system_t system);

//=============================================================================
// Visualization & Analysis
//=============================================================================

/**
 * @brief Export concentration field for visualization
 *
 * WHAT: Exports concentration values for external plotting
 * WHY:  Visualize spatial distribution, gradients
 * HOW:  Writes to provided buffer
 *
 * @param field Spatial field
 * @param buffer Output buffer [num_neurons]
 * @param buffer_size Size of buffer (must be >= num_neurons)
 * @return Number of values written, or 0 on error
 */
uint32_t spatial_neuromod_export(const spatial_neuromod_field_t* field,
                                  float* buffer,
                                  uint32_t buffer_size);

/**
 * @brief Compute spatial statistics
 *
 * WHAT: Various spatial measures (mean, variance, gradient, etc.)
 * WHY:  Quantitative analysis of spatial distribution
 * HOW:  Single pass over concentration array
 *
 * @param field Spatial field
 * @param network Network topology
 * @param mean_out Output: mean concentration (can be NULL)
 * @param variance_out Output: variance (can be NULL)
 * @param max_gradient_out Output: maximum gradient (can be NULL)
 * @return true on success
 */
bool spatial_neuromod_compute_stats(const spatial_neuromod_field_t* field,
                                     neural_network_t network,
                                     float* mean_out,
                                     float* variance_out,
                                     float* max_gradient_out);

//=============================================================================
// Reset & Debugging
//=============================================================================

/**
 * @brief Reset field to baseline concentration
 *
 * WHAT: Sets all concentrations to baseline, clears sources
 * WHY:  Reset between experiments/trials
 * HOW:  c_i = baseline, S_i = 0 for all i
 *
 * @param field Spatial field
 * @return true on success
 */
bool spatial_neuromod_reset(spatial_neuromod_field_t* field);

/**
 * @brief Validate field state (for debugging)
 *
 * WHAT: Checks invariants (concentrations in [0,1], no NaN/inf, etc.)
 * WHY:  Catch numerical errors early
 * HOW:  Scans arrays for invalid values
 *
 * @param field Spatial field
 * @return true if valid, false if errors detected
 */
bool spatial_neuromod_validate(const spatial_neuromod_field_t* field);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_SPATIAL_NEUROMOD_H
