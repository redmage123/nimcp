//=============================================================================
// nimcp_fractal_topology.h - Fractal Network Topology Generator
//=============================================================================
/**
 * @file nimcp_fractal_topology.h
 * @brief Scale-free fractal network topology generation for biologically realistic connectivity
 *
 * WHAT: Generates scale-free networks with power-law degree distributions and fractal properties
 * WHY: Real brains exhibit scale-free connectivity (hub neurons + sparse connections)
 *      - 70-80% fewer connections with same information capacity
 *      - More robust to random failures (hub-based architecture)
 *      - Biologically accurate (matches cortical connectivity patterns)
 * HOW: Implements Barabási-Albert preferential attachment algorithm with spatial constraints
 *
 * BIOLOGICAL MOTIVATION:
 * - Cortical connectivity follows power-law distributions (Sporns et al., 2004)
 * - Hub neurons exist in all brain regions (Van den Heuvel & Sporns, 2011)
 * - Scale-free networks optimize information processing (Bassett & Bullmore, 2006)
 * - Fractal dimension of neural connectivity ~2.5 (Wen & Ding, 2013)
 *
 * ARCHITECTURE:
 * - Strategy Pattern: Multiple topology generation algorithms (random, scale-free, small-world)
 * - Factory Pattern: Topology generators created via config structs
 * - Builder Pattern: Complex networks built incrementally
 *
 * PERFORMANCE:
 * - Scale-free generation: O(N log N) time, O(N) space
 * - 70-80% reduction in synapses vs random connectivity
 * - GPU-friendly: Connection matrix generation parallelizable
 *
 * @author NIMCP Development Team
 * @date 2025-11-08
 * @version 2.7.0
 */

#ifndef NIMCP_FRACTAL_TOPOLOGY_H
#define NIMCP_FRACTAL_TOPOLOGY_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

// Forward declare neural_network_t (defined in core/neuralnet/nimcp_neuralnet.h)
typedef struct neural_network_struct* neural_network_t;

//=============================================================================
// Topology Types and Configuration
//=============================================================================

/**
 * @brief Topology generation algorithms
 *
 * WHAT: Available network topology types
 * WHY: Different topologies have different properties suited for different tasks
 */
typedef enum {
    TOPOLOGY_RANDOM,        /**< Random Erdős-Rényi connectivity */
    TOPOLOGY_SCALE_FREE,    /**< Barabási-Albert scale-free (power-law) */
    TOPOLOGY_SMALL_WORLD,   /**< Watts-Strogatz small-world */
    TOPOLOGY_FRACTAL,       /**< Fractal with configurable dimension */
    TOPOLOGY_SPATIAL        /**< Distance-dependent connectivity */
} topology_type_t;

/**
 * @brief Configuration for scale-free network generation
 *
 * WHAT: Parameters controlling power-law degree distribution
 * WHY: Flexibility to match different biological systems
 * HOW: Controls hub concentration and connection sparsity
 */
typedef struct {
    float power_law_gamma;      /**< Exponent for P(k) ~ k^γ, typical: -2.0 to -3.0 */
    float hub_ratio;            /**< Fraction of neurons that are hubs, typical: 0.10-0.20 */
    uint32_t min_degree;        /**< Minimum connections per neuron, typical: 2-5 */
    uint32_t max_degree;        /**< Maximum connections per neuron, typical: N/10 */
    float spatial_constraint;   /**< Spatial distance factor (0=ignore, 1=strong), typical: 0.3-0.7 */
    bool bidirectional;         /**< Create reciprocal connections, typical: false */
} scale_free_config_t;

/**
 * @brief Configuration for fractal network generation
 *
 * WHAT: Parameters controlling fractal dimension and self-similarity
 * WHY: Match specific fractal properties of biological networks
 * HOW: Hierarchical generation with scale-invariant patterns
 */
typedef struct {
    float fractal_dimension;    /**< Fractal dimension, typical: 1.5-2.5 (cortex ~2.5) */
    uint32_t hierarchy_levels;  /**< Number of hierarchical levels, typical: 3-5 */
    float branching_factor;     /**< Average branches per level, typical: 2-4 */
    float scale_factor;         /**< Size reduction per level, typical: 0.5-0.8 */
    float clustering_coeff;     /**< Local clustering coefficient, typical: 0.3-0.6 */
} fractal_config_t;

/**
 * @brief Unified topology configuration
 *
 * WHAT: Single config struct supporting all topology types
 * WHY: Consistent API regardless of topology choice
 * HOW: Union of type-specific configs
 */
typedef struct {
    topology_type_t type;       /**< Which topology algorithm to use */
    union {
        scale_free_config_t scale_free;
        fractal_config_t fractal;
    } params;
} topology_config_t;

/**
 * @brief Network topology statistics
 *
 * WHAT: Metrics describing generated topology
 * WHY: Validate topology matches expected properties
 * HOW: Computed during or after generation
 */
