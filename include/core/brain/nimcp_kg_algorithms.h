/**
 * @file nimcp_kg_algorithms.h
 * @brief Graph Algorithm Utilities for KG Hierarchy
 * @version 1.0.0
 * @date 2025-01-15
 *
 * WHAT: Integrated NIMCP utils, math, and quantum algorithms for KG operations
 * WHY:  Enable advanced graph analysis, quantum-accelerated search, and
 *       intelligent knowledge graph operations
 * HOW:  Unified API wrapping centrality, community detection, quantum walk,
 *       similarity search, phase coherence, and ternary relationships
 *
 * ALGORITHM CATEGORIES:
 * ```
 * +-----------------------------------------------------------------------+
 * |                    KG ALGORITHM UTILITIES                             |
 * +-----------------------------------------------------------------------+
 * |                                                                       |
 * |  Graph Analysis (Classical)           Quantum Algorithms              |
 * |  -------------------------           -------------------              |
 * |  - Centrality metrics                - Quantum walk (sqrt(N) speedup) |
 * |  - Community detection (Louvain)     - Quantum Monte Carlo            |
 * |  - Graph metrics                     - Quantum Shannon search         |
 * |                                                                       |
 * |  Spatial/Embedding                   Logic/Relationships              |
 * |  -----------------                   -------------------              |
 * |  - KD-tree indexing                  - Ternary relationships (+1/-1/0)|
 * |  - Hyperbolic embeddings             - Ternary inference (Kleene K3)  |
 * |  - MPS tensor compression            - Phase coherence analysis       |
 * |                                                                       |
 * +-----------------------------------------------------------------------+
 * ```
 *
 * BIOLOGICAL BASIS:
 * - Centrality metrics model neural hub identification
 * - Community detection mirrors brain region clustering
 * - Quantum walk models neural state superposition
 * - Phase coherence reflects neural synchronization
 *
 * THREAD SAFETY: All functions are thread-safe when used with properly
 *                locked kg_hierarchy_t instances
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_KG_ALGORITHMS_H
#define NIMCP_KG_ALGORITHMS_H

#include "core/brain/nimcp_kg_hierarchy.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/ternary/nimcp_ternary_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Default hub detection threshold */
#define KG_ALGO_DEFAULT_HUB_THRESHOLD   0.8f

/** Default quantum walk steps */
#define KG_ALGO_DEFAULT_QUANTUM_STEPS   100

/** Default QMC samples */
#define KG_ALGO_DEFAULT_QMC_SAMPLES     1000

/** Default coherence threshold */
#define KG_ALGO_DEFAULT_COHERENCE_THRESHOLD  0.7f

/** Default hyperbolic embedding dimensions */
#define KG_ALGO_DEFAULT_HYPERBOLIC_DIMS  8

/** Default MPS bond dimension */
#define KG_ALGO_DEFAULT_MPS_BOND_DIM    32

/** Default KD-tree dimensions */
#define KG_ALGO_DEFAULT_KDTREE_DIMS     64

/* ============================================================================
 * Quantum Walk Coin Types
 * ============================================================================ */

/**
 * @brief Coin operator types for quantum walk
 *
 * WHAT: Coin operators determine quantum walk behavior at each step
 * WHY:  Different coins provide different search characteristics
 * HOW:  Applied as unitary transformation during walk
 */
typedef enum {
    KG_QUANTUM_COIN_HADAMARD = 0,    /**< Hadamard coin (balanced superposition) */
    KG_QUANTUM_COIN_GROVER,          /**< Grover diffusion coin (amplitude amplification) */
    KG_QUANTUM_COIN_FOURIER,         /**< Fourier coin (frequency domain) */
    KG_QUANTUM_COIN_IDENTITY,        /**< Identity coin (classical random walk) */
    KG_QUANTUM_COIN_CUSTOM           /**< User-defined coin operator */
} kg_quantum_coin_t;

