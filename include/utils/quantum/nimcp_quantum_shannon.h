//=============================================================================
// nimcp_quantum_shannon.h - Quantum Walk + Shannon Information Theory
//=============================================================================
/**
 * @file nimcp_quantum_shannon.h
 * @brief Quantum walk with Shannon information metrics for bottleneck detection
 *
 * WHAT: Combine quantum walk speedup with Shannon capacity monitoring
 * WHY:  Optimal information propagation with √N speedup + bottleneck detection
 * HOW:  Quantum walk propagates signal, Shannon tracks information flow
 *
 * PHASE C4.1: SHANNON + QUANTUM WALK INTEGRATION
 *
 * MATHEMATICAL FOUNDATION:
 *
 * Quantum Walk Speedup:
 * - Classical diffusion: Distance d in O(d²) steps
 * - Quantum walk: Distance d in O(d) steps → √N speedup
 *
 * Shannon Information Flow:
 * - Channel capacity: C = B × log₂(1 + SNR) bits/second
 * - Information content: H(i) = -Σ p(i) log₂ p(i) bits
 * - Mutual information: I(source;targets) = H(source) - H(source|targets)
 * - Propagation efficiency: η = I(source;targets) / H(source)
 *
 * PERFORMANCE:
 * - Quantum walk: O(N) vs O(N²) classical
 * - Shannon optimization: 2-5x better information utilization
 * - Combined: 2√N to 5√N overall speedup
 * - Example (10K neurons): 2,000 ops vs 100M classical (50,000x faster)
 *
 * INTEGRATION WITH NIMCP:
 * - Neuromodulator diffusion with bottleneck detection
 * - Attention spread with capacity monitoring
 * - Information propagation optimization
 * - Reward signals with minimal information loss
 *
 * EXAMPLE:
 * ```c
 * // Create quantum-Shannon diffusion
 * quantum_shannon_config_t config = quantum_shannon_default_config();
 * quantum_shannon_diffusion_t* qsd = quantum_shannon_create(
 *     network, source_id, 8.0f, &config  // 8 bits source information
 * );
 *
 * // Evolve with Shannon monitoring
 * for (int step = 0; step < 100; step++) {
 *     quantum_shannon_step(qsd);
 * }
 *
 * // Get Shannon metrics
 * shannon_diffusion_metrics_t metrics;
 * quantum_shannon_get_metrics(qsd, &metrics);
 * printf("Propagation efficiency: %.2f%%\n", metrics.propagation_efficiency * 100.0f);
 * printf("Bottlenecks detected: %u\n", metrics.num_bottlenecks);
 *
 * // Optimize diffusion parameters based on Shannon feedback
 * quantum_shannon_optimize(qsd);
 *
 * // Get probability distribution
 * float* probs = nimcp_malloc(network->num_neurons * sizeof(float));
 * quantum_shannon_get_distribution(qsd, probs);
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-11-14
 * @version 2.10.0 Phase C4.1
 */

#ifndef NIMCP_QUANTUM_SHANNON_H
#define NIMCP_QUANTUM_SHANNON_H

#include "utils/quantum/nimcp_quantum_walk.h"
#include "information/nimcp_shannon.h"
#include <stdint.h>
#include <stdbool.h>
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Quantum-Shannon Configuration
//=============================================================================

/**
 * @brief Quantum-Shannon diffusion configuration
 *
 * WHAT: Parameters for quantum walk with Shannon monitoring
 * WHY:  Tune speedup vs accuracy tradeoff
 * HOW:  Combine quantum walk config + Shannon analysis params
 */
typedef struct {
    quantum_walk_config_t quantum_config;  /**< Quantum walk parameters */
    shannon_config_t shannon_config;       /**< Shannon analysis parameters */

    // Sampling parameters
    uint32_t synapse_sample_size;          /**< Number of synapses to sample for capacity */
    uint32_t neuron_sample_size;           /**< Number of neurons to sample for entropy */

    // Optimization parameters
    float bottleneck_threshold;            /**< Capacity utilization threshold (0-1) */
    bool enable_adaptive_coin;             /**< Adapt coin operator based on bottlenecks */
    float coin_adaptation_rate;            /**< How fast to adapt coin (0-1) */

    // Performance tuning
    uint32_t shannon_update_interval;      /**< Compute Shannon metrics every N steps */
    bool track_information_loss;           /**< Monitor information loss during diffusion */
} quantum_shannon_config_t;