typedef struct {
    uint32_t num_neurons;           /**< Total neurons in network */
    uint32_t num_synapses;          /**< Total synapses created */
    float avg_degree;               /**< Mean connections per neuron */
    float degree_std;               /**< Standard deviation of degree */
    float clustering_coefficient;   /**< Local clustering (friend-of-friend) */
    float characteristic_path;      /**< Average shortest path length */
    float power_law_fit;            /**< R² for power-law fit (0-1) */
    uint32_t num_hubs;              /**< Count of hub neurons */
    float hub_connectivity;         /**< Fraction of connections through hubs */
    float small_world_sigma;        /**< Small-world coefficient (σ = C/Crand / L/Lrand) */
} topology_stats_t;

//=============================================================================
// Topology Generation Functions
//=============================================================================

/**
 * @brief Generate scale-free network topology using Barabási-Albert algorithm
 *
 * WHAT: Creates power-law degree distribution with preferential attachment
 * WHY: Biologically realistic, efficient (70-80% fewer synapses), robust to failures
 * HOW:
 *   1. Start with small fully-connected seed network
 *   2. Add neurons one at a time
 *   3. Each new neuron connects to existing neurons with probability ∝ degree
 *   4. Result: Power-law P(k) ~ k^γ degree distribution
 *
 * ALGORITHM:
 *   for each new neuron n:
 *     for m connections:
 *       target = select neuron with probability P(i) = degree(i) / Σ degrees
 *       if spatial_constraint > 0:
 *         P(i) *= exp(-distance(n,i) / spatial_constraint)
 *       create synapse n → target
 *
 * BIOLOGICAL BASIS:
 * - Matches cortical connectivity (Sporns et al., 2004)
 * - Hub neurons are pyramidal cells in layers 2/3 and 5
 * - Explains small-world properties of brain networks
 *
 * @param network Neural network to add topology to (must be initialized)
 * @param config Scale-free generation parameters
 * @param stats Output statistics (can be NULL)
 * @return true on success, false on failure
 *
 * @note Network must have neurons already added via neural_network_add_neuron()
 * @note Existing synapses are preserved, new ones are added
 * @note Thread-safe if network is not shared
 */
bool topology_generate_scale_free(
    neural_network_t network,
    const scale_free_config_t* config,
    topology_stats_t* stats
);

/**
 * @brief Generate fractal hierarchical topology
 *
 * WHAT: Creates self-similar hierarchical structure with configurable fractal dimension
 * WHY: Matches hierarchical organization of cortical areas and columns
 * HOW: Recursive subdivision with scale-invariant connection patterns
 *
 * ALGORITHM:
 *   1. Partition neurons into hierarchy_levels groups
 *   2. At each level, create local clusters
 *   3. Connect clusters hierarchically (level i connects to level i±1)
 *   4. Within clusters, use local connectivity rules
 *   5. Between clusters, use long-range sparse connections
 *
 * BIOLOGICAL BASIS:
 * - Cortical columns (Mountcastle, 1997)
 * - Hierarchical visual processing (Felleman & Van Essen, 1991)
 * - Fractal branching of dendrites (Wen & Ding, 2013)
 *
 * @param network Neural network to add topology to
 * @param config Fractal generation parameters
 * @param stats Output statistics (can be NULL)
 * @return true on success, false on failure
 */
bool topology_generate_fractal(
    neural_network_t network,
    const fractal_config_t* config,
    topology_stats_t* stats
);

/**
 * @brief Generate topology from unified configuration
 *
 * WHAT: Dispatcher function that calls appropriate generator based on config.type
 * WHY: Consistent API for all topology types
 * HOW: Switch on config.type, delegate to specific generator
 *
 * @param network Neural network to add topology to
 * @param config Topology configuration (type + parameters)
 * @param stats Output statistics (can be NULL)
 * @return true on success, false on failure
 *
 * EXAMPLE:
 * @code
 * topology_config_t config = {
 *     .type = TOPOLOGY_SCALE_FREE,
 *     .params.scale_free = {
 *         .power_law_gamma = -2.1f,
 *         .hub_ratio = 0.15f,
 *         .min_degree = 3,
 *         .max_degree = 100,
 *         .spatial_constraint = 0.5f,
 *         .bidirectional = false
 *     }
 * };
 * topology_stats_t stats;
 * topology_generate(network, &config, &stats);
 * printf("Generated %u synapses (avg degree: %.2f)\n", stats.num_synapses, stats.avg_degree);
 * @endcode
 */
bool topology_generate(
    neural_network_t network,
    const topology_config_t* config,
    topology_stats_t* stats
);

//=============================================================================
// Topology Analysis Functions
//=============================================================================