/* ============================================================================
 * Algorithm Configuration
 * ============================================================================ */

/**
 * @brief Algorithm configuration for KG operations
 *
 * WHAT: Configuration struct controlling which algorithms are enabled
 * WHY:  Allow selective algorithm usage based on performance/accuracy needs
 * HOW:  Set flags and parameters before calling algorithm functions
 *
 * PERFORMANCE CONSIDERATIONS:
 * - Quantum algorithms provide speedup for large graphs
 * - MPS compression trades accuracy for memory efficiency
 * - Hyperbolic embeddings optimal for hierarchical data
 */
typedef struct {
    /* Graph Analysis (from nimcp_centrality.h, nimcp_louvain.h) */
    bool enable_centrality_metrics;      /**< Hub detection, betweenness */
    bool enable_community_detection;     /**< Louvain clustering */
    bool enable_graph_metrics;           /**< Modularity, clustering coefficient */

    /* Quantum Algorithms (from nimcp_quantum_walk.h, nimcp_quantum_monte_carlo.h) */
    bool enable_quantum_walk;            /**< sqrt(N) speedup for traversal */
    bool enable_quantum_monte_carlo;     /**< Probabilistic search */
    bool enable_quantum_shannon;         /**< Information-theoretic search */
    kg_quantum_coin_t quantum_coin;      /**< HADAMARD, GROVER, FOURIER */
    uint32_t quantum_walk_steps;         /**< Steps per search */
    float decoherence_rate;              /**< Environment noise model [0-1] */

    /* Spatial Indexing (from nimcp_kdtree.h) */
    bool enable_kdtree_indexing;         /**< O(log N) nearest neighbor */
    uint32_t kdtree_dimensions;          /**< Embedding dimensions */

    /* Tensor Compression (from nimcp_mps.h) */
    bool enable_mps_compression;         /**< 10-100x memory reduction */
    uint32_t mps_bond_dimension;         /**< Accuracy vs compression tradeoff */

    /* Ternary Logic (from nimcp_ternary_logic.h) */
    bool enable_ternary_relationships;   /**< +1/-1/0 edge weights */
    bool enable_ternary_inference;       /**< Kleene K3 logic reasoning */

    /* Math Utils (from nimcp_complex_math.h, nimcp_hyperbolic.h) */
    bool enable_phase_coherence;         /**< Entity synchronization */
    bool enable_hyperbolic_embeddings;   /**< Tree-like hierarchy embedding */
} kg_algorithm_config_t;

/* ============================================================================
 * Centrality Metrics
 * ============================================================================ */

/**
 * @brief Centrality metrics for KG nodes
 *
 * WHAT: Multiple centrality measures for node importance analysis
 * WHY:  Identify hub nodes, bottlenecks, and influential modules
 * HOW:  Computed via graph algorithms (degree, betweenness, etc.)
 *
 * BIOLOGICAL BASIS:
 * - Hub nodes correspond to highly connected brain regions
 * - Betweenness indicates information flow bottlenecks
 * - Eigenvector centrality models cascading influence
 */
typedef struct {
    brain_kg_node_id_t node_id;          /**< Node this metrics applies to */
    float degree_centrality;             /**< Importance via connections [0-1] */
    float betweenness_centrality;        /**< Control over paths [0-1] */
    float closeness_centrality;          /**< Average distance to all nodes [0-1] */
    float eigenvector_centrality;        /**< Importance via neighbor importance [0-1] */
    bool is_hub;                         /**< Above hub threshold */
    uint32_t hub_rank;                   /**< Rank among hubs (1 = highest) */
} kg_centrality_metrics_t;

/* ============================================================================
 * Community Detection
 * ============================================================================ */

/**
 * @brief Community/cluster information
 *
 * WHAT: Information about a detected community in the KG
 * WHY:  Identify functional clusters of related modules
 * HOW:  Computed via Louvain or similar community detection
 *
 * BIOLOGICAL BASIS:
 * - Communities correspond to functional brain networks
 * - Modularity reflects specialized processing regions
 */
