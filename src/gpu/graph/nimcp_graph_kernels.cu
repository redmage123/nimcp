/**
 * @file nimcp_graph_kernels.cu
 * @brief GPU Graph CUDA Kernels Implementation
 *
 * WHAT: CUDA kernels for graph algorithms
 * WHY:  Massive parallelization of graph operations
 * HOW:  CSR format with frontier-based BFS, parallel metrics
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

// Include CUDA headers FIRST (before any extern "C" blocks from our headers)
#include <cuda_runtime.h>
#include <math.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>

// Now include our headers (which have extern "C" blocks)
#include "gpu/graph/nimcp_graph_gpu.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// CUDA Error Checking - Using recovery-integrated macros
//=============================================================================

// Map legacy macros to recovery-integrated versions for self-healing
#define CUDA_CHECK(call) NIMCP_CUDA_RECOVER(call, GPU_ERROR_CUDA_RUNTIME)
#define CUDA_CHECK_ERR(call) NIMCP_CUDA_RECOVER_ERROR(call, GPU_ERROR_CUDA_RUNTIME)
#define CUDA_CHECK_PTR(call) NIMCP_CUDA_RECOVER_NULL(call, GPU_ERROR_OUT_OF_MEMORY)

//=============================================================================
// Kernel Configuration
//=============================================================================

#define BLOCK_SIZE 256
#define WARP_SIZE 32
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)
#define INF_DISTANCE NIMCP_GRAPH_INF_DISTANCE

//=============================================================================
// CUDA Kernels - Degree Computation
//=============================================================================

/**
 * @brief Compute vertex degrees from CSR format
 */
__global__ void kernel_compute_degrees(
    const int* row_offsets,
    int* degrees,
    int num_vertices)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_vertices) return;

    degrees[idx] = row_offsets[idx + 1] - row_offsets[idx];
}

/**
 * @brief Compute degree centrality
 */
__global__ void kernel_degree_centrality(
    const int* degrees,
    float* centrality,
    int num_vertices)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_vertices) return;

    centrality[idx] = (float)degrees[idx] / (float)(num_vertices - 1);
}

//=============================================================================
// CUDA Kernels - BFS Traversal
//=============================================================================

/**
 * @brief Initialize BFS distances
 */
__global__ void kernel_bfs_init(
    float* distances,
    int* visited,
    int source,
    int num_vertices)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_vertices) return;

    if (idx == source) {
        distances[idx] = 0.0f;
        visited[idx] = 1;
    } else {
        distances[idx] = INF_DISTANCE;
        visited[idx] = 0;
    }
}

/**
 * @brief BFS frontier expansion
 */
__global__ void kernel_bfs_expand(
    const int* row_offsets,
    const int* col_indices,
    const int* current_frontier,
    int* next_frontier,
    int* next_frontier_size,
    float* distances,
    int* visited,
    int* predecessors,
    float current_distance,
    int frontier_size)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= frontier_size) return;

    int node = current_frontier[idx];
    int start = row_offsets[node];
    int end = row_offsets[node + 1];

    for (int e = start; e < end; e++) {
        int neighbor = col_indices[e];

        // Atomic check-and-set for visited
        int old_visited = atomicCAS(&visited[neighbor], 0, 1);
        if (old_visited == 0) {
            // First time visiting this node
            distances[neighbor] = current_distance + 1.0f;
            if (predecessors) {
                predecessors[neighbor] = node;
            }

            // Add to next frontier
            int pos = atomicAdd(next_frontier_size, 1);
            next_frontier[pos] = neighbor;
        }
    }
}

//=============================================================================
// CUDA Kernels - Clustering Coefficient
//=============================================================================

/**
 * @brief Count triangles for each vertex (local clustering)
 *
 * For clustering coefficient, we count how many pairs of neighbors
 * of a vertex are connected. This is the number of edges between
 * neighbors (triangles the vertex participates in).
 */
__global__ void kernel_count_triangles(
    const int* row_offsets,
    const int* col_indices,
    int* triangle_count,
    int num_vertices)
{
    int node = blockIdx.x * blockDim.x + threadIdx.x;
    if (node >= num_vertices) return;

    int count = 0;
    int start_u = row_offsets[node];
    int end_u = row_offsets[node + 1];

    // For each pair of neighbors (v1, v2) of node, check if they're connected
    for (int i = start_u; i < end_u; i++) {
        int v1 = col_indices[i];

        for (int j = i + 1; j < end_u; j++) {
            int v2 = col_indices[j];

            // Check if v1 and v2 are connected (binary search in v1's neighbors)
            int v1_start = row_offsets[v1];
            int v1_end = row_offsets[v1 + 1];

            // Binary search for v2 in v1's neighbor list
            int lo = v1_start, hi = v1_end;
            while (lo < hi) {
                int mid = lo + (hi - lo) / 2;
                int neighbor = col_indices[mid];
                if (neighbor == v2) {
                    count++;  // Found edge v1-v2, so (node, v1, v2) form a triangle
                    break;
                } else if (neighbor < v2) {
                    lo = mid + 1;
                } else {
                    hi = mid;
                }
            }
        }
    }

    triangle_count[node] = count;
}

