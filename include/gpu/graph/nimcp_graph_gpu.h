/**
 * @file nimcp_graph_gpu.h
 * @brief GPU-Accelerated Graph Operations with CSR Representation
 *
 * WHAT: CUDA kernels for graph algorithms on GPU
 * WHY:  Massive parallelization of graph traversal and analysis
 * HOW:  CSR (Compressed Sparse Row) format with custom CUDA kernels
 *
 * ARCHITECTURE:
 *
 *   +----------------------------------------------------------+
 *   |                   GPU GRAPH MODULE                       |
 *   |                                                          |
 *   |  +---------------+  +---------------+  +--------------+  |
 *   |  | CSR Storage   |  | BFS/Traversal |  | Metrics      |  |
 *   |  | (row_offsets, |  | (Frontier     |  | (Clustering, |  |
 *   |  |  col_indices) |  |  Expansion)   |  |  Centrality) |  |
 *   |  +---------------+  +---------------+  +--------------+  |
 *   |                          |                              |
 *   |              +-----------------------+                  |
 *   |              |    CUDA Kernels       |                  |
 *   |              | (Parallel Execution)  |                  |
 *   |              +-----------------------+                  |
 *   +---------------------------------------------------------+
 *
 * CSR FORMAT:
 * - row_offsets[i] to row_offsets[i+1] gives edge range for vertex i
 * - col_indices[j] gives destination vertex for edge j
 * - edge_weights[j] gives weight for edge j
 *
 * PARALLELIZATION STRATEGIES:
 * - BFS: Frontier-based, one thread per frontier vertex
 * - Clustering: One thread per vertex, neighbor intersection
 * - Centrality: Parallel reduction for degree computation
 * - Subgraph: Parallel candidate filtering
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_GRAPH_GPU_H
#define NIMCP_GRAPH_GPU_H

#include "gpu/context/nimcp_gpu_context.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include "utils/error/nimcp_error_codes.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

//=============================================================================
// Constants
//=============================================================================

/** Infinite distance for BFS/Dijkstra */
#define NIMCP_GRAPH_INF_DISTANCE 1e30f

/** Maximum supported vertices */
#define NIMCP_GRAPH_MAX_VERTICES (1ULL << 28)

/** Maximum supported edges */
#define NIMCP_GRAPH_MAX_EDGES (1ULL << 30)

/** Default feature dimension */
#define NIMCP_GRAPH_DEFAULT_FEATURE_DIM 64

//=============================================================================
// GPU Graph Structure (CSR Format)
//=============================================================================

/**
 * @brief GPU graph in Compressed Sparse Row (CSR) format
 *
 * WHAT: Efficient GPU-resident graph representation
 * WHY:  CSR enables coalesced memory access for graph traversal
 * HOW:  Separate arrays for row pointers, column indices, weights
 */
typedef struct nimcp_gpu_graph_s {
    size_t num_vertices;         /**< Number of vertices */
    size_t num_edges;            /**< Number of edges */

    // CSR representation (device memory)
    int* d_row_offsets;          /**< Row pointers [num_vertices + 1] */
    int* d_col_indices;          /**< Column indices [num_edges] */
    float* d_edge_weights;       /**< Edge weights [num_edges] (optional) */

    // Vertex features (device memory)
    float* d_vertex_features;    /**< Vertex features [num_vertices * feature_dim] */
    int feature_dim;             /**< Feature vector dimension */

    // Cached vertex data
    int* d_degrees;              /**< Vertex degrees [num_vertices] (cached) */
    float* d_degree_centrality;  /**< Degree centrality [num_vertices] (cached) */

    // GPU context
    nimcp_gpu_context_t* ctx;    /**< GPU context */
    bool owns_data;              /**< Whether graph owns its memory */

} nimcp_gpu_graph_t;

/**
 * @brief BFS result structure
 */
typedef struct nimcp_graph_bfs_result_s {
    float* d_distances;          /**< Distance from source [num_vertices] */
    int* d_predecessors;         /**< Predecessor for path [num_vertices] */
    int* d_visited;              /**< Visited bitmap [num_vertices] */
    size_t num_visited;          /**< Total vertices visited */
    int max_distance;            /**< Maximum distance reached */
} nimcp_graph_bfs_result_t;

/**
 * @brief Subgraph match result
 */