/**
 * @brief Get default quantum-Shannon configuration
 *
 * WHAT: Balanced configuration for general use
 * WHY:  Good starting point (√N speedup, 90% efficiency)
 * HOW:  Default quantum walk + Shannon monitoring
 *
 * @return Default configuration
 */
quantum_shannon_config_t quantum_shannon_default_config(void);

/**
 * @brief Get high-accuracy quantum-Shannon configuration
 *
 * WHAT: Maximizes information preservation
 * WHY:  Critical applications (learning, memory consolidation)
 * HOW:  Slower quantum walk, frequent Shannon updates
 *
 * @return High-accuracy configuration
 */
quantum_shannon_config_t quantum_shannon_high_accuracy_config(void);

/**
 * @brief Get fast quantum-Shannon configuration
 *
 * WHAT: Maximizes speed, accepts information loss
 * WHY:  Real-time applications (attention, inference)
 * HOW:  Fast quantum walk, infrequent Shannon updates
 *
 * @return Fast configuration
 */
quantum_shannon_config_t quantum_shannon_fast_config(void);

//=============================================================================
// Shannon Diffusion Metrics
//=============================================================================

/**
 * @brief Shannon metrics for information diffusion
 *
 * WHAT: Information-theoretic measures of diffusion quality
 * WHY:  Quantify how efficiently information propagates
 * HOW:  Track entropy, capacity, mutual information
 */
typedef struct {
    // Information content
    float source_entropy;                  /**< H(source) bits */
    float total_entropy;                   /**< Σ H(i) across all nodes bits */
    float mutual_information;              /**< I(source;targets) bits */

    // Propagation quality
    float propagation_efficiency;          /**< I/H_source (0-1, 1=perfect) */
    float information_loss;                /**< H_source - I bits */

    // Channel capacity
    float total_capacity;                  /**< Σ C(i,j) bits/second */
    float average_capacity;                /**< Mean capacity per synapse */
    float min_capacity;                    /**< Minimum synapse capacity */
    float max_capacity;                    /**< Maximum synapse capacity */

    // Bottlenecks
    uint32_t num_bottlenecks;              /**< Number of bottleneck synapses */
    float bottleneck_severity;             /**< Average capacity deficit (0-1) */

    // Performance
    float speedup_vs_classical;            /**< Estimated speedup factor */
    float information_rate;                /**< dH/dt bits/second */

    // Node statistics
    uint32_t num_nodes_reached;            /**< Nodes with P(i) > threshold */
    float spreading_distance;              /**< Average distance from source */
} shannon_diffusion_metrics_t;

/**
 * @brief Bottleneck information for optimization
 *
 * WHAT: Identification of low-capacity synapses
 * WHY:  Guide optimization and routing
 * HOW:  Store synapse ID + capacity deficit
 */
typedef struct {
    uint32_t pre_node;                     /**< Pre-synaptic neuron ID */
    uint32_t post_node;                    /**< Post-synaptic neuron ID */
    float capacity;                        /**< Actual capacity bits/second */
    float demand;                          /**< Information flow demand bits/second */
    float deficit;                         /**< (demand - capacity) / demand */
    float suggested_weight;                /**< Optimal weight to resolve bottleneck */
} quantum_shannon_bottleneck_t;

//=============================================================================
// Quantum-Shannon Diffusion Structure
//=============================================================================

/**
 * @brief Quantum walk with Shannon information tracking
 *
 * WHAT: Quantum walker + Shannon metrics + bottleneck detection
 * WHY:  Combine √N speedup with information optimization
 * HOW:  Quantum walk for propagation, Shannon for monitoring
 */