typedef struct {
    uint32_t community_id;               /**< Unique community identifier */
    char community_name[64];             /**< Optional descriptive name */
    brain_kg_node_id_t* members;         /**< Array of member node IDs */
    uint32_t member_count;               /**< Number of members */
    float modularity_contribution;       /**< Community's Q contribution */
    float internal_density;              /**< Intra-community connection density [0-1] */
    float external_density;              /**< Inter-community connection density [0-1] */
} kg_community_t;

/* ============================================================================
 * Graph Metrics
 * ============================================================================ */

/**
 * @brief Graph quality metrics
 *
 * WHAT: Global graph topology metrics
 * WHY:  Assess overall graph structure and properties
 * HOW:  Computed via graph analysis algorithms
 *
 * BIOLOGICAL BASIS:
 * - Small-world property reflects efficient brain connectivity
 * - Clustering coefficient indicates local processing
 * - Path length affects information propagation speed
 */
typedef struct {
    float modularity_q;                  /**< Modularity score [-0.5 to 1.0] */
    float clustering_coefficient;        /**< Local connection density [0-1] */
    float characteristic_path_length;    /**< Average shortest path length */
    float small_world_coefficient;       /**< sigma = (C/C_rand) / (L/L_rand) */
    uint32_t diameter;                   /**< Longest shortest path */
    float assortativity;                 /**< Degree correlation [-1 to 1] */
    bool is_small_world;                 /**< sigma > 1 */
} kg_graph_metrics_t;

/* ============================================================================
 * Quantum Walk Results
 * ============================================================================ */

/**
 * @brief Quantum walk search result
 *
 * WHAT: Result of quantum walk-based graph search
 * WHY:  Provides quantum-accelerated path finding
 * HOW:  Quantum superposition enables parallel path exploration
 *
 * PERFORMANCE:
 * - Provides O(sqrt(N)) speedup vs classical search
 * - Hitting time indicates expected steps to target
 * - Entropy measures distribution spread
 */
typedef struct {
    brain_kg_node_id_t* path;            /**< Quantum-discovered path (allocated) */
    uint32_t path_length;                /**< Number of nodes in path */
    float* amplitudes;                   /**< Probability amplitude at each node */
    float total_probability;             /**< Sum of |psi|^2 */
    uint32_t steps_taken;                /**< Actual walk steps performed */
    float speedup_factor;                /**< Achieved speedup vs classical */
    float hitting_time;                  /**< Expected time to target */
    float entropy;                       /**< Shannon entropy of distribution */
} kg_quantum_walk_result_t;

/* ============================================================================
 * Quantum Monte Carlo Results
 * ============================================================================ */

/**
 * @brief Quantum Monte Carlo estimation result
 *
 * WHAT: Probabilistic estimation via QMC sampling
 * WHY:  Estimate reachability and path probabilities
 * HOW:  Monte Carlo sampling with quantum-inspired importance
 *
 * ACCURACY:
 * - Variance decreases with more samples
 * - 95% confidence interval provides error bounds
 * - KL divergence measures distribution match
 */
typedef struct {
    float estimated_probability;         /**< P(target | query) [0-1] */
    float variance;                      /**< Estimation uncertainty */
    float confidence_interval_95;        /**< 95% CI width */
    uint32_t samples_used;               /**< Number of QMC samples */
    float kl_divergence;                 /**< KL from prior distribution */
    float fidelity;                      /**< Similarity to target distribution [0-1] */
} kg_qmc_result_t;

/* ============================================================================
 * Similarity Search Results
 * ============================================================================ */

/**
 * @brief Similarity search result
 *
 * WHAT: Result of similarity/nearest-neighbor search
 * WHY:  Find nodes similar to a query node
 * HOW:  Uses KD-tree or MPS-compressed embeddings
 *
 * DISTANCE METRICS:
 * - distance: Raw distance (Euclidean or hyperbolic)
 * - similarity_score: 1 - normalized_distance [0-1]
 */