typedef struct nimcp_graph_match_result_s {
    int* d_vertex_mappings;      /**< Vertex mappings [max_matches * pattern_size] */
    float* d_scores;             /**< Match scores [max_matches] */
    size_t num_matches;          /**< Number of matches found */
    size_t max_matches;          /**< Maximum matches requested */
    size_t pattern_size;         /**< Size of pattern graph */
} nimcp_graph_match_result_t;

/**
 * @brief Small-world metrics result
 */
typedef struct nimcp_graph_small_world_s {
    float avg_clustering;        /**< Average clustering coefficient */
    float avg_path_length;       /**< Average shortest path length */
    float small_world_sigma;     /**< Small-world coefficient (sigma) */
    float small_world_omega;     /**< Small-world coefficient (omega) */
} nimcp_graph_small_world_t;

//=============================================================================
// Graph Creation and Destruction
//=============================================================================

/**
 * @brief Create an empty GPU graph
 *
 * @param ctx GPU context
 * @param num_vertices Number of vertices
 * @param num_edges Number of edges
 * @return Graph structure or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_graph_t* nimcp_gpu_graph_create(
    nimcp_gpu_context_t* ctx,
    size_t num_vertices,
    size_t num_edges
);

/**
 * @brief Destroy a GPU graph
 *
 * @param graph Graph to destroy (can be NULL)
 */
NIMCP_EXPORT void nimcp_gpu_graph_destroy(nimcp_gpu_graph_t* graph);

/**
 * @brief Create GPU graph from dense adjacency matrix
 *
 * WHAT: Converts dense matrix to CSR format on GPU
 * WHY:  Many algorithms represent graphs as adjacency matrices
 * HOW:  Parallel counting, prefix sum, and copy
 *
 * @param ctx GPU context
 * @param adjacency Dense adjacency matrix [n x n] (host memory)
 * @param n Number of vertices
 * @param threshold Minimum edge weight to include
 * @return GPU graph in CSR format or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_graph_t* nimcp_gpu_graph_from_adjacency(
    nimcp_gpu_context_t* ctx,
    const float* adjacency,
    size_t n,
    float threshold
);

/**
 * @brief Create GPU graph from edge list
 *
 * @param ctx GPU context
 * @param src_vertices Source vertex indices [num_edges] (host)
 * @param dst_vertices Destination vertex indices [num_edges] (host)
 * @param weights Edge weights [num_edges] (host, can be NULL)
 * @param num_edges Number of edges
 * @param num_vertices Number of vertices (0 to auto-detect)
 * @return GPU graph in CSR format or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_graph_t* nimcp_gpu_graph_from_edge_list(
    nimcp_gpu_context_t* ctx,
    const int* src_vertices,
    const int* dst_vertices,
    const float* weights,
    size_t num_edges,
    size_t num_vertices
);

/**
 * @brief Create GPU graph from CSR arrays
 *
 * @param ctx GPU context
 * @param row_offsets CSR row offsets [num_vertices + 1] (host)
 * @param col_indices CSR column indices [num_edges] (host)
 * @param weights Edge weights [num_edges] (host, can be NULL)
 * @param num_vertices Number of vertices
 * @param num_edges Number of edges
 * @return GPU graph or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_graph_t* nimcp_gpu_graph_from_csr(
    nimcp_gpu_context_t* ctx,
    const int* row_offsets,
    const int* col_indices,
    const float* weights,
    size_t num_vertices,
    size_t num_edges
);

/**
 * @brief Clone a GPU graph
 *
 * @param graph Source graph
 * @return Cloned graph or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_graph_t* nimcp_gpu_graph_clone(
    const nimcp_gpu_graph_t* graph
);

/**
 * @brief Set vertex features
 *
 * @param graph GPU graph
 * @param features Feature matrix [num_vertices x feature_dim] (host)
 * @param feature_dim Feature dimension
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_gpu_graph_set_features(
    nimcp_gpu_graph_t* graph,
    const float* features,
    int feature_dim
);

//=============================================================================
// BFS Traversal
//=============================================================================

/**
 * @brief Parallel BFS from single source
 *
 * WHAT: Breadth-first search using frontier expansion
 * WHY:  Fundamental for shortest paths in unweighted graphs
 * HOW:  Parallel frontier expansion with atomic updates
 *
 * @param graph GPU graph
 * @param source Source vertex ID
 * @param distances Output: distance from source [num_vertices] (device)
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_gpu_graph_bfs(
    nimcp_gpu_graph_t* graph,
    int source,
    float* distances
);

/**
 * @brief Parallel BFS with full result
 *
 * @param graph GPU graph
 * @param source Source vertex ID
 * @param result Output: full BFS result (caller must free)
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_gpu_graph_bfs_full(
    nimcp_gpu_graph_t* graph,
    int source,
    nimcp_graph_bfs_result_t** result
);

/**
 * @brief Free BFS result
 */
