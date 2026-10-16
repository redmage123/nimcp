/**
 * @file nimcp_topology_gpu.h
 * @brief GPU-accelerated Topology and Community Detection Operations
 *
 * WHAT: CUDA kernels for graph topology analysis and community detection
 * WHY:  GPU acceleration for large-scale neural network topology operations
 * HOW:  Custom kernels for Louvain community detection, graph metrics,
 *       shortest paths, and network generation algorithms
 *
 * ARCHITECTURE:
 * - CSR/Dense graph representation for efficient GPU access
 * - Parallel community detection (Louvain algorithm, Label Propagation)
 * - Parallel graph metrics (degree, clustering, PageRank, betweenness)
 * - Parallel shortest path algorithms (BFS, Dijkstra, Floyd-Warshall)
 * - Parallel network generation (Erdos-Renyi, Barabasi-Albert, Watts-Strogatz)
 *
 * PARALLELIZATION STRATEGIES:
 * - Community detection: Parallelize across nodes for local moving
 * - Graph metrics: Parallelize across nodes/edges for computation
 * - Shortest paths: Frontier-based parallelization for BFS/Dijkstra
 * - Network generation: Parallelize across edges for random graph creation
 *
 * BIOLOGICAL MOTIVATION:
 * - Brain networks exhibit modular organization (Sporns & Betzel, 2016)
 * - Functional modules emerge during development (Fair et al., 2009)
 * - Hub neurons are critical for network function (van den Heuvel, 2012)
 * - Scale-free and small-world properties optimize information flow
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_TOPOLOGY_GPU_H
#define NIMCP_TOPOLOGY_GPU_H

#include "gpu/context/nimcp_gpu_context.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// Graph Representation
//=============================================================================

/**
 * @brief GPU graph representation (Dense or CSR format)
 *
 * WHAT: Graph data structure optimized for GPU operations
 * WHY:  Efficient parallel graph traversal and community detection
 * HOW:  Supports both dense adjacency matrix and sparse CSR format
 *
 * MEMORY LAYOUT:
 * - Dense: adjacency[i * num_nodes + j] = edge weight from i to j
 * - CSR: row_ptrs[i] to row_ptrs[i+1] gives edge indices for node i
 */
typedef struct nimcp_graph_gpu {
    nimcp_gpu_tensor_t* adjacency;      /**< Dense adjacency matrix [num_nodes x num_nodes] */
    nimcp_gpu_tensor_t* edge_weights;   /**< Edge weights for CSR [num_edges] or dense */
    nimcp_gpu_tensor_t* node_features;  /**< Node features [num_nodes x feature_dim] (optional) */
    int num_nodes;                       /**< Number of nodes in graph */
    int num_edges;                       /**< Number of edges in graph */
    bool is_sparse;                      /**< Whether graph uses CSR format */

    // CSR format pointers (if sparse)
    nimcp_gpu_tensor_t* row_ptrs;       /**< CSR row pointers [num_nodes + 1] */
    nimcp_gpu_tensor_t* col_indices;    /**< CSR column indices [num_edges] */

    nimcp_gpu_context_t* ctx;           /**< GPU context */
} nimcp_graph_gpu_t;

/**
 * @brief Community detection result
 *
 * WHAT: Result structure for community detection algorithms
 * WHY:  Encapsulates community assignments and quality metrics
 */
typedef struct nimcp_community_result_gpu {
    nimcp_gpu_tensor_t* node_communities;   /**< Community assignment [num_nodes] (int32) */
    nimcp_gpu_tensor_t* community_sizes;    /**< Size of each community [num_communities] (int32) */
    int num_communities;                     /**< Number of detected communities */
    float modularity;                        /**< Newman's modularity Q score */
} nimcp_community_result_gpu_t;

/**
 * @brief Topology metrics result
 *
 * WHAT: Comprehensive topology metrics for graph analysis
 * WHY:  Characterize network properties for validation and analysis
 */
typedef struct nimcp_topology_metrics_gpu {
    nimcp_gpu_tensor_t* degree;             /**< Node degrees [num_nodes] (int32) */
    nimcp_gpu_tensor_t* weighted_degree;    /**< Weighted node degrees [num_nodes] (float32) */
    nimcp_gpu_tensor_t* clustering_coeff;   /**< Local clustering coefficients [num_nodes] (float32) */
    nimcp_gpu_tensor_t* betweenness;        /**< Betweenness centrality [num_nodes] (float32) */
    nimcp_gpu_tensor_t* pagerank;           /**< PageRank scores [num_nodes] (float32) */
    float avg_path_length;                   /**< Average shortest path length */
    float global_clustering;                 /**< Global clustering coefficient */
    float diameter;                          /**< Graph diameter (longest shortest path) */
    float density;                           /**< Edge density */
} nimcp_topology_metrics_gpu_t;

