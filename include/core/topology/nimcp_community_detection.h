//=============================================================================
// nimcp_community_detection.h - Community Detection for Neural Networks
//=============================================================================
/**
 * @file nimcp_community_detection.h
 * @brief Louvain algorithm for detecting functional modules in neural networks
 *
 * WHAT: Community detection finds groups of densely connected neurons (modules)
 * WHY: Brain networks are modular - functionally related neurons cluster together
 *      - Modularity enables specialization (visual cortex, motor cortex, etc.)
 *      - Tracking modularity during training reveals network organization
 *      - High modularity (Q > 0.3) indicates good functional specialization
 * HOW: Louvain algorithm optimizes Newman's modularity Q score via greedy local moves
 *
 * BIOLOGICAL MOTIVATION:
 * - Brain networks exhibit modular organization (Sporns & Betzel, 2016)
 * - Functional modules emerge during development (Fair et al., 2009)
 * - Module structure correlates with cognitive function (Bullmore & Sporns, 2012)
 * - Typical brain modularity Q ≈ 0.3-0.5 (Power et al., 2011)
 *
 * ALGORITHM (Louvain Method):
 * Phase 1: Local optimization
 *   - For each neuron, compute modularity gain from moving to neighbor communities
 *   - Move neuron to community with max positive gain
 *   - Repeat until no improvements
 * Phase 2: Network aggregation
 *   - Build new network where each community becomes a super-node
 *   - Repeat Phase 1 on aggregated network
 * Phase 3: Convergence
 *   - Stop when modularity no longer increases
 *
 * PERFORMANCE:
 * - Time: O(N log N) average case for sparse networks
 * - Space: O(N + M) where N=neurons, M=synapses
 * - Suitable for real-time training analysis
 *
 * MODULARITY METRIC:
 * Newman's Q = (1/2m) * Σ [A_ij - (k_i * k_j)/2m] * δ(c_i, c_j)
 * Where:
 *   - A_ij = adjacency matrix (1 if connected, 0 otherwise)
 *   - k_i = degree of neuron i
 *   - m = total number of connections
 *   - c_i = community of neuron i
 *   - δ(c_i, c_j) = 1 if i and j in same community, 0 otherwise
 *
 * INTERPRETATION:
 * - Q ∈ [-0.5, 1.0]
 * - Q > 0.3: Strong community structure
 * - Q ∈ [0.2, 0.3]: Moderate community structure
 * - Q < 0.2: Weak or no community structure
 * - Q < 0: Worse than random (unusual)
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 * @version 1.0.0
 */

#ifndef NIMCP_COMMUNITY_DETECTION_H
#define NIMCP_COMMUNITY_DETECTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

// Forward declare neural_network_t (defined in core/neuralnet/nimcp_neuralnet.h)
typedef struct neural_network_struct* neural_network_t;
#define NIMCP_NEURAL_NETWORK_T_DEFINED

//=============================================================================
// Community Detection Types
//=============================================================================

/**
 * @brief Community structure of a neural network
 *
 * WHAT: Stores community assignments and modularity metrics
 * WHY: Encapsulates all community detection results
 */
typedef struct {
    uint32_t num_neurons;           /**< Total neurons in network */
    uint32_t num_communities;       /**< Number of detected communities */
    uint32_t* community_ids;        /**< Community ID for each neuron (size: num_neurons) */
    uint32_t* community_sizes;      /**< Number of neurons in each community (size: num_communities) */
    float modularity;               /**< Newman's modularity Q score */
    float* internal_density;        /**< Intra-community connection density (size: num_communities) */
    float* external_density;        /**< Inter-community connection density (size: num_communities) */
} community_structure_t;

/**
 * @brief Configuration for community detection algorithm
 *
 * WHAT: Parameters controlling Louvain algorithm behavior
 * WHY: Allow tuning for different network sizes and characteristics
 */