NIMCP_EXPORT void nimcp_gpu_graph_bfs_result_destroy(
    nimcp_graph_bfs_result_t* result
);

/**
 * @brief Multi-source BFS
 *
 * @param graph GPU graph
 * @param sources Source vertices [num_sources] (host)
 * @param num_sources Number of sources
 * @param distances Output: min distance to any source [num_vertices] (device)
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_gpu_graph_bfs_multi_source(
    nimcp_gpu_graph_t* graph,
    const int* sources,
    size_t num_sources,
    float* distances
);

//=============================================================================
// Clustering Coefficient
//=============================================================================

/**
 * @brief Compute local clustering coefficients
 *
 * WHAT: Measures how connected a vertex's neighbors are
 * WHY:  Key metric for small-world networks
 * HOW:  Parallel triangle counting with neighbor intersection
 *
 * clustering[i] = 2 * triangles[i] / (degree[i] * (degree[i] - 1))
 *
 * @param graph GPU graph
 * @param coefficients Output: clustering coefficients [num_vertices] (device)
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_gpu_graph_clustering_coeff(
    nimcp_gpu_graph_t* graph,
    float* coefficients
);

/**
 * @brief Compute average clustering coefficient
 *
 * @param graph GPU graph
 * @param avg_clustering Output: average clustering coefficient
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_gpu_graph_avg_clustering(
    nimcp_gpu_graph_t* graph,
    float* avg_clustering
);

//=============================================================================
// Centrality Measures
//=============================================================================

/**
 * @brief Compute degree centrality
 *
 * WHAT: Centrality based on vertex degree
 * WHY:  Identifies hub vertices
 * HOW:  degree_centrality[i] = degree[i] / (n - 1)
 *
 * @param graph GPU graph
 * @param centrality Output: degree centrality [num_vertices] (device)
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_gpu_graph_degree_centrality(
    nimcp_gpu_graph_t* graph,
    float* centrality
);

/**
 * @brief Find hub vertices (high degree centrality)
 *
 * @param graph GPU graph
 * @param threshold Minimum centrality to be considered a hub
 * @param hub_ids Output: hub vertex IDs [max_hubs] (host, caller allocated)
 * @param max_hubs Maximum number of hubs to return
 * @return Number of hubs found
 */
NIMCP_EXPORT size_t nimcp_gpu_graph_find_hubs(
    nimcp_gpu_graph_t* graph,
    float threshold,
    int* hub_ids,
    size_t max_hubs
);

/**
 * @brief Compute betweenness centrality (approximate)
 *
 * Uses sampling for large graphs.
 *
 * @param graph GPU graph
 * @param centrality Output: betweenness centrality [num_vertices] (device)
 * @param num_samples Number of source vertices to sample (0 for all)
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_gpu_graph_betweenness_centrality(
    nimcp_gpu_graph_t* graph,
    float* centrality,
    size_t num_samples
);

//=============================================================================
// Small-World Metrics
//=============================================================================

/**
 * @brief Compute small-world coefficient
 *
 * WHAT: Measures small-world property (high clustering, short paths)
 * WHY:  Key characteristic of biological neural networks
 * HOW:  sigma = (C/C_rand) / (L/L_rand)
 *
 * @param graph GPU graph
 * @return Small-world coefficient sigma (0 if error)
 */
NIMCP_EXPORT float nimcp_gpu_graph_small_world_coeff(
    nimcp_gpu_graph_t* graph
);