/**
 * @brief Shortest path result
 *
 * WHAT: Result structure for shortest path computations
 * WHY:  Store distances and predecessors for path reconstruction
 */
typedef struct nimcp_shortest_path_result_gpu {
    nimcp_gpu_tensor_t* distances;          /**< Distances from source [num_nodes] (float32) */
    nimcp_gpu_tensor_t* predecessors;       /**< Predecessor for path [num_nodes] (int32) */
    float max_distance;                      /**< Maximum distance found */
    int num_reachable;                       /**< Number of reachable nodes */
} nimcp_shortest_path_result_gpu_t;

/**
 * @brief All-pairs shortest path result
 *
 * WHAT: Distance matrix between all pairs of nodes
 * WHY:  Required for diameter, average path length, betweenness
 */
typedef struct nimcp_apsp_result_gpu {
    nimcp_gpu_tensor_t* distances;          /**< Distance matrix [num_nodes x num_nodes] (float32) */
    float diameter;                          /**< Graph diameter */
    float avg_path_length;                   /**< Average shortest path length */
} nimcp_apsp_result_gpu_t;

//=============================================================================
// Graph Creation and Destruction
//=============================================================================

/**
 * @brief Create empty GPU graph
 *
 * @param ctx GPU context
 * @param num_nodes Number of nodes
 * @param sparse Whether to use sparse CSR format
 * @return GPU graph or NULL on failure
 */
NIMCP_EXPORT nimcp_graph_gpu_t* nimcp_graph_gpu_create(
    nimcp_gpu_context_t* ctx,
    int num_nodes,
    bool sparse
);

/**
 * @brief Create GPU graph from edge list
 *
 * WHAT: Build graph from source-destination edge pairs
 * WHY:  Common format for graph construction
 * HOW:  Parallel edge insertion and CSR construction
 *
 * @param ctx GPU context
 * @param src Source node indices [num_edges]
 * @param dst Destination node indices [num_edges]
 * @param weights Edge weights [num_edges] (NULL for unweighted)
 * @param num_edges Number of edges
 * @param num_nodes Number of nodes
 * @return GPU graph or NULL on failure
 */
NIMCP_EXPORT nimcp_graph_gpu_t* nimcp_graph_gpu_from_edges(
    nimcp_gpu_context_t* ctx,
    const int* src,
    const int* dst,
    const float* weights,
    int num_edges,
    int num_nodes
);

/**
 * @brief Create GPU graph from dense adjacency matrix
 *
 * @param ctx GPU context
 * @param adjacency Adjacency matrix [num_nodes x num_nodes]
 * @param num_nodes Number of nodes
 * @return GPU graph or NULL on failure
 */
NIMCP_EXPORT nimcp_graph_gpu_t* nimcp_graph_gpu_from_dense(
    nimcp_gpu_context_t* ctx,
    const float* adjacency,
    int num_nodes
);

/**
 * @brief Create GPU graph from CSR format
 *
 * @param ctx GPU context
 * @param row_ptrs CSR row pointers [num_nodes + 1]
 * @param col_indices CSR column indices [num_edges]
 * @param weights Edge weights [num_edges] (NULL for unweighted)
 * @param num_nodes Number of nodes
 * @param num_edges Number of edges
 * @return GPU graph or NULL on failure
 */
NIMCP_EXPORT nimcp_graph_gpu_t* nimcp_graph_gpu_from_csr(
    nimcp_gpu_context_t* ctx,
    const int* row_ptrs,
    const int* col_indices,
    const float* weights,
    int num_nodes,
    int num_edges
);

/**
 * @brief Destroy GPU graph and free resources
 *
 * @param graph Graph to destroy
 */
NIMCP_EXPORT void nimcp_graph_gpu_destroy(nimcp_graph_gpu_t* graph);