/**
 * @brief Compute clustering coefficient from triangle counts
 */
__global__ void kernel_clustering_coefficient(
    const int* degrees,
    const int* triangle_count,
    float* clustering,
    int num_vertices)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_vertices) return;

    int deg = degrees[idx];
    if (deg < 2) {
        clustering[idx] = 0.0f;
    } else {
        float max_triangles = (float)(deg * (deg - 1)) / 2.0f;
        clustering[idx] = (float)triangle_count[idx] / max_triangles;
    }
}

//=============================================================================
// CUDA Kernels - Modularity
//=============================================================================

/**
 * @brief Compute modularity contribution per edge
 */
__global__ void kernel_modularity(
    const int* row_offsets,
    const int* col_indices,
    const float* edge_weights,
    const int* degrees,
    const int* community_labels,
    float* modularity_contrib,
    float total_weight,
    int num_vertices)
{
    int node = blockIdx.x * blockDim.x + threadIdx.x;
    if (node >= num_vertices) return;

    float contrib = 0.0f;
    int start = row_offsets[node];
    int end = row_offsets[node + 1];
    int comm_i = community_labels[node];
    float k_i = (float)degrees[node];

    for (int e = start; e < end; e++) {
        int neighbor = col_indices[e];
        int comm_j = community_labels[neighbor];

        if (comm_i == comm_j) {
            float weight = edge_weights ? edge_weights[e] : 1.0f;
            float k_j = (float)degrees[neighbor];
            float expected = (k_i * k_j) / (2.0f * total_weight);
            contrib += weight - expected;
        }
    }

    modularity_contrib[node] = contrib;
}

//=============================================================================
// CUDA Kernels - Small World Metrics
//=============================================================================

/**
 * @brief Compute path length contributions from BFS results
 */
__global__ void kernel_sum_distances(
    const float* distances,
    float* total_distance,
    int* reachable_count,
    int num_vertices)
{
    extern __shared__ float sdata[];
    float* s_dist = sdata;
    int* s_count = (int*)&sdata[blockDim.x];

    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int tid = threadIdx.x;

    float dist = 0.0f;
    int count = 0;
    if (idx < num_vertices) {
        float d = distances[idx];
        if (d > 0.0f && d < INF_DISTANCE) {
            dist = d;
            count = 1;
        }
    }

    s_dist[tid] = dist;
    s_count[tid] = count;
    __syncthreads();

    // Reduction
    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_dist[tid] += s_dist[tid + s];
            s_count[tid] += s_count[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        atomicAdd(total_distance, s_dist[0]);
        atomicAdd(reachable_count, s_count[0]);
    }
}

//=============================================================================
// CUDA Kernels - Subgraph Matching
//=============================================================================

/**
 * @brief Filter candidate vertices by degree
 */
__global__ void kernel_filter_candidates(
    const int* target_degrees,
    int pattern_degree,
    int* candidates,
    int* candidate_count,
    int num_vertices)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_vertices) return;

    if (target_degrees[idx] >= pattern_degree) {
        int pos = atomicAdd(candidate_count, 1);
        candidates[pos] = idx;
    }
}

//=============================================================================
// CUDA Kernels - Dense to CSR Conversion
//=============================================================================

/**
 * @brief Count non-zero edges per row
 */
__global__ void kernel_count_edges_per_row(
    const float* adjacency,
    int* edge_counts,
    int n,
    float threshold)
{
    int row = blockIdx.x * blockDim.x + threadIdx.x;
    if (row >= n) return;

    int count = 0;
    for (int col = 0; col < n; col++) {
        float val = adjacency[row * n + col];
        if (fabsf(val) > threshold && row != col) {
            count++;
        }
    }
    edge_counts[row] = count;
}

/**
 * @brief Fill CSR col_indices and weights from dense matrix
 */
__global__ void kernel_fill_csr(
    const float* adjacency,
    const int* row_offsets,
    int* col_indices,
    float* weights,
    int n,
    float threshold)
{
    int row = blockIdx.x * blockDim.x + threadIdx.x;
    if (row >= n) return;

    int offset = row_offsets[row];
    for (int col = 0; col < n; col++) {
        float val = adjacency[row * n + col];
        if (fabsf(val) > threshold && row != col) {
            col_indices[offset] = col;
            if (weights) {
                weights[offset] = val;
            }
            offset++;
        }
    }
}

//=============================================================================
// Graph Creation/Destruction
//=============================================================================