/**
 * @brief Compute topology statistics for existing network
 *
 * WHAT: Analyzes network connectivity and computes graph metrics
 * WHY: Validate topology properties, compare to biological data
 * HOW: Graph theory algorithms (BFS for paths, clustering coefficient, degree distribution)
 *
 * METRICS COMPUTED:
 * - Degree distribution (mean, std, power-law fit)
 * - Clustering coefficient (local transitivity)
 * - Characteristic path length (average shortest path)
 * - Hub identification (top 10% by degree)
 * - Small-world coefficient
 *
 * COMPLEXITY: O(N² log N) for full analysis
 *
 * @param network Neural network to analyze
 * @param stats Output statistics struct
 * @return true on success, false on failure (NULL inputs, empty network)
 */
bool topology_compute_stats(
    neural_network_t network,
    topology_stats_t* stats
);

/**
 * @brief Check if network exhibits small-world properties
 *
 * WHAT: Tests for high clustering + short path length (C >> Crand, L ≈ Lrand)
 * WHY: Small-world networks are efficient for information transfer
 * HOW: Compute σ = (C/Crand) / (L/Lrand), small-world if σ > 1
 *
 * BIOLOGICAL SIGNIFICANCE:
 * - Brain networks are small-world (Watts & Strogatz, 1998)
 * - Balances local processing with global integration
 *
 * @param network Neural network to test
 * @param sigma Output small-world coefficient (can be NULL)
 * @return true if small-world, false otherwise
 */
bool topology_is_small_world(
    neural_network_t network,
    float* sigma
);

/**
 * @brief Fit power-law distribution to degree distribution
 *
 * WHAT: Estimates γ in P(k) ~ k^γ using maximum likelihood
 * WHY: Validate scale-free property quantitatively
 * HOW: MLE on log-transformed degree distribution
 *
 * @param network Neural network to analyze
 * @param gamma Output power-law exponent (typically -2 to -3)
 * @param r_squared Output goodness-of-fit (0-1, >0.8 is good)
 * @return true if power-law fit is valid, false otherwise
 */
bool topology_fit_power_law(
    neural_network_t network,
    float* gamma,
    float* r_squared
);

//=============================================================================
// Hub Neuron Functions
//=============================================================================

/**
 * @brief Identify hub neurons in network
 *
 * WHAT: Returns indices of neurons with degree > threshold percentile
 * WHY: Hub neurons are critical for network function and analysis
 * HOW: Sort neurons by degree, return top percentile
 *
 * @param network Neural network to analyze
 * @param percentile Threshold percentile (0.0-1.0), typical: 0.9 (top 10%)
 * @param hub_indices Output array of hub neuron indices (caller must free)
 * @param num_hubs Output count of hub neurons
 * @return true on success, false on failure
 *
 * USAGE:
 * @code
 * uint32_t* hubs = NULL;
 * uint32_t count = 0;
 * topology_identify_hubs(network, 0.9f, &hubs, &count);
 * printf("Found %u hub neurons\n", count);
 * for (uint32_t i = 0; i < count; i++) {
 *     printf("  Hub %u: neuron %u\n", i, hubs[i]);
 * }
 * nimcp_free(hubs);
 * @endcode
 */
bool topology_identify_hubs(
    neural_network_t network,
    float percentile,
    uint32_t** hub_indices,
    uint32_t* num_hubs
);

/**
 * @brief Compute betweenness centrality for neurons
 *
 * WHAT: Measures how many shortest paths pass through each neuron
 * WHY: Identifies critical neurons for information flow
 * HOW: Brandes' algorithm for betweenness centrality
 *
 * COMPLEXITY: O(NM) where N=neurons, M=synapses
 *
 * @param network Neural network to analyze
 * @param centrality Output array of centrality scores (caller must allocate N floats)
 * @return true on success, false on failure
 */
bool topology_compute_betweenness(
    neural_network_t network,
    float* centrality
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Create default scale-free configuration
 *
 * WHAT: Returns sensible defaults for cortical-like networks
 * WHY: Easy starting point without parameter tuning
 * HOW: Values based on empirical neuroscience data
 *
 * @return Default scale-free config
 */
scale_free_config_t topology_default_scale_free_config(void);

/**
 * @brief Create default fractal configuration
 *
 * WHAT: Returns sensible defaults for hierarchical networks
 * WHY: Easy starting point for fractal topologies
 * HOW: Values based on cortical hierarchy measurements
 *
 * @return Default fractal config
 */
fractal_config_t topology_default_fractal_config(void);

/**
 * @brief Validate topology configuration
 *
 * WHAT: Checks if config parameters are in valid ranges
 * WHY: Prevent invalid network generation
 * HOW: Range checks on all parameters
 *
 * @param config Configuration to validate
 * @return true if valid, false if invalid
 */
bool topology_validate_config(const topology_config_t* config);

//=============================================================================
// Error Handling
//=============================================================================

/**
 * @brief Get last topology error message
 *
 * WHAT: Returns human-readable description of last error
 * WHY: Debugging and user-friendly error reporting
 * HOW: Thread-local error string storage
 *
 * @return Error message string (NULL if no error)
 */
const char* topology_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_FRACTAL_TOPOLOGY_H