/**
 * @brief Convert dense graph to CSR format
 *
 * @param graph GPU graph (dense)
 * @param threshold Threshold for edge inclusion (edges < threshold are removed)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_graph_gpu_to_csr(nimcp_graph_gpu_t* graph, float threshold);

/**
 * @brief Convert CSR graph to dense format
 *
 * @param graph GPU graph (sparse)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_graph_gpu_to_dense(nimcp_graph_gpu_t* graph);

/**
 * @brief Set node features for graph
 *
 * @param graph GPU graph
 * @param features Node features [num_nodes x feature_dim]
 * @param feature_dim Feature dimension
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_graph_gpu_set_features(
    nimcp_graph_gpu_t* graph,
    const float* features,
    int feature_dim
);

//=============================================================================
// Community Detection
//=============================================================================

/**
 * @brief Detect communities using Louvain algorithm
 *
 * WHAT: GPU-accelerated Louvain community detection
 * WHY:  Efficient parallel modularity optimization
 * HOW:  Two-phase approach: local moving + community aggregation
 *
 * ALGORITHM:
 * Phase 1 (Local Moving):
 *   - Each node evaluates modularity gain from moving to neighbor communities
 *   - Nodes move to community with maximum positive gain
 *   - Iterate until convergence
 * Phase 2 (Aggregation):
 *   - Aggregate nodes into super-nodes by community
 *   - Build new graph with community edges
 *   - Repeat Phase 1 on aggregated graph
 *
 * PARALLELIZATION:
 * - Parallel modularity gain computation across all nodes
 * - Parallel community assignment updates
 * - Parallel edge aggregation
 *
 * COMPLEXITY: O(N log N) average case
 *
 * @param ctx GPU context
 * @param graph GPU graph
 * @param resolution Resolution parameter (>1 finds more communities)
 * @param max_iterations Maximum iterations per phase
 * @param min_modularity_gain Convergence threshold
 * @return Community detection result or NULL on failure
 */
NIMCP_EXPORT nimcp_community_result_gpu_t* nimcp_community_detect_louvain(
    nimcp_gpu_context_t* ctx,
    nimcp_graph_gpu_t* graph,
    float resolution,
    int max_iterations,
    float min_modularity_gain
);

/**
 * @brief Detect communities using Label Propagation
 *
 * WHAT: GPU-accelerated Label Propagation Algorithm (LPA)
 * WHY:  Simple, fast community detection without hyperparameters
 * HOW:  Nodes adopt most frequent label among neighbors
 *
 * ALGORITHM:
 *   - Initialize each node with unique label
 *   - Each iteration: node adopts most common neighbor label
 *   - Iterate until convergence or max_iter reached
 *
 * PARALLELIZATION:
 * - Asynchronous label updates (may affect convergence)
 * - Parallel neighbor label counting
 *
 * COMPLEXITY: O(N * max_iter) time, O(N) space
 *
 * @param ctx GPU context
 * @param graph GPU graph
 * @param max_iter Maximum iterations
 * @return Community detection result or NULL on failure
 */
NIMCP_EXPORT nimcp_community_result_gpu_t* nimcp_community_detect_label_prop(
    nimcp_gpu_context_t* ctx,
    nimcp_graph_gpu_t* graph,
    int max_iter
);

/**
 * @brief Compute modularity for given community assignment
 *
 * WHAT: Calculate Newman's modularity Q
 * WHY:  Evaluate quality of community structure
 * HOW:  Q = (1/2m) * sum[A_ij - (k_i * k_j)/2m] * delta(c_i, c_j)
 *
 * @param ctx GPU context
 * @param graph GPU graph
 * @param communities Community assignments [num_nodes]
 * @param num_communities Number of communities
 * @param resolution Resolution parameter
 * @return Modularity score
 */
NIMCP_EXPORT float nimcp_community_compute_modularity(
    nimcp_gpu_context_t* ctx,
    nimcp_graph_gpu_t* graph,
    const int* communities,
    int num_communities,
    float resolution
);

/**
 * @brief Destroy community detection result
 *
 * @param result Result to destroy
 */
NIMCP_EXPORT void nimcp_community_result_gpu_destroy(nimcp_community_result_gpu_t* result);

//=============================================================================
// Graph Metrics
//=============================================================================

/**
 * @brief Compute comprehensive topology metrics
 *
 * WHAT: All-in-one graph metrics computation
 * WHY:  Characterize network properties
 * HOW:  Parallel computation of degree, clustering, centrality
 *
 * @param ctx GPU context
 * @param graph GPU graph
 * @return Topology metrics or NULL on failure
 */