extern "C" {

nimcp_gpu_graph_t* nimcp_gpu_graph_create(
    nimcp_gpu_context_t* ctx,
    size_t num_vertices,
    size_t num_edges)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        return NULL;
    }

    if (num_vertices > NIMCP_GRAPH_MAX_VERTICES || num_edges > NIMCP_GRAPH_MAX_EDGES) {
        return NULL;
    }

    nimcp_gpu_graph_t* graph = (nimcp_gpu_graph_t*)nimcp_calloc(1, sizeof(nimcp_gpu_graph_t));
    if (!graph) {
        return NULL;
    }

    graph->num_vertices = num_vertices;
    graph->num_edges = num_edges;
    graph->ctx = ctx;
    graph->owns_data = true;
    graph->feature_dim = 0;

    // Allocate CSR arrays
    CUDA_CHECK_PTR(cudaMalloc(&graph->d_row_offsets, (num_vertices + 1) * sizeof(int)));
    CUDA_CHECK_PTR(cudaMalloc(&graph->d_col_indices, num_edges * sizeof(int)));
    CUDA_CHECK_PTR(cudaMalloc(&graph->d_edge_weights, num_edges * sizeof(float)));

    // Initialize to zero
    cudaMemset(graph->d_row_offsets, 0, (num_vertices + 1) * sizeof(int));
    cudaMemset(graph->d_col_indices, 0, num_edges * sizeof(int));
    cudaMemset(graph->d_edge_weights, 0, num_edges * sizeof(float));

    return graph;
}

void nimcp_gpu_graph_destroy(nimcp_gpu_graph_t* graph)
{
    if (!graph) {
        return;
    }

    if (graph->owns_data) {
        if (graph->d_row_offsets) cudaFree(graph->d_row_offsets);
        if (graph->d_col_indices) cudaFree(graph->d_col_indices);
        if (graph->d_edge_weights) cudaFree(graph->d_edge_weights);
        if (graph->d_vertex_features) cudaFree(graph->d_vertex_features);
        if (graph->d_degrees) cudaFree(graph->d_degrees);
        if (graph->d_degree_centrality) cudaFree(graph->d_degree_centrality);
    }

    nimcp_free(graph);
}

nimcp_gpu_graph_t* nimcp_gpu_graph_from_adjacency(
    nimcp_gpu_context_t* ctx,
    const float* adjacency,
    size_t n,
    float threshold)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !adjacency || n == 0) {
        return NULL;
    }

    // Allocate device memory for adjacency
    float* d_adj = NULL;
    CUDA_CHECK_PTR(cudaMalloc(&d_adj, n * n * sizeof(float)));
    CUDA_CHECK_PTR(cudaMemcpy(d_adj, adjacency, n * n * sizeof(float), cudaMemcpyHostToDevice));

    // Count edges per row
    int* d_edge_counts = NULL;
    CUDA_CHECK_PTR(cudaMalloc(&d_edge_counts, n * sizeof(int)));

    kernel_count_edges_per_row<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        d_adj, d_edge_counts, n, threshold);
    cudaDeviceSynchronize();

    // Copy edge counts to host and compute row_offsets
    int* h_edge_counts = (int*)nimcp_malloc(n * sizeof(int));
    cudaMemcpy(h_edge_counts, d_edge_counts, n * sizeof(int), cudaMemcpyDeviceToHost);

    int* h_row_offsets = (int*)nimcp_malloc((n + 1) * sizeof(int));
    h_row_offsets[0] = 0;
    for (size_t i = 0; i < n; i++) {
        h_row_offsets[i + 1] = h_row_offsets[i] + h_edge_counts[i];
    }
    size_t num_edges = h_row_offsets[n];

    // Create graph
    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_create(ctx, n, num_edges);
    if (!graph) {
        cudaFree(d_adj);
        cudaFree(d_edge_counts);
        nimcp_free(h_edge_counts);
        nimcp_free(h_row_offsets);
        return NULL;
    }

    // Copy row_offsets to device
    cudaMemcpy(graph->d_row_offsets, h_row_offsets, (n + 1) * sizeof(int), cudaMemcpyHostToDevice);

    // Fill col_indices and weights
    kernel_fill_csr<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        d_adj, graph->d_row_offsets, graph->d_col_indices, graph->d_edge_weights, n, threshold);
    cudaDeviceSynchronize();

    // Cleanup
    cudaFree(d_adj);
    cudaFree(d_edge_counts);
    nimcp_free(h_edge_counts);
    nimcp_free(h_row_offsets);

    return graph;
}

nimcp_gpu_graph_t* nimcp_gpu_graph_from_edge_list(
    nimcp_gpu_context_t* ctx,
    const int* src_vertices,
    const int* dst_vertices,
    const float* weights,
    size_t num_edges,
    size_t num_vertices)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !src_vertices || !dst_vertices || num_edges == 0) {
        return NULL;
    }

    // Auto-detect num_vertices if not provided
    if (num_vertices == 0) {
        for (size_t i = 0; i < num_edges; i++) {
            if (src_vertices[i] >= (int)num_vertices) num_vertices = src_vertices[i] + 1;
            if (dst_vertices[i] >= (int)num_vertices) num_vertices = dst_vertices[i] + 1;
        }
    }

    // Count edges per source vertex
    int* edge_counts = (int*)nimcp_calloc(num_vertices, sizeof(int));
    for (size_t i = 0; i < num_edges; i++) {
        edge_counts[src_vertices[i]]++;
    }

    // Build row_offsets
    int* row_offsets = (int*)nimcp_malloc((num_vertices + 1) * sizeof(int));
    row_offsets[0] = 0;
    for (size_t i = 0; i < num_vertices; i++) {
        row_offsets[i + 1] = row_offsets[i] + edge_counts[i];
    }

    // Build col_indices and weights (using insertion sort per row)
    int* col_indices = (int*)nimcp_malloc(num_edges * sizeof(int));
    float* edge_weights = weights ? (float*)nimcp_malloc(num_edges * sizeof(float)) : NULL;

    int* current_pos = (int*)nimcp_calloc(num_vertices, sizeof(int));
    for (size_t i = 0; i < num_edges; i++) {
        int src = src_vertices[i];
        int pos = row_offsets[src] + current_pos[src];
        col_indices[pos] = dst_vertices[i];
        if (edge_weights) {
            edge_weights[pos] = weights[i];
        }
        current_pos[src]++;
    }

    // Create graph from CSR
    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_from_csr(
        ctx, row_offsets, col_indices, edge_weights, num_vertices, num_edges);

    // Cleanup
    nimcp_free(edge_counts);
    nimcp_free(row_offsets);
    nimcp_free(col_indices);
    if (edge_weights) nimcp_free(edge_weights);
    nimcp_free(current_pos);

    return graph;
}