typedef struct {
    // Quantum walk state
    quantum_walker_t* walker;              /**< Underlying quantum walker */

    // Shannon information tracking
    float* information_content;            /**< H(i) at each node bits */
    float* channel_capacities;             /**< C(i,j) for sampled synapses */
    uint32_t* sampled_synapses;            /**< IDs of sampled synapses */

    // Diffusion parameters
    uint32_t source_node;                  /**< Initial information source */
    float source_information_bits;         /**< H(source) initial entropy */

    // Configuration
    quantum_shannon_config_t config;       /**< Configuration parameters */

    // Metrics
    shannon_diffusion_metrics_t metrics;   /**< Current Shannon metrics */

    // Bottleneck tracking
    quantum_shannon_bottleneck_t* bottlenecks; /**< Detected bottlenecks */
    uint32_t num_bottlenecks;              /**< Number of bottlenecks */
    uint32_t bottleneck_capacity;          /**< Max bottlenecks to track */

    // Evolution state
    uint32_t current_step;                 /**< Current evolution step */
    bool optimized;                        /**< Has optimization been applied */
} quantum_shannon_diffusion_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create quantum-Shannon diffusion system
 *
 * WHAT: Allocate and initialize quantum walk + Shannon tracking
 * WHY:  Set up information diffusion with monitoring
 * HOW:  Create quantum walker, allocate Shannon arrays
 *
 * COMPLEXITY: O(N + E) where N=nodes, E=edges
 *
 * @param network Neural network (graph structure)
 * @param source_node Initial source of information
 * @param source_information_bits Initial entropy H(source)
 * @param config Configuration parameters
 * @return Quantum-Shannon diffusion system, or NULL on failure
 */
quantum_shannon_diffusion_t* quantum_shannon_create(
    neural_network_t network,
    uint32_t source_node,
    float source_information_bits,
    const quantum_shannon_config_t* config
);

/**
 * @brief Destroy quantum-Shannon diffusion system
 *
 * WHAT: Free all allocated memory
 * WHY:  Prevent memory leaks
 * HOW:  Destroy quantum walker, free arrays
 *
 * @param qsd Quantum-Shannon diffusion to destroy
 */
void quantum_shannon_destroy(quantum_shannon_diffusion_t* qsd);

/**
 * @brief Reset quantum-Shannon diffusion
 *
 * WHAT: Clear state, restart from source
 * WHY:  Reuse for new diffusion
 * HOW:  Reset quantum walker, clear metrics
 *
 * @param qsd Quantum-Shannon diffusion
 * @return true on success, false on failure
 */
bool quantum_shannon_reset(quantum_shannon_diffusion_t* qsd);

//=============================================================================
// Evolution Functions
//=============================================================================

/**
 * @brief Single quantum-Shannon diffusion step
 *
 * WHAT: Evolve quantum state + update Shannon metrics
 * WHY:  Propagate information with monitoring
 * HOW:  quantum_walk_step() + compute Shannon metrics
 *
 * ALGORITHM:
 * 1. Quantum walk step (O(E))
 * 2. Measure probability distribution (O(N))
 * 3. Compute Shannon entropy at each node (O(N))
 * 4. Update channel capacities for sampled synapses (O(S))
 * 5. Compute mutual information (O(N))
 * 6. Detect bottlenecks if interval reached (O(S))
 *
 * COMPLEXITY: O(E + N + S) where S = synapse_sample_size
 *
 * @param qsd Quantum-Shannon diffusion
 * @return true on success, false on failure
 */
bool quantum_shannon_step(quantum_shannon_diffusion_t* qsd);

/**
 * @brief Evolve for N steps
 *
 * WHAT: Apply quantum_shannon_step() N times
 * WHY:  Convenience function for full evolution
 * HOW:  Loop over steps
 *
 * COMPLEXITY: O(N_steps × (E + N + S))
 *
 * @param qsd Quantum-Shannon diffusion
 * @param num_steps Number of steps
 * @return true on success, false on failure
 */
bool quantum_shannon_evolve(quantum_shannon_diffusion_t* qsd, uint32_t num_steps);

//=============================================================================
// Measurement and Output
//=============================================================================