NIMCP_EXPORT nimcp_topology_metrics_gpu_t* nimcp_topology_compute_metrics(
    nimcp_gpu_context_t* ctx,
    nimcp_graph_gpu_t* graph
);

/**
 * @brief Compute node degrees
 *
 * WHAT: Count edges per node
 * WHY:  Basic graph metric, input to many algorithms
 * HOW:  Parallel edge counting
 *
 * @param ctx GPU context
 * @param graph GPU graph
 * @param degree_out Output degree tensor [num_nodes]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_topology_compute_degree(
    nimcp_gpu_context_t* ctx,
    nimcp_graph_gpu_t* graph,
    nimcp_gpu_tensor_t* degree_out
);

/**
 * @brief Compute weighted node degrees
 *
 * @param ctx GPU context
 * @param graph GPU graph
 * @param weighted_degree_out Output weighted degree tensor [num_nodes]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_topology_compute_weighted_degree(
    nimcp_gpu_context_t* ctx,
    nimcp_graph_gpu_t* graph,
    nimcp_gpu_tensor_t* weighted_degree_out
);

/**
 * @brief Compute local clustering coefficients
 *
 * WHAT: Measure local connectivity around each node
 * WHY:  Characterizes local network structure
 * HOW:  C(i) = 2 * triangles(i) / (degree(i) * (degree(i) - 1))
 *
 * PARALLELIZATION:
 * - Parallel triangle counting per node
 * - Shared memory for neighbor list intersection
 *
 * @param ctx GPU context
 * @param graph GPU graph
 * @param clustering_out Output clustering coefficients [num_nodes]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_topology_compute_clustering(
    nimcp_gpu_context_t* ctx,
    nimcp_graph_gpu_t* graph,
    nimcp_gpu_tensor_t* clustering_out
);

/**
 * @brief Count triangles in graph
 *
 * WHAT: Total number of triangles (3-cliques)
 * WHY:  Required for global clustering coefficient
 * HOW:  Parallel triangle enumeration
 *
 * @param ctx GPU context
 * @param graph GPU graph
 * @param count_out Output triangle count
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_topology_count_triangles(
    nimcp_gpu_context_t* ctx,
    nimcp_graph_gpu_t* graph,
    int64_t* count_out
);

/**
 * @brief Compute PageRank scores
 *
 * WHAT: PageRank centrality measure
 * WHY:  Identifies important nodes based on link structure
 * HOW:  Power iteration: PR(i) = (1-d)/N + d * sum(PR(j)/out_degree(j))
 *
 * PARALLELIZATION:
 * - Parallel contribution computation
 * - Parallel score updates
 *
 * @param ctx GPU context
 * @param graph GPU graph
 * @param damping Damping factor (typical: 0.85)
 * @param max_iter Maximum iterations
 * @param tolerance Convergence tolerance
 * @param pagerank_out Output PageRank scores [num_nodes]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_topology_compute_pagerank(
    nimcp_gpu_context_t* ctx,
    nimcp_graph_gpu_t* graph,
    float damping,
    int max_iter,
    float tolerance,
    nimcp_gpu_tensor_t* pagerank_out
);

/**
 * @brief Compute betweenness centrality
 *
 * WHAT: Measure how often node lies on shortest paths
 * WHY:  Identifies bridge/connector nodes
 * HOW:  Brandes' algorithm with parallel BFS
 *
 * ALGORITHM (Brandes, 2001):
 *   For each source node:
 *     1. BFS to find shortest paths
 *     2. Count paths through each node
 *     3. Accumulate dependencies in reverse
 *
 * PARALLELIZATION:
 * - Parallel BFS from multiple sources
 * - Parallel dependency accumulation
 *
 * COMPLEXITY: O(N*M) where N=nodes, M=edges
 *
 * @param ctx GPU context
 * @param graph GPU graph
 * @param normalized Whether to normalize by (N-1)(N-2)
 * @param betweenness_out Output betweenness centrality [num_nodes]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_topology_compute_betweenness(
    nimcp_gpu_context_t* ctx,
    nimcp_graph_gpu_t* graph,
    bool normalized,
    nimcp_gpu_tensor_t* betweenness_out
);

/**
 * @brief Destroy topology metrics result
 *
 * @param metrics Metrics to destroy
 */
NIMCP_EXPORT void nimcp_topology_metrics_gpu_destroy(nimcp_topology_metrics_gpu_t* metrics);