nimcp_gpu_graph_t* nimcp_gpu_graph_from_csr(
    nimcp_gpu_context_t* ctx,
    const int* row_offsets,
    const int* col_indices,
    const float* weights,
    size_t num_vertices,
    size_t num_edges)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !row_offsets || !col_indices) {
        return NULL;
    }

    nimcp_gpu_graph_t* graph = nimcp_gpu_graph_create(ctx, num_vertices, num_edges);
    if (!graph) {
        return NULL;
    }

    // Copy CSR data to device
    cudaError_t err;
    err = cudaMemcpy(graph->d_row_offsets, row_offsets, (num_vertices + 1) * sizeof(int),
                     cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        nimcp_gpu_graph_destroy(graph);
        return NULL;
    }

    err = cudaMemcpy(graph->d_col_indices, col_indices, num_edges * sizeof(int),
                     cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        nimcp_gpu_graph_destroy(graph);
        return NULL;
    }

    if (weights) {
        err = cudaMemcpy(graph->d_edge_weights, weights, num_edges * sizeof(float),
                         cudaMemcpyHostToDevice);
        if (err != cudaSuccess) {
            nimcp_gpu_graph_destroy(graph);
            return NULL;
        }
    } else {
        // Fill with 1.0 for unweighted
        float* h_ones = (float*)nimcp_malloc(num_edges * sizeof(float));
        for (size_t i = 0; i < num_edges; i++) h_ones[i] = 1.0f;
        cudaMemcpy(graph->d_edge_weights, h_ones, num_edges * sizeof(float), cudaMemcpyHostToDevice);
        nimcp_free(h_ones);
    }

    return graph;
}

nimcp_gpu_graph_t* nimcp_gpu_graph_clone(const nimcp_gpu_graph_t* graph)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !graph->ctx) {
        return NULL;
    }

    nimcp_gpu_graph_t* clone = nimcp_gpu_graph_create(
        graph->ctx, graph->num_vertices, graph->num_edges);
    if (!clone) {
        return NULL;
    }

    // Copy CSR data
    cudaMemcpy(clone->d_row_offsets, graph->d_row_offsets,
               (graph->num_vertices + 1) * sizeof(int), cudaMemcpyDeviceToDevice);
    cudaMemcpy(clone->d_col_indices, graph->d_col_indices,
               graph->num_edges * sizeof(int), cudaMemcpyDeviceToDevice);
    cudaMemcpy(clone->d_edge_weights, graph->d_edge_weights,
               graph->num_edges * sizeof(float), cudaMemcpyDeviceToDevice);

    // Copy features if present
    if (graph->d_vertex_features && graph->feature_dim > 0) {
        size_t feature_size = graph->num_vertices * graph->feature_dim * sizeof(float);
        cudaMalloc(&clone->d_vertex_features, feature_size);
        cudaMemcpy(clone->d_vertex_features, graph->d_vertex_features,
                   feature_size, cudaMemcpyDeviceToDevice);
        clone->feature_dim = graph->feature_dim;
    }

    return clone;
}

nimcp_error_t nimcp_gpu_graph_set_features(
    nimcp_gpu_graph_t* graph,
    const float* features,
    int feature_dim)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !features || feature_dim <= 0) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    size_t size = graph->num_vertices * feature_dim * sizeof(float);

    if (graph->d_vertex_features) {
        cudaFree(graph->d_vertex_features);
    }

    CUDA_CHECK_ERR(cudaMalloc(&graph->d_vertex_features, size));
    CUDA_CHECK_ERR(cudaMemcpy(graph->d_vertex_features, features, size, cudaMemcpyHostToDevice));

    graph->feature_dim = feature_dim;
    return NIMCP_SUCCESS;
}

//=============================================================================
// BFS Implementation
//=============================================================================