typedef struct {
    brain_kg_node_id_t node_id;          /**< Similar node ID */
    float distance;                      /**< Euclidean/hyperbolic distance */
    float similarity_score;              /**< 1 - normalized_distance [0-1] */
    float* embedding;                    /**< Node's embedding vector (allocated) */
    uint32_t embedding_dim;              /**< Embedding dimensionality */
} kg_similarity_result_t;

/* ============================================================================
 * Phase Coherence Results
 * ============================================================================ */

/**
 * @brief Phase coherence analysis result
 *
 * WHAT: Phase synchronization between two nodes
 * WHY:  Detect coordinated activity between modules
 * HOW:  Complex phase analysis using oscillation data
 *
 * BIOLOGICAL BASIS:
 * - Phase locking value (PLV) measures synchronization strength
 * - Phase-amplitude coupling (PAC) indicates cross-frequency coordination
 * - Synchronized pairs indicate functional connectivity
 */
typedef struct {
    brain_kg_node_id_t node_a;           /**< First node */
    brain_kg_node_id_t node_b;           /**< Second node */
    float phase_difference;              /**< Phase lag between nodes [0-2pi] */
    float coherence;                     /**< Phase locking value (PLV) [0-1] */
    float amplitude_coupling;            /**< Phase-amplitude coupling (PAC) [0-1] */
    bool synchronized;                   /**< Above coherence threshold */
} kg_coherence_result_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default algorithm configuration
 *
 * WHAT: Initialize configuration with sensible defaults
 * WHY:  Provide starting point for algorithm configuration
 * HOW:  Sets commonly used parameters
 *
 * @param config Output configuration struct
 * @return 0 on success, -1 on error (NULL pointer)
 */
int kg_algorithm_config_default(kg_algorithm_config_t* config);

/* ============================================================================
 * Centrality Analysis API
 * ============================================================================ */

/**
 * @brief Compute centrality metrics for a single node
 *
 * WHAT: Calculate all centrality measures for specified node
 * WHY:  Determine node importance in the graph
 * HOW:  Uses graph algorithms (BFS, shortest paths, eigenvector)
 *
 * @param hier Hierarchy handle
 * @param node_id Node to analyze
 * @param metrics Output centrality metrics
 * @return 0 on success, -1 on error
 */
int kg_compute_centrality(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t node_id,
    kg_centrality_metrics_t* metrics
);

/**
 * @brief Compute centrality metrics for all nodes
 *
 * WHAT: Calculate centrality for entire graph
 * WHY:  Global importance analysis
 * HOW:  Batch computation with shared intermediate results
 *
 * @param hier Hierarchy handle
 * @param metrics Output array (allocated by callee, must be freed)
 * @param count Output: number of metrics returned
 * @return 0 on success, -1 on error
 */
int kg_compute_all_centrality(
    const kg_hierarchy_t* hier,
    kg_centrality_metrics_t** metrics,
    uint32_t* count
);

/**
 * @brief Detect hub nodes based on centrality threshold
 *
 * WHAT: Find highly connected/important nodes
 * WHY:  Identify key modules for monitoring/optimization
 * HOW:  Filter by combined centrality score
 *
 * @param hier Hierarchy handle
 * @param threshold Hub threshold [0-1] (nodes above this are hubs)
 * @param hubs Output array of hub node IDs (allocated by callee)
 * @param hub_count Output: number of hubs found
 * @return 0 on success, -1 on error
 */
int kg_detect_hubs(
    const kg_hierarchy_t* hier,
    float threshold,
    brain_kg_node_id_t** hubs,
    uint32_t* hub_count
);

/**
 * @brief Free centrality metrics array
 *
 * @param metrics Array to free (NULL safe)
 * @param count Number of elements
 */
void kg_centrality_metrics_free(kg_centrality_metrics_t* metrics, uint32_t count);