//=============================================================================
// Shortest Path Algorithms
//=============================================================================

/**
 * @brief BFS-based shortest path (unweighted)
 *
 * WHAT: Single-source shortest paths using BFS
 * WHY:  O(V+E) for unweighted graphs
 * HOW:  Frontier-based parallel BFS
 *
 * @param ctx GPU context
 * @param graph GPU graph
 * @param source Source node index
 * @param result Output shortest path result
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_shortest_path_bfs(
    nimcp_gpu_context_t* ctx,
    nimcp_graph_gpu_t* graph,
    int source,
    nimcp_shortest_path_result_gpu_t* result
);

/**
 * @brief Dijkstra's algorithm (weighted)
 *
 * WHAT: Single-source shortest paths with non-negative weights
 * WHY:  Standard weighted shortest path algorithm
 * HOW:  Parallel edge relaxation with frontier
 *
 * PARALLELIZATION:
 * - Parallel edge relaxation
 * - Parallel minimum selection
 *
 * @param ctx GPU context
 * @param graph GPU graph
 * @param source Source node index
 * @param result Output shortest path result
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_shortest_path_dijkstra(
    nimcp_gpu_context_t* ctx,
    nimcp_graph_gpu_t* graph,
    int source,
    nimcp_shortest_path_result_gpu_t* result
);

/**
 * @brief Floyd-Warshall all-pairs shortest paths
 *
 * WHAT: Compute distances between all pairs of nodes
 * WHY:  Required for diameter, average path length
 * HOW:  Blocked parallel Floyd-Warshall
 *
 * PARALLELIZATION:
 * - Tiled/blocked algorithm for cache efficiency
 * - Parallel updates within each block
 *
 * COMPLEXITY: O(N^3) time, O(N^2) space
 *
 * @param ctx GPU context
 * @param graph GPU graph
 * @param result Output APSP result
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_shortest_path_floyd_warshall(
    nimcp_gpu_context_t* ctx,
    nimcp_graph_gpu_t* graph,
    nimcp_apsp_result_gpu_t* result
);

/**
 * @brief Multi-source BFS shortest paths
 *
 * WHAT: BFS from multiple sources simultaneously
 * WHY:  Efficient for computing distances from multiple nodes
 * HOW:  Frontier union across all sources
 *
 * @param ctx GPU context
 * @param graph GPU graph
 * @param sources Source node indices [num_sources]
 * @param num_sources Number of source nodes
 * @param distances_out Output distance matrix [num_sources x num_nodes]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_shortest_path_multi_source_bfs(
    nimcp_gpu_context_t* ctx,
    nimcp_graph_gpu_t* graph,
    const int* sources,
    int num_sources,
    nimcp_gpu_tensor_t* distances_out
);

/**
 * @brief Destroy shortest path result
 *
 * @param result Result to destroy
 */
NIMCP_EXPORT void nimcp_shortest_path_result_gpu_destroy(nimcp_shortest_path_result_gpu_t* result);

/**
 * @brief Destroy APSP result
 *
 * @param result Result to destroy
 */
NIMCP_EXPORT void nimcp_apsp_result_gpu_destroy(nimcp_apsp_result_gpu_t* result);

//=============================================================================
// Network Generation
//=============================================================================

/**
 * @brief Generate Erdos-Renyi random graph
 *
 * WHAT: Random graph with fixed edge probability
 * WHY:  Baseline for comparison, simple random topology
 * HOW:  Each edge exists with probability p
 *
 * @param ctx GPU context
 * @param num_nodes Number of nodes
 * @param edge_probability Probability of edge existence
 * @param seed Random seed (0 for random)
 * @return Generated graph or NULL on failure
 */
NIMCP_EXPORT nimcp_graph_gpu_t* nimcp_graph_generate_erdos_renyi(
    nimcp_gpu_context_t* ctx,
    int num_nodes,
    float edge_probability,
    uint32_t seed
);

/**
 * @brief Generate Barabasi-Albert scale-free graph
 *
 * WHAT: Scale-free graph with preferential attachment
 * WHY:  Models many real-world networks including brain networks
 * HOW:  New nodes connect preferentially to high-degree nodes
 *
 * ALGORITHM:
 *   - Start with small complete graph
 *   - Add nodes one at a time
 *   - Each new node connects to m existing nodes
 *   - Connection probability proportional to degree
 *
 * @param ctx GPU context
 * @param num_nodes Final number of nodes
 * @param m Number of edges for each new node
 * @param seed Random seed (0 for random)
 * @return Generated graph or NULL on failure
 */