typedef struct {
    uint32_t max_iterations;        /**< Max iterations per phase, typical: 100 */
    float min_modularity_gain;      /**< Stop if gain < threshold, typical: 1e-5 */
    uint32_t max_communities;       /**< Limit number of communities (0 = no limit) */
    float resolution;               /**< Resolution parameter (>1 finds more communities), typical: 1.0 */
    bool weighted;                  /**< Consider synapse weights in modularity, typical: true */
    uint32_t random_seed;           /**< Seed for randomization (0 = time-based) */
    /* Scale-down knobs — appended to preserve ABI. Set to 0 to disable. */
    uint32_t max_nodes_sample;      /**< Cap graph nodes at this (0 = use all). Top-N by degree. */
    float weight_threshold;         /**< Skip edges with |weight| below this (0 = keep all). */
} community_detection_config_t;

/**
 * @brief Hub neuron identification results
 *
 * WHAT: Stores identified hub neurons and their centrality metrics
 * WHY: Hubs are critical for network function and information flow
 */
typedef struct {
    uint32_t num_hubs;              /**< Number of hub neurons identified */
    uint32_t* hub_indices;          /**< Indices of hub neurons (size: num_hubs) */
    float* degree_centrality;       /**< Degree centrality scores (size: num_hubs) */
    float* betweenness_centrality;  /**< Betweenness centrality scores (size: num_hubs) */
    uint32_t* hub_communities;      /**< Community ID for each hub (size: num_hubs) */
} hub_structure_t;

/**
 * @brief Topology validation results
 *
 * WHAT: Comprehensive topology quality metrics
 * WHY: Ensure network has desirable properties during training
 */
typedef struct {
    bool is_valid;                  /**< Overall validation passed */
    float modularity;               /**< Newman's modularity Q */
    float clustering_coefficient;   /**< Local clustering coefficient */
    float characteristic_path;      /**< Average shortest path length */
    float small_world_sigma;        /**< Small-world coefficient */
    uint32_t num_communities;       /**< Number of communities */
    uint32_t num_hubs;              /**< Number of hub neurons */
    char error_message[256];        /**< Human-readable error if invalid */
} topology_validation_t;

//=============================================================================
// Community Detection Functions
//=============================================================================

/**
 * @brief Detect communities using Louvain algorithm
 *
 * WHAT: Finds functional modules by optimizing modularity
 * WHY: Reveals network organization and specialization
 * HOW: Iterative local moves + network aggregation
 *
 * @param network Neural network to analyze
 * @param config Algorithm configuration (NULL = defaults)
 * @return Community structure (caller must free with topology_community_structure_free)
 *
 * USAGE:
 * @code
 * community_structure_t* communities = community_detect(network, NULL);
 * printf("Modularity Q: %.3f\n", communities->modularity);
 * printf("Communities: %u\n", communities->num_communities);
 * for (uint32_t i = 0; i < communities->num_neurons; i++) {
 *     printf("Neuron %u -> Community %u\n", i, communities->community_ids[i]);
 * }
 * topology_community_structure_free(communities);
 * @endcode
 */
community_structure_t* community_detect(
    neural_network_t network,
    const community_detection_config_t* config
);

/**
 * @brief Free community structure memory
 *
 * WHAT: Deallocates all memory in community_structure_t
 * WHY: Prevent memory leaks
 *
 * @param structure Community structure to free
 */
void topology_community_structure_free(community_structure_t* structure);

/**
 * @brief Compute Newman's modularity Q for given community assignments
 *
 * WHAT: Calculates modularity score for specific partition
 * WHY: Evaluate quality of community structure
 * HOW: Implements Newman's Q formula
 *
 * @param network Neural network
 * @param community_ids Community assignment for each neuron (size: num_neurons)
 * @param num_communities Number of distinct communities
 * @return Modularity Q score (typically 0.0 to 1.0)
 */
float community_compute_modularity(
    neural_network_t network,
    const uint32_t* community_ids,
    uint32_t num_communities
);

/**
 * @brief Get community ID for a specific neuron
 *
 * WHAT: Returns which community a neuron belongs to
 * WHY: Query individual neuron community membership
 *
 * @param structure Community structure from detection
 * @param neuron_id Neuron index to query
 * @return Community ID (0 to num_communities-1), or UINT32_MAX if invalid
 */
uint32_t community_get_neuron_community(
    const community_structure_t* structure,
    uint32_t neuron_id
);