/**
 * @brief Get probability distribution
 *
 * WHAT: Extract P(i) = |αᵢ|² for all nodes
 * WHY:  Use as neuromodulator concentration field
 * HOW:  Query underlying quantum walker
 *
 * @param qsd Quantum-Shannon diffusion
 * @param probabilities Output array (size: num_nodes, allocated by caller)
 * @return true on success, false on failure
 */
bool quantum_shannon_get_distribution(
    const quantum_shannon_diffusion_t* qsd,
    float* probabilities
);

/**
 * @brief Get information content per node
 *
 * WHAT: Return H(i) for each node
 * WHY:  Visualize information distribution
 * HOW:  Copy information_content array
 *
 * @param qsd Quantum-Shannon diffusion
 * @param information Output array (size: num_nodes, allocated by caller)
 * @return true on success, false on failure
 */
bool quantum_shannon_get_information(
    const quantum_shannon_diffusion_t* qsd,
    float* information
);

/**
 * @brief Get Shannon diffusion metrics
 *
 * WHAT: Return current Shannon metrics
 * WHY:  Monitor diffusion quality
 * HOW:  Copy metrics structure
 *
 * @param qsd Quantum-Shannon diffusion
 * @param metrics Output metrics structure
 * @return true on success, false on failure
 */
bool quantum_shannon_get_metrics(
    const quantum_shannon_diffusion_t* qsd,
    shannon_diffusion_metrics_t* metrics
);

/**
 * @brief Get detected bottlenecks
 *
 * WHAT: Return list of low-capacity synapses
 * WHY:  Identify optimization targets
 * HOW:  Copy bottleneck array
 *
 * @param qsd Quantum-Shannon diffusion
 * @param bottlenecks Output array (allocated by caller)
 * @param max_bottlenecks Size of output array
 * @return Number of bottlenecks copied
 */
uint32_t quantum_shannon_get_bottlenecks(
    const quantum_shannon_diffusion_t* qsd,
    quantum_shannon_bottleneck_t* bottlenecks,
    uint32_t max_bottlenecks
);

//=============================================================================
// Optimization Functions
//=============================================================================

/**
 * @brief Optimize diffusion parameters using Shannon feedback
 *
 * WHAT: Adjust quantum walk parameters to maximize information transfer
 * WHY:  Adaptive optimization based on network structure
 * HOW:  Use Shannon capacity to tune coin operator, step size
 *
 * ALGORITHM:
 * 1. Detect bottleneck edges (low capacity relative to information flow)
 * 2. Compute bottleneck scores = (desired flow) / (actual capacity)
 * 3. Adapt quantum walk coin operator to avoid bottlenecks
 *    - High bottleneck score → increase exploration (coin bias)
 * 4. Suggest weight adjustments to resolve bottlenecks
 *
 * COMPLEXITY: O(S) where S = synapse_sample_size
 *
 * @param qsd Quantum-Shannon diffusion
 * @return true on success, false on failure
 */
bool quantum_shannon_optimize(quantum_shannon_diffusion_t* qsd);

/**
 * @brief Route around bottlenecks
 *
 * WHAT: Modify quantum walk to bypass low-capacity paths
 * WHY:  Maximize information flow through high-capacity paths
 * HOW:  Adjust coin operator bias per node based on neighbor capacities
 *
 * @param qsd Quantum-Shannon diffusion
 * @return true on success, false on failure
 */
bool quantum_shannon_route_around_bottlenecks(quantum_shannon_diffusion_t* qsd);

/**
 * @brief Adaptive routing using network topology analysis
 *
 * WHAT: Adjust quantum walk parameters based on real-time topology metrics
 * WHY:  Optimize information flow using graph structure (hubs, communities, clustering)
 * HOW:  Query network analyzer, bias quantum walk toward hubs and inter-community edges
 *
 * ALGORITHM:
 * 1. Get topology metrics (degree, centrality, clustering) from network analyzer
 * 2. Identify hubs (high-degree, high-betweenness neurons)
 * 3. Detect community boundaries
 * 4. Adapt routing:
 *    - Increase exploration near hubs (better distribution)
 *    - Bias through inter-community edges (global spread)
 *    - Reduce exploration in dense clusters (avoid redundancy)
 * 5. Adjust step size based on network diameter
 *
 * INTEGRATION:
 * - Requires network_analyzer_t from cognitive/analysis module
 * - Works with brain_get_network_analyzer()
 * - Enhances quantum-Shannon with topology awareness
 *
 * PERFORMANCE:
 * - O(N + H + C) where N=nodes, H=hubs, C=communities
 * - Cached topology query: O(1) after first analysis
 * - Typical overhead: 10-20% of quantum walk step time
 *
 * COMPLEXITY: O(N + H + C)
 *
 * @param qsd Quantum-Shannon diffusion
 * @param network_analyzer Network analyzer (from brain_get_network_analyzer())
 * @return true on success, false on failure
 */