/* ============================================================================
 * Community Detection API (Louvain)
 * ============================================================================ */

/**
 * @brief Detect communities using Louvain algorithm
 *
 * WHAT: Partition graph into communities
 * WHY:  Identify functional clusters of modules
 * HOW:  Louvain modularity optimization
 *
 * @param hier Hierarchy handle
 * @param communities Output array (allocated by callee, must be freed)
 * @param count Output: number of communities found
 * @return 0 on success, -1 on error
 */
int kg_detect_communities(
    const kg_hierarchy_t* hier,
    kg_community_t** communities,
    uint32_t* count
);

/**
 * @brief Get community ID for a specific node
 *
 * WHAT: Find which community a node belongs to
 * WHY:  Determine node's functional group
 * HOW:  Lookup in community membership map
 *
 * @param hier Hierarchy handle
 * @param node_id Node to query
 * @param community_id Output: community ID
 * @return 0 on success, -1 if node not found
 */
int kg_get_node_community(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t node_id,
    uint32_t* community_id
);

/**
 * @brief Compute modularity score for current community structure
 *
 * WHAT: Calculate graph modularity Q
 * WHY:  Assess quality of community partition
 * HOW:  Newman modularity formula
 *
 * @param hier Hierarchy handle
 * @return Modularity score [-0.5 to 1.0], NaN on error
 */
float kg_compute_modularity(const kg_hierarchy_t* hier);

/**
 * @brief Free community array
 *
 * @param communities Array to free (NULL safe)
 * @param count Number of communities
 */
void kg_community_free(kg_community_t* communities, uint32_t count);

/* ============================================================================
 * Graph Metrics API
 * ============================================================================ */

/**
 * @brief Compute global graph metrics
 *
 * WHAT: Calculate graph topology metrics
 * WHY:  Assess overall graph structure
 * HOW:  Various graph algorithms (clustering, paths, etc.)
 *
 * @param hier Hierarchy handle
 * @param metrics Output metrics struct
 * @return 0 on success, -1 on error
 */
int kg_compute_graph_metrics(
    const kg_hierarchy_t* hier,
    kg_graph_metrics_t* metrics
);

/**
 * @brief Check if graph has small-world property
 *
 * WHAT: Test for small-world network characteristic
 * WHY:  Small-world indicates efficient connectivity
 * HOW:  Compare to random graph (sigma > 1)
 *
 * @param hier Hierarchy handle
 * @return true if small-world, false otherwise
 */
bool kg_is_small_world(const kg_hierarchy_t* hier);

/* ============================================================================
 * Quantum Walk Search API
 * ============================================================================ */

/**
 * @brief Quantum walk-based graph search
 *
 * WHAT: Find path using quantum walk
 * WHY:  Quadratic speedup over classical search
 * HOW:  Discrete-time quantum walk with coin operator
 *
 * PERFORMANCE:
 * - O(sqrt(N)) vs O(N) classical
 * - Coin type affects search behavior
 *
 * @param hier Hierarchy handle
 * @param start Starting node
 * @param target Target node
 * @param config Algorithm configuration
 * @return Result struct (must be freed), NULL on error
 */
kg_quantum_walk_result_t* kg_quantum_walk_search(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t start,
    brain_kg_node_id_t target,
    const kg_algorithm_config_t* config
);

/**
 * @brief Quantum walk spreading activation
 *
 * WHAT: Compute activation spread from source using quantum walk
 * WHY:  Model neural activation propagation
 * HOW:  Quantum walk probability distribution
 *
 * @param hier Hierarchy handle
 * @param source Source node
 * @param activation_map Output activation map (caller allocated, size = node count)
 * @param steps Number of walk steps
 * @return 0 on success, -1 on error
 */
int kg_quantum_walk_spreading_activation(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t source,
    float* activation_map,
    uint32_t steps
);

/**
 * @brief Free quantum walk result
 *
 * @param result Result to free (NULL safe)
 */