NIMCP_EXPORT nimcp_graph_gpu_t* nimcp_graph_generate_barabasi_albert(
    nimcp_gpu_context_t* ctx,
    int num_nodes,
    int m,
    uint32_t seed
);

/**
 * @brief Generate Watts-Strogatz small-world graph
 *
 * WHAT: Small-world graph with high clustering and short paths
 * WHY:  Models brain network properties
 * HOW:  Start with ring lattice, randomly rewire edges
 *
 * ALGORITHM:
 *   - Create ring lattice with k neighbors
 *   - Rewire each edge with probability p
 *   - p=0: regular lattice, p=1: random graph
 *   - Small p gives small-world properties
 *
 * @param ctx GPU context
 * @param num_nodes Number of nodes
 * @param k Number of nearest neighbors in ring
 * @param rewire_prob Rewiring probability
 * @param seed Random seed (0 for random)
 * @return Generated graph or NULL on failure
 */
NIMCP_EXPORT nimcp_graph_gpu_t* nimcp_graph_generate_watts_strogatz(
    nimcp_gpu_context_t* ctx,
    int num_nodes,
    int k,
    float rewire_prob,
    uint32_t seed
);

/**
 * @brief Generate fractal topology graph
 *
 * WHAT: Graph with fractal structure at multiple scales
 * WHY:  Models hierarchical brain organization
 * HOW:  Hierarchical modular structure with scale-invariant patterns
 *
 * PARAMETERS:
 * - fractal_dimension: Controls sparsity (1.5-3.0, cortex ~2.5)
 * - clustering_scale: Local clustering coefficient target
 *
 * @param ctx GPU context
 * @param num_nodes Number of nodes
 * @param fractal_dim Fractal dimension
 * @param cluster_scale Clustering scale factor
 * @param seed Random seed (0 for random)
 * @return Generated graph or NULL on failure
 */
NIMCP_EXPORT nimcp_graph_gpu_t* nimcp_graph_generate_fractal(
    nimcp_gpu_context_t* ctx,
    int num_nodes,
    float fractal_dim,
    float cluster_scale,
    uint32_t seed
);

/**
 * @brief Generate power-law degree distribution graph
 *
 * WHAT: Graph with specified power-law exponent
 * WHY:  Match observed degree distribution
 * HOW:  Configuration model with power-law stubs
 *
 * @param ctx GPU context
 * @param num_nodes Number of nodes
 * @param gamma Power-law exponent (negative, e.g., -2.5)
 * @param min_degree Minimum node degree
 * @param seed Random seed (0 for random)
 * @return Generated graph or NULL on failure
 */
NIMCP_EXPORT nimcp_graph_gpu_t* nimcp_graph_generate_power_law(
    nimcp_gpu_context_t* ctx,
    int num_nodes,
    float gamma,
    int min_degree,
    uint32_t seed
);

//=============================================================================
// Graph Utilities
//=============================================================================

/**
 * @brief Check if graph is valid
 *
 * @param graph Graph to check
 * @return true if valid
 */
NIMCP_EXPORT bool nimcp_graph_gpu_is_valid(const nimcp_graph_gpu_t* graph);

/**
 * @brief Get graph statistics
 *
 * @param graph GPU graph
 * @param avg_degree Output average degree (can be NULL)
 * @param max_degree Output maximum degree (can be NULL)
 * @param density Output edge density (can be NULL)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_graph_gpu_stats(
    nimcp_graph_gpu_t* graph,
    float* avg_degree,
    int* max_degree,
    float* density
);

/**
 * @brief Copy graph to host memory
 *
 * @param graph GPU graph
 * @param adjacency_out Output adjacency matrix (must be pre-allocated)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_graph_gpu_to_host(
    nimcp_graph_gpu_t* graph,
    float* adjacency_out
);

/**
 * @brief Make graph symmetric (undirected)
 *
 * @param graph GPU graph (modified in place)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_graph_gpu_symmetrize(nimcp_graph_gpu_t* graph);

/**
 * @brief Remove self-loops from graph
 *
 * @param graph GPU graph (modified in place)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_graph_gpu_remove_self_loops(nimcp_graph_gpu_t* graph);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_TOPOLOGY_GPU_H