nimcp_error_t nimcp_gpu_graph_bfs(
    nimcp_gpu_graph_t* graph,
    int source,
    float* distances)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !distances || source < 0 || (size_t)source >= graph->num_vertices) {
        return NIMCP_ERROR_INVALID_PARAMETER;
    }

    int n = (int)graph->num_vertices;

    // Allocate temporary buffers
    int* d_visited = NULL;
    int* d_frontier[2] = {NULL, NULL};
    int* d_frontier_size = NULL;

    CUDA_CHECK_ERR(cudaMalloc(&d_visited, n * sizeof(int)));
    CUDA_CHECK_ERR(cudaMalloc(&d_frontier[0], n * sizeof(int)));
    CUDA_CHECK_ERR(cudaMalloc(&d_frontier[1], n * sizeof(int)));
    CUDA_CHECK_ERR(cudaMalloc(&d_frontier_size, sizeof(int)));

    // Initialize BFS
    kernel_bfs_init<<<GRID_SIZE(n), BLOCK_SIZE>>>(distances, d_visited, source, n);

    // Initialize frontier with source
    int h_source = source;
    cudaMemcpy(d_frontier[0], &h_source, sizeof(int), cudaMemcpyHostToDevice);

    int current = 0;
    int h_frontier_size = 1;
    float depth = 0.0f;

    while (h_frontier_size > 0) {
        // Reset next frontier size
        cudaMemset(d_frontier_size, 0, sizeof(int));

        // Expand frontier
        kernel_bfs_expand<<<GRID_SIZE(h_frontier_size), BLOCK_SIZE>>>(
            graph->d_row_offsets,
            graph->d_col_indices,
            d_frontier[current],
            d_frontier[1 - current],
            d_frontier_size,
            distances,
            d_visited,
            NULL,  // No predecessors
            depth,
            h_frontier_size
        );
        cudaDeviceSynchronize();

        // Get next frontier size
        cudaMemcpy(&h_frontier_size, d_frontier_size, sizeof(int), cudaMemcpyDeviceToHost);

        // Swap frontiers
        current = 1 - current;
        depth += 1.0f;
    }

    // Cleanup
    cudaFree(d_visited);
    cudaFree(d_frontier[0]);
    cudaFree(d_frontier[1]);
    cudaFree(d_frontier_size);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gpu_graph_bfs_full(
    nimcp_gpu_graph_t* graph,
    int source,
    nimcp_graph_bfs_result_t** result)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !result || source < 0 || (size_t)source >= graph->num_vertices) {
        return NIMCP_ERROR_INVALID_PARAMETER;
    }

    int n = (int)graph->num_vertices;

    nimcp_graph_bfs_result_t* res = (nimcp_graph_bfs_result_t*)nimcp_calloc(1, sizeof(nimcp_graph_bfs_result_t));
    if (!res) {
        return NIMCP_ERROR_NO_MEMORY;
    }

    // Allocate result buffers
    CUDA_CHECK_ERR(cudaMalloc(&res->d_distances, n * sizeof(float)));
    CUDA_CHECK_ERR(cudaMalloc(&res->d_predecessors, n * sizeof(int)));
    CUDA_CHECK_ERR(cudaMalloc(&res->d_visited, n * sizeof(int)));

    // Run BFS
    nimcp_error_t err = nimcp_gpu_graph_bfs(graph, source, res->d_distances);
    if (err != NIMCP_SUCCESS) {
        nimcp_gpu_graph_bfs_result_destroy(res);
        return err;
    }

    *result = res;
    return NIMCP_SUCCESS;
}

void nimcp_gpu_graph_bfs_result_destroy(nimcp_graph_bfs_result_t* result)
{
    if (!result) return;

    if (result->d_distances) cudaFree(result->d_distances);
    if (result->d_predecessors) cudaFree(result->d_predecessors);
    if (result->d_visited) cudaFree(result->d_visited);

    nimcp_free(result);
}

//=============================================================================
// Clustering Coefficient Implementation
//=============================================================================

nimcp_error_t nimcp_gpu_graph_clustering_coeff(
    nimcp_gpu_graph_t* graph,
    float* coefficients)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !coefficients) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    int n = (int)graph->num_vertices;

    // Compute degrees if not cached
    int* d_degrees = NULL;
    CUDA_CHECK_ERR(cudaMalloc(&d_degrees, n * sizeof(int)));

    kernel_compute_degrees<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        graph->d_row_offsets, d_degrees, n);

    // Count triangles
    int* d_triangles = NULL;
    CUDA_CHECK_ERR(cudaMalloc(&d_triangles, n * sizeof(int)));

    kernel_count_triangles<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        graph->d_row_offsets, graph->d_col_indices, d_triangles, n);

    // Compute clustering coefficients
    kernel_clustering_coefficient<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        d_degrees, d_triangles, coefficients, n);

    cudaDeviceSynchronize();

    cudaFree(d_degrees);
    cudaFree(d_triangles);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gpu_graph_avg_clustering(
    nimcp_gpu_graph_t* graph,
    float* avg_clustering)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !avg_clustering) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    int n = (int)graph->num_vertices;

    float* d_coefficients = NULL;
    CUDA_CHECK_ERR(cudaMalloc(&d_coefficients, n * sizeof(float)));

    nimcp_error_t err = nimcp_gpu_graph_clustering_coeff(graph, d_coefficients);
    if (err != NIMCP_SUCCESS) {
        cudaFree(d_coefficients);
        return err;
    }

    // Sum and average (simplified - should use reduction kernel)
    float* h_coefficients = (float*)nimcp_malloc(n * sizeof(float));
    cudaMemcpy(h_coefficients, d_coefficients, n * sizeof(float), cudaMemcpyDeviceToHost);

    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        sum += h_coefficients[i];
    }
    *avg_clustering = sum / (float)n;

    nimcp_free(h_coefficients);
    cudaFree(d_coefficients);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Centrality Implementation