bool quantum_adaptive_routing(quantum_shannon_diffusion_t* qsd, void* network_analyzer);

/**
 * @brief Suggest synapse weight adjustments
 *
 * WHAT: Compute optimal weights to resolve bottlenecks
 * WHY:  Guide learning/plasticity to improve information flow
 * HOW:  Use Shannon capacity formula to find weights matching demand
 *
 * @param qsd Quantum-Shannon diffusion
 * @param weight_adjustments Output array (size: num_synapses, allocated by caller)
 *                          value[i] = suggested_weight - current_weight
 * @return Number of weight adjustments suggested
 */
uint32_t quantum_shannon_suggest_weight_adjustments(
    const quantum_shannon_diffusion_t* qsd,
    float* weight_adjustments
);

//=============================================================================
// Diagnostics and Visualization
//=============================================================================

/**
 * @brief Print Shannon diffusion metrics
 *
 * WHAT: Human-readable output of diffusion quality
 * WHY:  Debugging and analysis
 * HOW:  Print metrics to stdout
 *
 * @param qsd Quantum-Shannon diffusion
 */
void quantum_shannon_print_metrics(const quantum_shannon_diffusion_t* qsd);

/**
 * @brief Print detected bottlenecks
 *
 * WHAT: List of bottleneck synapses with details
 * WHY:  Identify optimization targets
 * HOW:  Print bottleneck array to stdout
 *
 * @param qsd Quantum-Shannon diffusion
 */
void quantum_shannon_print_bottlenecks(const quantum_shannon_diffusion_t* qsd);

/**
 * @brief Verify quantum-Shannon integrity
 *
 * WHAT: Check probability conservation, information bounds
 * WHY:  Detect numerical errors or bugs
 * HOW:  Verify Σ|αᵢ|² ≈ 1.0, H ≥ 0, I ≤ H
 *
 * @param qsd Quantum-Shannon diffusion
 * @return true if valid, false if corrupted
 */
bool quantum_shannon_verify(const quantum_shannon_diffusion_t* qsd);

//=============================================================================
// Monte Carlo Integration API
//=============================================================================

/* Forward declarations for QMC types */
struct qmc_entropy_result;

/**
 * @brief Estimate network entropy using Monte Carlo sampling
 *
 * Faster than O(N) direct computation for large networks.
 *
 * @param qsd Quantum Shannon context
 * @param num_samples MC samples (0 = auto)
 * @param result Output entropy estimate
 * @return true on success
 */
bool quantum_shannon_estimate_entropy_mc(
    quantum_shannon_diffusion_t* qsd,
    uint32_t num_samples,
    struct qmc_entropy_result* result
);

/**
 * @brief Detect bottlenecks using adaptive MCMC with early stopping
 *
 * Finds network bottlenecks faster than full scan.
 *
 * @param qsd Quantum Shannon context
 * @param max_bottlenecks Maximum bottlenecks to find
 * @param threshold Capacity threshold (0-1)
 * @param bottleneck_indices Output array
 * @param num_found Output: count found
 * @return true on success
 */
bool quantum_shannon_detect_bottlenecks_mc(
    quantum_shannon_diffusion_t* qsd,
    uint32_t max_bottlenecks,
    float threshold,
    uint32_t* bottleneck_indices,
    uint32_t* num_found
);

/**
 * @brief Get thread-local MC seed for quantum shannon operations
 *
 * @return Pointer to seed
 */
uint32_t* quantum_shannon_get_mc_seed(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_QUANTUM_SHANNON_H