/**
 * @brief Get all neurons in a specific community
 *
 * WHAT: Returns list of neuron indices in given community
 * WHY: Analyze composition of functional modules
 *
 * @param structure Community structure from detection
 * @param community_id Community to query
 * @param neuron_ids Output array of neuron indices (caller must free)
 * @param count Output count of neurons in community
 * @return true on success, false on failure
 */
bool community_get_neurons_in_community(
    const community_structure_t* structure,
    uint32_t community_id,
    uint32_t** neuron_ids,
    uint32_t* count
);

//=============================================================================
// Hub Detection Functions
//=============================================================================

/**
 * @brief Detect hub neurons using centrality metrics
 *
 * WHAT: Identifies highly connected and central neurons
 * WHY: Hubs are critical for network function and information flow
 * HOW: Computes degree and betweenness centrality, selects top neurons
 *
 * @param network Neural network to analyze
 * @param threshold Centrality threshold (0.0-1.0), typical: 0.8 (top 20%)
 * @return Hub structure (caller must free with hub_structure_free)
 *
 * USAGE:
 * @code
 * hub_structure_t* hubs = community_detect_hubs(network, 0.8f);
 * printf("Found %u hub neurons\n", hubs->num_hubs);
 * for (uint32_t i = 0; i < hubs->num_hubs; i++) {
 *     printf("Hub %u (neuron %u): degree=%.3f, betweenness=%.3f\n",
 *            i, hubs->hub_indices[i],
 *            hubs->degree_centrality[i],
 *            hubs->betweenness_centrality[i]);
 * }
 * hub_structure_free(hubs);
 * @endcode
 */
hub_structure_t* community_detect_hubs(
    neural_network_t network,
    float threshold
);

/**
 * @brief Free hub structure memory
 *
 * WHAT: Deallocates all memory in hub_structure_t
 * WHY: Prevent memory leaks
 *
 * @param structure Hub structure to free
 */
void hub_structure_free(hub_structure_t* structure);

//=============================================================================
// Topology Validation Functions
//=============================================================================

/**
 * @brief Validate network topology quality
 *
 * WHAT: Checks if network has desirable topological properties
 * WHY: Ensure healthy network structure during training
 * HOW: Computes multiple metrics and validates against thresholds
 *
 * VALIDATION CRITERIA:
 * - Modularity Q > 0.2 (moderate community structure)
 * - Clustering coefficient > 0.1 (local connectivity)
 * - Small-world sigma > 1.0 (efficient information flow)
 * - At least 2 communities
 * - At least 1 hub neuron per 100 neurons
 *
 * @param network Neural network to validate
 * @param min_modularity Minimum acceptable modularity (typical: 0.2)
 * @return Validation results with detailed metrics
 *
 * USAGE:
 * @code
 * topology_validation_t validation = community_validate_topology(network, 0.25f);
 * if (validation.is_valid) {
 *     printf("Topology is healthy: Q=%.3f, Communities=%u\n",
 *            validation.modularity, validation.num_communities);
 * } else {
 *     printf("Topology validation failed: %s\n", validation.error_message);
 * }
 * @endcode
 */
topology_validation_t community_validate_topology(
    neural_network_t network,
    float min_modularity
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Create default community detection configuration
 *
 * WHAT: Returns sensible defaults for Louvain algorithm
 * WHY: Easy starting point without parameter tuning
 *
 * @return Default configuration
 */
community_detection_config_t community_default_config(void);

/**
 * @brief Print community statistics to stdout
 *
 * WHAT: Displays human-readable summary of community structure
 * WHY: Quick visualization for debugging and analysis
 *
 * @param structure Community structure to print
 */
void community_print_stats(const community_structure_t* structure);

/**
 * @brief Print hub statistics to stdout
 *
 * WHAT: Displays human-readable summary of hub neurons
 * WHY: Quick visualization for debugging and analysis
 *
 * @param hubs Hub structure to print
 */
void community_print_hubs(const hub_structure_t* hubs);

//=============================================================================
// Error Handling
//=============================================================================

/**
 * @brief Get last community detection error message
 *
 * WHAT: Returns human-readable description of last error
 * WHY: Debugging and user-friendly error reporting
 * HOW: Thread-local error string storage
 *
 * @return Error message string (NULL if no error)
 */
const char* community_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_COMMUNITY_DETECTION_H