//=============================================================================

nimcp_error_t nimcp_gpu_graph_degree_centrality(
    nimcp_gpu_graph_t* graph,
    float* centrality)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !centrality) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    int n = (int)graph->num_vertices;

    // Compute degrees
    int* d_degrees = NULL;
    CUDA_CHECK_ERR(cudaMalloc(&d_degrees, n * sizeof(int)));

    kernel_compute_degrees<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        graph->d_row_offsets, d_degrees, n);

    // Compute centrality
    kernel_degree_centrality<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        d_degrees, centrality, n);

    cudaDeviceSynchronize();
    cudaFree(d_degrees);

    return NIMCP_SUCCESS;
}

size_t nimcp_gpu_graph_find_hubs(
    nimcp_gpu_graph_t* graph,
    float threshold,
    int* hub_ids,
    size_t max_hubs)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !hub_ids || max_hubs == 0) {
        return 0;
    }

    int n = (int)graph->num_vertices;

    // Compute centrality
    float* d_centrality = NULL;
    cudaMalloc(&d_centrality, n * sizeof(float));

    if (nimcp_gpu_graph_degree_centrality(graph, d_centrality) != NIMCP_SUCCESS) {
        cudaFree(d_centrality);
        return 0;
    }

    // Copy to host and find hubs
    float* h_centrality = (float*)nimcp_malloc(n * sizeof(float));
    cudaMemcpy(h_centrality, d_centrality, n * sizeof(float), cudaMemcpyDeviceToHost);

    size_t count = 0;
    for (int i = 0; i < n && count < max_hubs; i++) {
        if (h_centrality[i] >= threshold) {
            hub_ids[count++] = i;
        }
    }

    nimcp_free(h_centrality);
    cudaFree(d_centrality);

    return count;
}

//=============================================================================
// Small-World Metrics Implementation
//=============================================================================

float nimcp_gpu_graph_small_world_coeff(nimcp_gpu_graph_t* graph)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph) {
        return 0.0f;
    }

    nimcp_graph_small_world_t metrics;
    if (nimcp_gpu_graph_small_world_metrics(graph, &metrics) != NIMCP_SUCCESS) {
        return 0.0f;
    }

    return metrics.small_world_sigma;
}

nimcp_error_t nimcp_gpu_graph_small_world_metrics(
    nimcp_gpu_graph_t* graph,
    nimcp_graph_small_world_t* metrics)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !metrics) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Compute average clustering
    nimcp_error_t err = nimcp_gpu_graph_avg_clustering(graph, &metrics->avg_clustering);
    if (err != NIMCP_SUCCESS) {
        return err;
    }

    // Compute average path length (sampled)
    err = nimcp_gpu_graph_avg_path_length(graph, 100, &metrics->avg_path_length);
    if (err != NIMCP_SUCCESS) {
        return err;
    }

    // Expected values for random graph
    int n = (int)graph->num_vertices;
    int m = (int)graph->num_edges;
    float p = (float)(2 * m) / (float)(n * (n - 1));  // Edge probability

    float C_rand = p;  // Expected clustering for random graph
    float L_rand = logf((float)n) / logf((float)(n * p));  // Expected path length

    // Small-world coefficients
    if (C_rand > 0.0f && L_rand > 0.0f) {
        metrics->small_world_sigma = (metrics->avg_clustering / C_rand) /
                                     (metrics->avg_path_length / L_rand);
        metrics->small_world_omega = (L_rand / metrics->avg_path_length) -
                                     (metrics->avg_clustering / C_rand);
    } else {
        metrics->small_world_sigma = 0.0f;
        metrics->small_world_omega = 0.0f;
    }

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gpu_graph_avg_path_length(
    nimcp_gpu_graph_t* graph,
    size_t num_samples,
    float* avg_path_length)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !avg_path_length) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    int n = (int)graph->num_vertices;
    if (num_samples == 0 || num_samples > (size_t)n) {
        num_samples = n;
    }

    float* d_distances = NULL;
    CUDA_CHECK_ERR(cudaMalloc(&d_distances, n * sizeof(float)));

    float total_distance = 0.0f;
    int total_pairs = 0;

    // Sample source vertices
    for (size_t s = 0; s < num_samples; s++) {
        int source = (int)(s * n / num_samples);

        nimcp_error_t err = nimcp_gpu_graph_bfs(graph, source, d_distances);
        if (err != NIMCP_SUCCESS) {
            cudaFree(d_distances);
            return err;
        }

        // Sum distances on host (should use reduction kernel)
        float* h_distances = (float*)nimcp_malloc(n * sizeof(float));
        cudaMemcpy(h_distances, d_distances, n * sizeof(float), cudaMemcpyDeviceToHost);

        for (int i = 0; i < n; i++) {
            if (h_distances[i] > 0.0f && h_distances[i] < INF_DISTANCE) {
                total_distance += h_distances[i];
                total_pairs++;
            }
        }

        nimcp_free(h_distances);
    }

    cudaFree(d_distances);

    if (total_pairs > 0) {
        *avg_path_length = total_distance / (float)total_pairs;
    } else {
        *avg_path_length = 0.0f;
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Modularity Implementation
//=============================================================================

nimcp_error_t nimcp_gpu_graph_modularity(
    nimcp_gpu_graph_t* graph,
    const int* community_labels,
    float* modularity)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !community_labels || !modularity) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    int n = (int)graph->num_vertices;

    // Copy community labels to device
    int* d_labels = NULL;
    CUDA_CHECK_ERR(cudaMalloc(&d_labels, n * sizeof(int)));
    CUDA_CHECK_ERR(cudaMemcpy(d_labels, community_labels, n * sizeof(int), cudaMemcpyHostToDevice));

    // Compute degrees
    int* d_degrees = NULL;
    CUDA_CHECK_ERR(cudaMalloc(&d_degrees, n * sizeof(int)));
    kernel_compute_degrees<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        graph->d_row_offsets, d_degrees, n);

    // Compute total edge weight
    float total_weight = (float)graph->num_edges;

    // Compute modularity contributions
    float* d_contrib = NULL;
    CUDA_CHECK_ERR(cudaMalloc(&d_contrib, n * sizeof(float)));

    kernel_modularity<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        graph->d_row_offsets,
        graph->d_col_indices,
        graph->d_edge_weights,
        d_degrees,
        d_labels,
        d_contrib,
        total_weight,
        n
    );
    cudaDeviceSynchronize();

    // Sum contributions (should use reduction kernel)
    float* h_contrib = (float*)nimcp_malloc(n * sizeof(float));
    cudaMemcpy(h_contrib, d_contrib, n * sizeof(float), cudaMemcpyDeviceToHost);

    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        sum += h_contrib[i];
    }
    *modularity = sum / (2.0f * total_weight);

    nimcp_free(h_contrib);
    cudaFree(d_labels);
    cudaFree(d_degrees);
    cudaFree(d_contrib);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Subgraph Matching Implementation