/**
 * @brief Compute full small-world metrics
 *
 * @param graph GPU graph
 * @param metrics Output: small-world metrics structure
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_gpu_graph_small_world_metrics(
    nimcp_gpu_graph_t* graph,
    nimcp_graph_small_world_t* metrics
);

/**
 * @brief Compute average path length (sampled)
 *
 * @param graph GPU graph
 * @param num_samples Number of source vertices to sample
 * @param avg_path_length Output: average shortest path length
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_gpu_graph_avg_path_length(
    nimcp_gpu_graph_t* graph,
    size_t num_samples,
    float* avg_path_length
);

//=============================================================================
// Modularity and Community Detection
//=============================================================================

/**
 * @brief Compute modularity score for given community labels
 *
 * WHAT: Measures quality of community partition
 * WHY:  Evaluates community detection algorithms
 * HOW:  Q = (1/2m) * sum_ij[(A_ij - k_i*k_j/2m) * delta(c_i, c_j)]
 *
 * @param graph GPU graph
 * @param community_labels Community label for each vertex [num_vertices] (host)
 * @param modularity Output: modularity score
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_gpu_graph_modularity(
    nimcp_gpu_graph_t* graph,
    const int* community_labels,
    float* modularity
);

/**
 * @brief Count number of communities
 *
 * @param graph GPU graph
 * @param community_labels Community labels [num_vertices] (host)
 * @return Number of unique communities
 */
NIMCP_EXPORT size_t nimcp_gpu_graph_count_communities(
    nimcp_gpu_graph_t* graph,
    const int* community_labels
);

//=============================================================================
// Subgraph Matching
//=============================================================================

/**
 * @brief Simple subgraph pattern matching
 *
 * WHAT: Finds occurrences of pattern graph in target graph
 * WHY:  Detects specific connectivity patterns (motifs)
 * HOW:  Parallel candidate filtering with verification
 *
 * @param target Target graph to search in
 * @param pattern Pattern graph to find
 * @param max_matches Maximum number of matches to find
 * @param result Output: match result (caller must free)
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_gpu_graph_subgraph_match(
    nimcp_gpu_graph_t* target,
    nimcp_gpu_graph_t* pattern,
    size_t max_matches,
    nimcp_graph_match_result_t** result
);

/**
 * @brief Free subgraph match result
 */
NIMCP_EXPORT void nimcp_gpu_graph_match_result_destroy(
    nimcp_graph_match_result_t* result
);

/**
 * @brief Count triangle motifs
 *
 * @param graph GPU graph
 * @param triangle_count Output: number of triangles
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_gpu_graph_count_triangles(
    nimcp_gpu_graph_t* graph,
    size_t* triangle_count
);

//=============================================================================
// Graph Statistics
//=============================================================================

/**
 * @brief Compute vertex degrees
 *
 * @param graph GPU graph
 * @param degrees Output: vertex degrees [num_vertices] (device)
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_gpu_graph_compute_degrees(
    nimcp_gpu_graph_t* graph,
    int* degrees
);

/**
 * @brief Get graph statistics
 *
 * @param graph GPU graph
 * @param avg_degree Output: average degree
 * @param max_degree Output: maximum degree
 * @param min_degree Output: minimum degree
 * @param density Output: graph density
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_gpu_graph_stats(
    nimcp_gpu_graph_t* graph,
    float* avg_degree,
    int* max_degree,
    int* min_degree,
    float* density
);

/**
 * @brief Check if graph is connected
 *
 * @param graph GPU graph
 * @param is_connected Output: true if connected
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_gpu_graph_is_connected(
    nimcp_gpu_graph_t* graph,
    bool* is_connected
);

//=============================================================================
// Data Transfer
//=============================================================================

/**
 * @brief Copy CSR data to host
 *
 * @param graph GPU graph
 * @param row_offsets Output: row offsets [num_vertices + 1] (host)
 * @param col_indices Output: column indices [num_edges] (host)
 * @param weights Output: edge weights [num_edges] (host, can be NULL)
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_gpu_graph_to_host(
    const nimcp_gpu_graph_t* graph,
    int* row_offsets,
    int* col_indices,
    float* weights
);

/**
 * @brief Copy vertex features to host
 *
 * @param graph GPU graph
 * @param features Output: features [num_vertices x feature_dim] (host)
 * @return NIMCP_SUCCESS or error code
 */
NIMCP_EXPORT nimcp_error_t nimcp_gpu_graph_features_to_host(
    const nimcp_gpu_graph_t* graph,
    float* features
);

/**
 * @brief Check if graph is valid
 *
 * @param graph GPU graph
 * @return true if graph is valid and initialized
 */
NIMCP_EXPORT bool nimcp_gpu_graph_is_valid(
    const nimcp_gpu_graph_t* graph
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GRAPH_GPU_H */