void kg_quantum_walk_result_free(kg_quantum_walk_result_t* result);

/* ============================================================================
 * Quantum Monte Carlo API
 * ============================================================================ */

/**
 * @brief Estimate reachability probability via QMC
 *
 * WHAT: Probabilistic path existence estimation
 * WHY:  Fast approximate reachability for large graphs
 * HOW:  Quantum-inspired Monte Carlo sampling
 *
 * @param hier Hierarchy handle
 * @param source Source node
 * @param target Target node
 * @param samples Number of QMC samples
 * @return Result struct (must be freed), NULL on error
 */
kg_qmc_result_t* kg_qmc_estimate_reachability(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t source,
    brain_kg_node_id_t target,
    uint32_t samples
);

/**
 * @brief Estimate graph entropy via QMC
 *
 * WHAT: Information-theoretic graph complexity measure
 * WHY:  Assess graph structure complexity
 * HOW:  Shannon entropy estimation via sampling
 *
 * @param hier Hierarchy handle
 * @param entropy Output: estimated entropy (bits)
 * @return 0 on success, -1 on error
 */
int kg_qmc_estimate_entropy(
    const kg_hierarchy_t* hier,
    float* entropy
);

/**
 * @brief Free QMC result
 *
 * @param result Result to free (NULL safe)
 */
void kg_qmc_result_free(kg_qmc_result_t* result);

/* ============================================================================
 * Similarity Search API (KD-Tree + MPS)
 * ============================================================================ */

/**
 * @brief Find k most similar nodes
 *
 * WHAT: K-nearest neighbor search in embedding space
 * WHY:  Find functionally similar modules
 * HOW:  KD-tree or MPS-compressed search
 *
 * @param hier Hierarchy handle
 * @param query_node Node to find similar nodes for
 * @param k Number of neighbors to find
 * @param out_count Output: actual number found (may be < k)
 * @return Array of results (must be freed), NULL on error
 */
kg_similarity_result_t* kg_find_similar(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t query_node,
    uint32_t k,
    uint32_t* out_count
);

/**
 * @brief Find nodes within distance radius
 *
 * WHAT: Range search in embedding space
 * WHY:  Find all nodes within similarity threshold
 * HOW:  KD-tree range query
 *
 * @param hier Hierarchy handle
 * @param query_node Query node
 * @param radius Maximum distance
 * @param out_count Output: number of results
 * @return Array of results (must be freed), NULL on error
 */
kg_similarity_result_t* kg_find_in_radius(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t query_node,
    float radius,
    uint32_t* out_count
);

/**
 * @brief Rebuild similarity index
 *
 * WHAT: Reconstruct KD-tree/MPS index
 * WHY:  Needed after significant graph changes
 * HOW:  Rebuild spatial index from embeddings
 *
 * @param hier Hierarchy handle (mutable)
 * @return 0 on success, -1 on error
 */
int kg_rebuild_similarity_index(kg_hierarchy_t* hier);

/**
 * @brief Free similarity result array
 *
 * @param results Array to free (NULL safe)
 * @param count Number of results
 */
void kg_similarity_result_free(kg_similarity_result_t* results, uint32_t count);

/* ============================================================================
 * Phase Coherence API
 * ============================================================================ */

/**
 * @brief Compute phase coherence between two nodes
 *
 * WHAT: Measure phase synchronization
 * WHY:  Detect coordinated module activity
 * HOW:  Complex phase analysis
 *
 * @param hier Hierarchy handle
 * @param node_a First node
 * @param node_b Second node
 * @param result Output coherence result
 * @return 0 on success, -1 on error
 */
int kg_compute_coherence(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t node_a,
    brain_kg_node_id_t node_b,
    kg_coherence_result_t* result
);