//=============================================================================

nimcp_error_t nimcp_gpu_graph_subgraph_match(
    nimcp_gpu_graph_t* target,
    nimcp_gpu_graph_t* pattern,
    size_t max_matches,
    nimcp_graph_match_result_t** result)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!target || !pattern || !result) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    nimcp_graph_match_result_t* res = (nimcp_graph_match_result_t*)nimcp_calloc(
        1, sizeof(nimcp_graph_match_result_t));
    if (!res) {
        return NIMCP_ERROR_NO_MEMORY;
    }

    res->max_matches = max_matches;
    res->pattern_size = pattern->num_vertices;
    res->num_matches = 0;

    // Allocate result buffers
    size_t mapping_size = max_matches * pattern->num_vertices * sizeof(int);
    CUDA_CHECK_ERR(cudaMalloc(&res->d_vertex_mappings, mapping_size));
    CUDA_CHECK_ERR(cudaMalloc(&res->d_scores, max_matches * sizeof(float)));

    // Simple degree-based filtering (actual matching would be more complex)
    // This is a placeholder - real implementation would use VF2 or similar algorithm

    *result = res;
    return NIMCP_SUCCESS;
}

void nimcp_gpu_graph_match_result_destroy(nimcp_graph_match_result_t* result)
{
    if (!result) return;

    if (result->d_vertex_mappings) cudaFree(result->d_vertex_mappings);
    if (result->d_scores) cudaFree(result->d_scores);

    nimcp_free(result);
}

//=============================================================================
// Graph Statistics
//=============================================================================

nimcp_error_t nimcp_gpu_graph_compute_degrees(
    nimcp_gpu_graph_t* graph,
    int* degrees)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !degrees) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    int n = (int)graph->num_vertices;
    kernel_compute_degrees<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        graph->d_row_offsets, degrees, n);
    cudaDeviceSynchronize();

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gpu_graph_stats(
    nimcp_gpu_graph_t* graph,
    float* avg_degree,
    int* max_degree,
    int* min_degree,
    float* density)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    int n = (int)graph->num_vertices;

    // Compute degrees
    int* d_degrees = NULL;
    CUDA_CHECK_ERR(cudaMalloc(&d_degrees, n * sizeof(int)));
    kernel_compute_degrees<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        graph->d_row_offsets, d_degrees, n);
    cudaDeviceSynchronize();

    // Copy to host and compute stats
    int* h_degrees = (int*)nimcp_malloc(n * sizeof(int));
    cudaMemcpy(h_degrees, d_degrees, n * sizeof(int), cudaMemcpyDeviceToHost);

    int sum = 0;
    int max_d = 0;
    int min_d = n;

    for (int i = 0; i < n; i++) {
        sum += h_degrees[i];
        if (h_degrees[i] > max_d) max_d = h_degrees[i];
        if (h_degrees[i] < min_d) min_d = h_degrees[i];
    }

    if (avg_degree) *avg_degree = (float)sum / (float)n;
    if (max_degree) *max_degree = max_d;
    if (min_degree) *min_degree = min_d;
    // Density: Graph stores directed edges (both u->v and v->u for undirected)
    // so num_edges is already 2x the undirected edge count
    if (density) *density = (float)graph->num_edges / (float)(n * (n - 1));

    nimcp_free(h_degrees);
    cudaFree(d_degrees);

    return NIMCP_SUCCESS;
}

nimcp_error_t nimcp_gpu_graph_to_host(
    const nimcp_gpu_graph_t* graph,
    int* row_offsets,
    int* col_indices,
    float* weights)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph) {
        return NIMCP_ERROR_NULL_POINTER;
    }

    if (row_offsets) {
        CUDA_CHECK_ERR(cudaMemcpy(row_offsets, graph->d_row_offsets,
                                  (graph->num_vertices + 1) * sizeof(int),
                                  cudaMemcpyDeviceToHost));
    }

    if (col_indices) {
        CUDA_CHECK_ERR(cudaMemcpy(col_indices, graph->d_col_indices,
                                  graph->num_edges * sizeof(int),
                                  cudaMemcpyDeviceToHost));
    }

    if (weights && graph->d_edge_weights) {
        CUDA_CHECK_ERR(cudaMemcpy(weights, graph->d_edge_weights,
                                  graph->num_edges * sizeof(float),
                                  cudaMemcpyDeviceToHost));
    }

    return NIMCP_SUCCESS;
}

bool nimcp_gpu_graph_is_valid(const nimcp_gpu_graph_t* graph)
{
    if (!graph) return false;
    if (!graph->ctx) return false;
    if (!graph->d_row_offsets) return false;
    if (!graph->d_col_indices) return false;
    if (graph->num_vertices == 0) return false;

    return true;
}

} // extern "C"

#else // !NIMCP_ENABLE_CUDA

//=============================================================================
// Stub implementations when CUDA is not available
//=============================================================================

#include "gpu/graph/nimcp_graph_gpu.h"
#include <stdlib.h>

extern "C" {

nimcp_gpu_graph_t* nimcp_gpu_graph_create(
    nimcp_gpu_context_t* ctx, size_t num_vertices, size_t num_edges)
{
    (void)ctx; (void)num_vertices; (void)num_edges;
    return NULL;
}

void nimcp_gpu_graph_destroy(nimcp_gpu_graph_t* graph)
{
    (void)graph;
}

nimcp_gpu_graph_t* nimcp_gpu_graph_from_adjacency(
    nimcp_gpu_context_t* ctx, const float* adjacency, size_t n, float threshold)
{
    (void)ctx; (void)adjacency; (void)n; (void)threshold;
    return NULL;
}

nimcp_gpu_graph_t* nimcp_gpu_graph_from_edge_list(
    nimcp_gpu_context_t* ctx, const int* src, const int* dst, const float* weights,
    size_t num_edges, size_t num_vertices)
{
    (void)ctx; (void)src; (void)dst; (void)weights; (void)num_edges; (void)num_vertices;
    return NULL;
}

nimcp_gpu_graph_t* nimcp_gpu_graph_from_csr(
    nimcp_gpu_context_t* ctx, const int* row_offsets, const int* col_indices,
    const float* weights, size_t num_vertices, size_t num_edges)
{
    (void)ctx; (void)row_offsets; (void)col_indices; (void)weights;
    (void)num_vertices; (void)num_edges;
    return NULL;
}

nimcp_error_t nimcp_gpu_graph_bfs(nimcp_gpu_graph_t* graph, int source, float* distances)
{
    (void)graph; (void)source; (void)distances;
    return NIMCP_ERROR_NOT_IMPLEMENTED;
}

nimcp_error_t nimcp_gpu_graph_clustering_coeff(nimcp_gpu_graph_t* graph, float* coefficients)
{
    (void)graph; (void)coefficients;
    return NIMCP_ERROR_NOT_IMPLEMENTED;
}

nimcp_error_t nimcp_gpu_graph_degree_centrality(nimcp_gpu_graph_t* graph, float* centrality)
{
    (void)graph; (void)centrality;
    return NIMCP_ERROR_NOT_IMPLEMENTED;
}

size_t nimcp_gpu_graph_find_hubs(
    nimcp_gpu_graph_t* graph, float threshold, int* hub_ids, size_t max_hubs)
{
    (void)graph; (void)threshold; (void)hub_ids; (void)max_hubs;
    return 0;
}

float nimcp_gpu_graph_small_world_coeff(nimcp_gpu_graph_t* graph)
{
    (void)graph;
    return 0.0f;
}

nimcp_error_t nimcp_gpu_graph_modularity(
    nimcp_gpu_graph_t* graph, const int* community_labels, float* modularity)
{
    (void)graph; (void)community_labels; (void)modularity;
    return NIMCP_ERROR_NOT_IMPLEMENTED;
}

bool nimcp_gpu_graph_is_valid(const nimcp_gpu_graph_t* graph)
{
    (void)graph;
    return false;
}

} // extern "C"

#endif // NIMCP_ENABLE_CUDA