/**
 * @brief Find all synchronized node pairs
 *
 * WHAT: Detect all pairs above coherence threshold
 * WHY:  Map functional connectivity
 * HOW:  Pairwise coherence computation
 *
 * @param hier Hierarchy handle
 * @param threshold Coherence threshold [0-1]
 * @param pairs Output array (allocated by callee)
 * @param count Output: number of pairs found
 * @return 0 on success, -1 on error
 */
int kg_find_synchronized_pairs(
    const kg_hierarchy_t* hier,
    float threshold,
    kg_coherence_result_t** pairs,
    uint32_t* count
);

/**
 * @brief Free coherence result array
 *
 * @param pairs Array to free (NULL safe)
 * @param count Number of pairs
 */
void kg_coherence_result_free(kg_coherence_result_t* pairs, uint32_t count);

/* ============================================================================
 * Hyperbolic Embedding API
 * ============================================================================ */

/**
 * @brief Compute hyperbolic embeddings for all nodes
 *
 * WHAT: Embed graph in hyperbolic space
 * WHY:  Hyperbolic geometry fits hierarchical structures
 * HOW:  Poincare ball model embedding
 *
 * @param hier Hierarchy handle (mutable, stores embeddings)
 * @param dimensions Embedding dimensionality
 * @return 0 on success, -1 on error
 */
int kg_compute_hyperbolic_embedding(
    kg_hierarchy_t* hier,
    uint32_t dimensions
);

/**
 * @brief Compute hyperbolic distance between two nodes
 *
 * WHAT: Distance in hyperbolic space
 * WHY:  Reflects hierarchical relationship
 * HOW:  Poincare ball geodesic distance
 *
 * @param hier Hierarchy handle
 * @param a First node
 * @param b Second node
 * @return Distance (>= 0), negative on error
 */
float kg_hyperbolic_distance(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t a,
    brain_kg_node_id_t b
);

/* ============================================================================
 * Ternary Relationship API
 * ============================================================================ */

/**
 * @brief Set ternary relationship weight between nodes
 *
 * WHAT: Assign +1/-1/0 edge weight
 * WHY:  Model excitatory/inhibitory/unknown relationships
 * HOW:  Store trit value as edge weight
 *
 * @param hier Hierarchy handle (mutable)
 * @param from Source node
 * @param to Target node
 * @param relationship Ternary value (+1=excitatory, -1=inhibitory, 0=unknown)
 * @return 0 on success, -1 on error
 */
int kg_set_relationship_ternary(
    kg_hierarchy_t* hier,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to,
    trit_t relationship
);

/**
 * @brief Get ternary relationship weight between nodes
 *
 * WHAT: Retrieve +1/-1/0 edge weight
 * WHY:  Query relationship type
 * HOW:  Lookup edge weight
 *
 * @param hier Hierarchy handle
 * @param from Source node
 * @param to Target node
 * @return Ternary value (+1/-1/0), 0 if not found
 */
trit_t kg_get_relationship_ternary(
    const kg_hierarchy_t* hier,
    brain_kg_node_id_t from,
    brain_kg_node_id_t to
);

/**
 * @brief Perform ternary inference (Kleene K3 logic)
 *
 * WHAT: Logical inference using ternary relationships
 * WHY:  Reason about uncertain/unknown relationships
 * HOW:  Kleene strong three-valued logic
 *
 * @param hier Hierarchy handle
 * @param query Inference query string
 * @param result Output: inference result (+1=true, -1=false, 0=unknown)
 * @return 0 on success, -1 on error
 */
int kg_ternary_inference(
    const kg_hierarchy_t* hier,
    const char* query,
    trit_t* result
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert quantum coin type to string
 *
 * @param coin Coin type
 * @return String representation
 */
const char* kg_quantum_coin_to_string(kg_quantum_coin_t coin);

/**
 * @brief Free hub array allocated by kg_detect_hubs
 *
 * @param hubs Array to free (NULL safe)
 */
void kg_hubs_free(brain_kg_node_id_t* hubs);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_KG_ALGORITHMS_H */
