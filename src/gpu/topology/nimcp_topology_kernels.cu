/**
 * @file nimcp_topology_kernels.cu
 * @brief GPU Topology and Community Detection CUDA Kernels Implementation
 *
 * WHAT: CUDA kernels for graph topology analysis and community detection
 * WHY:  GPU acceleration for large-scale neural network topology operations
 * HOW:  Custom kernels for Louvain, graph metrics, shortest paths, generation
 *
 * RNG ARCHITECTURE:
 * =================
 * Uses kernel_init_random for stochastic graph generation algorithms.
 * The pattern follows the central GPU statistics module:
 *   - See: gpu/statistics/nimcp_statistics_gpu.h for stats_gpu_rng_create/destroy
 *   - See: gpu/common/nimcp_device_utils.cuh for shared device RNG functions
 *
 * Local RNG state is used for:
 *   1. Random graph generation (Erdos-Renyi, Barabasi-Albert, etc.)
 *   2. Stochastic community detection algorithms
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <curand.h>
#include <curand_kernel.h>
#include <math.h>
#include <float.h>
#include <algorithm>
#include <thrust/device_ptr.h>
#include <thrust/sort.h>
#include <thrust/scan.h>
#include <thrust/reduce.h>
#include <thrust/count.h>
#include <thrust/sequence.h>
#include <thrust/execution_policy.h>
#include <thrust/extrema.h>
#include <thrust/fill.h>

#include "gpu/topology/nimcp_topology_gpu.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"

#define LOG_MODULE "TOPOLOGY_GPU"

//=============================================================================
// Kernel Configuration
//=============================================================================

#define BLOCK_SIZE 256
#define WARP_SIZE 32
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)
#define INF_DISTANCE 1e30f

//=============================================================================
// Basic Kernels
//=============================================================================

/**
 * @brief Initialize random states for cuRAND
 */
__global__ void kernel_init_random(curandState* states, uint64_t seed, int n)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        curand_init(seed, idx, 0, &states[idx]);
    }
}

/**
 * @brief Compute node degrees from adjacency matrix
 */
__global__ void kernel_compute_degree_dense(
    const float* adjacency,
    int* degree,
    int num_nodes,
    float threshold)
{
    int node = blockIdx.x * blockDim.x + threadIdx.x;
    if (node >= num_nodes) return;

    int deg = 0;
    for (int j = 0; j < num_nodes; j++) {
        float val = adjacency[node * num_nodes + j];
        if (fabsf(val) > threshold && node != j) {
            deg++;
        }
    }
    degree[node] = deg;
}

/**
 * @brief Compute node degrees from CSR format
 */
__global__ void kernel_compute_degree_csr(
    const int* row_ptrs,
    int* degree,
    int num_nodes)
{
    int node = blockIdx.x * blockDim.x + threadIdx.x;
    if (node >= num_nodes) return;

    degree[node] = row_ptrs[node + 1] - row_ptrs[node];
}

/**
 * @brief Compute weighted degrees from CSR format
 */
__global__ void kernel_compute_weighted_degree_csr(
    const int* row_ptrs,
    const float* weights,
    float* weighted_degree,
    int num_nodes)
{
    int node = blockIdx.x * blockDim.x + threadIdx.x;
    if (node >= num_nodes) return;

    float sum = 0.0f;
    int start = row_ptrs[node];
    int end = row_ptrs[node + 1];
    for (int j = start; j < end; j++) {
        sum += fabsf(weights[j]);
    }
    weighted_degree[node] = sum;
}

//=============================================================================
// Clustering Coefficient Kernels
//=============================================================================

/**
 * @brief Count triangles for each node (CSR format)
 */
__global__ void kernel_count_triangles_csr(
    const int* row_ptrs,
    const int* col_indices,
    int* triangle_count,
    int num_nodes)
{
    int node = blockIdx.x * blockDim.x + threadIdx.x;
    if (node >= num_nodes) return;

    int count = 0;
    int start_u = row_ptrs[node];
    int end_u = row_ptrs[node + 1];

    // For each neighbor v of u
    for (int i = start_u; i < end_u; i++) {
        int v = col_indices[i];
        if (v <= node) continue;  // Avoid counting twice

        int start_v = row_ptrs[v];
        int end_v = row_ptrs[v + 1];

        // Intersect neighbor lists of u and v
        int j = start_u;
        int k = start_v;
        while (j < end_u && k < end_v) {
            int nu = col_indices[j];
            int nv = col_indices[k];
            if (nu == nv && nu > v) {
                count++;
                j++;
                k++;
            } else if (nu < nv) {
                j++;
            } else {
                k++;
            }
        }
    }
    triangle_count[node] = count;
}

/**
 * @brief Compute local clustering coefficients
 */
__global__ void kernel_clustering_coefficient(
    const int* degree,
    const int* triangle_count,
    float* clustering,
    int num_nodes)
{
    int node = blockIdx.x * blockDim.x + threadIdx.x;
    if (node >= num_nodes) return;

    int deg = degree[node];
    if (deg < 2) {
        clustering[node] = 0.0f;
    } else {
        // Clustering = 2 * triangles / (degree * (degree - 1))
        float max_triangles = (float)(deg * (deg - 1)) / 2.0f;
        clustering[node] = (float)triangle_count[node] / max_triangles;
    }
}

//=============================================================================
// PageRank Kernels
//=============================================================================

/**
 * @brief PageRank iteration (CSR format)
 */
__global__ void kernel_pagerank_iteration_csr(
    const int* row_ptrs,
    const int* col_indices,
    const float* current_rank,
    const int* out_degree,
    float* new_rank,
    float damping,
    int num_nodes)
{
    int node = blockIdx.x * blockDim.x + threadIdx.x;
    if (node >= num_nodes) return;

    float sum = 0.0f;
    int start = row_ptrs[node];
    int end = row_ptrs[node + 1];

    for (int j = start; j < end; j++) {
        int neighbor = col_indices[j];
        if (out_degree[neighbor] > 0) {
            sum += current_rank[neighbor] / (float)out_degree[neighbor];
        }
    }

    new_rank[node] = (1.0f - damping) / (float)num_nodes + damping * sum;
}

/**
 * @brief Compute PageRank convergence (L1 difference)
 */
__global__ void kernel_pagerank_diff(
    const float* old_rank,
    const float* new_rank,
    float* diff,
    int num_nodes)
{
    extern __shared__ float sdata[];

    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int tid = threadIdx.x;

    float val = 0.0f;
    if (idx < num_nodes) {
        val = fabsf(new_rank[idx] - old_rank[idx]);
    }
    sdata[tid] = val;
    __syncthreads();

    // Reduction in shared memory
    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] += sdata[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        atomicAdd(diff, sdata[0]);
    }
}

//=============================================================================
// BFS Shortest Path Kernels
//=============================================================================

/**
 * @brief BFS frontier expansion (CSR format)
 */
__global__ void kernel_bfs_expand_csr(
    const int* row_ptrs,
    const int* col_indices,
    const int* frontier,
    int* next_frontier,
    int* frontier_size,
    float* distances,
    int* predecessors,
    float current_dist,
    int num_frontier)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_frontier) return;

    int node = frontier[idx];
    int start = row_ptrs[node];
    int end = row_ptrs[node + 1];

    for (int j = start; j < end; j++) {
        int neighbor = col_indices[j];
        float old_dist = atomicCAS((int*)&distances[neighbor],
                                    __float_as_int(INF_DISTANCE),
                                    __float_as_int(current_dist + 1.0f));
        if (old_dist == __float_as_int(INF_DISTANCE)) {
            // First time visiting this node
            int pos = atomicAdd(frontier_size, 1);
            next_frontier[pos] = neighbor;
            if (predecessors) {
                predecessors[neighbor] = node;
            }
        }
    }
}

/**
 * @brief Initialize BFS distances
 */
__global__ void kernel_bfs_init(
    float* distances,
    int* predecessors,
    int source,
    int num_nodes)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_nodes) return;

    if (idx == source) {
        distances[idx] = 0.0f;
        if (predecessors) predecessors[idx] = -1;
    } else {
        distances[idx] = INF_DISTANCE;
        if (predecessors) predecessors[idx] = -1;
    }
}

//=============================================================================
// Dijkstra Shortest Path Kernels
//=============================================================================

/**
 * @brief Dijkstra relaxation step (CSR format)
 */
__global__ void kernel_dijkstra_relax_csr(
    const int* row_ptrs,
    const int* col_indices,
    const float* edge_weights,
    float* distances,
    int* predecessors,
    int* updated,
    int num_nodes)
{
    int node = blockIdx.x * blockDim.x + threadIdx.x;
    if (node >= num_nodes) return;

    float my_dist = distances[node];
    if (my_dist >= INF_DISTANCE) return;

    int start = row_ptrs[node];
    int end = row_ptrs[node + 1];

    for (int j = start; j < end; j++) {
        int neighbor = col_indices[j];
        float weight = edge_weights ? edge_weights[j] : 1.0f;
        float new_dist = my_dist + weight;

        // Atomic min
        float old_dist;
        do {
            old_dist = distances[neighbor];
            if (new_dist >= old_dist) break;
        } while (atomicCAS((int*)&distances[neighbor],
                          __float_as_int(old_dist),
                          __float_as_int(new_dist)) != __float_as_int(old_dist));

        if (new_dist < old_dist) {
            *updated = 1;
            if (predecessors) {
                predecessors[neighbor] = node;
            }
        }
    }
}

//=============================================================================
// Floyd-Warshall APSP Kernel
//=============================================================================

/**
 * @brief Floyd-Warshall iteration for block
 */
__global__ void kernel_floyd_warshall_iteration(
    float* dist,
    int k,
    int num_nodes)
{
    int i = blockIdx.y * blockDim.y + threadIdx.y;
    int j = blockIdx.x * blockDim.x + threadIdx.x;

    if (i >= num_nodes || j >= num_nodes) return;

    float dik = dist[i * num_nodes + k];
    float dkj = dist[k * num_nodes + j];

    if (dik < INF_DISTANCE && dkj < INF_DISTANCE) {
        float new_dist = dik + dkj;
        if (new_dist < dist[i * num_nodes + j]) {
            dist[i * num_nodes + j] = new_dist;
        }
    }
}

/**
 * @brief Initialize Floyd-Warshall distance matrix from adjacency
 */
__global__ void kernel_floyd_warshall_init(
    const float* adjacency,
    float* dist,
    int num_nodes)
{
    int i = blockIdx.y * blockDim.y + threadIdx.y;
    int j = blockIdx.x * blockDim.x + threadIdx.x;

    if (i >= num_nodes || j >= num_nodes) return;

    if (i == j) {
        dist[i * num_nodes + j] = 0.0f;
    } else {
        float adj = adjacency[i * num_nodes + j];
        dist[i * num_nodes + j] = (adj > 0.0f) ? adj : INF_DISTANCE;
    }
}

//=============================================================================
// Betweenness Centrality Kernels
//=============================================================================

/**
 * @brief Initialize betweenness computation for source
 */
__global__ void kernel_betweenness_init(
    float* sigma,
    float* delta,
    int* dist,
    int source,
    int num_nodes)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_nodes) return;

    if (idx == source) {
        sigma[idx] = 1.0f;
        dist[idx] = 0;
    } else {
        sigma[idx] = 0.0f;
        dist[idx] = -1;
    }
    delta[idx] = 0.0f;
}

/**
 * @brief Betweenness BFS forward pass
 */
__global__ void kernel_betweenness_forward_csr(
    const int* row_ptrs,
    const int* col_indices,
    float* sigma,
    int* dist,
    int current_dist,
    int* changed,
    int num_nodes)
{
    int node = blockIdx.x * blockDim.x + threadIdx.x;
    if (node >= num_nodes) return;

    if (dist[node] != current_dist) return;

    int start = row_ptrs[node];
    int end = row_ptrs[node + 1];

    for (int j = start; j < end; j++) {
        int neighbor = col_indices[j];

        int old_dist = atomicCAS(&dist[neighbor], -1, current_dist + 1);
        if (old_dist == -1 || old_dist == current_dist + 1) {
            if (old_dist == -1) *changed = 1;
            atomicAdd(&sigma[neighbor], sigma[node]);
        }
    }
}

/**
 * @brief Betweenness backward accumulation
 */
__global__ void kernel_betweenness_backward_csr(
    const int* row_ptrs,
    const int* col_indices,
    const float* sigma,
    float* delta,
    const int* dist,
    int current_dist,
    int num_nodes)
{
    int node = blockIdx.x * blockDim.x + threadIdx.x;
    if (node >= num_nodes) return;

    if (dist[node] != current_dist) return;

    int start = row_ptrs[node];
    int end = row_ptrs[node + 1];

    float coeff = 0.0f;
    for (int j = start; j < end; j++) {
        int neighbor = col_indices[j];
        if (dist[neighbor] == current_dist + 1) {
            coeff += (sigma[node] / sigma[neighbor]) * (1.0f + delta[neighbor]);
        }
    }
    delta[node] = coeff;
}

//=============================================================================
// Community Detection - Louvain Algorithm Kernels
//=============================================================================

/**
 * @brief Initialize communities (each node is its own community)
 */
__global__ void kernel_init_communities(
    int* communities,
    float* community_weights,
    const int* degree,
    int num_nodes)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_nodes) return;

    communities[idx] = idx;
    community_weights[idx] = (float)degree[idx];
}

/**
 * @brief Compute modularity gain for moving node to neighbor's community
 */
__device__ float compute_modularity_gain(
    int node,
    int new_community,
    int old_community,
    const int* row_ptrs,
    const int* col_indices,
    const float* weights,
    const int* communities,
    const float* community_weights,
    float total_weight,
    float resolution)
{
    if (new_community == old_community) return 0.0f;

    // Sum of edges from node to new_community
    float ki_in = 0.0f;
    float ki_out = 0.0f;
    int start = row_ptrs[node];
    int end = row_ptrs[node + 1];

    for (int j = start; j < end; j++) {
        int neighbor = col_indices[j];
        float w = weights ? weights[j] : 1.0f;
        if (communities[neighbor] == new_community) {
            ki_in += w;
        }
        if (communities[neighbor] == old_community) {
            ki_out += w;
        }
    }

    // Node's total weight
    float ki = 0.0f;
    for (int j = start; j < end; j++) {
        ki += weights ? weights[j] : 1.0f;
    }

    float sigma_tot_new = community_weights[new_community];
    float sigma_tot_old = community_weights[old_community] - ki;

    // Modularity gain formula
    float gain = (ki_in - ki_out) / total_weight;
    gain -= resolution * ki * (sigma_tot_new - sigma_tot_old) / (2.0f * total_weight * total_weight);

    return gain;
}

/**
 * @brief Louvain local moving phase
 */
__global__ void kernel_louvain_local_move(
    const int* row_ptrs,
    const int* col_indices,
    const float* weights,
    int* communities,
    float* community_weights,
    const int* degree,
    float total_weight,
    float resolution,
    int* changed,
    int num_nodes)
{
    int node = blockIdx.x * blockDim.x + threadIdx.x;
    if (node >= num_nodes) return;

    int old_community = communities[node];
    int best_community = old_community;
    float best_gain = 0.0f;

    // Check each neighbor's community
    int start = row_ptrs[node];
    int end = row_ptrs[node + 1];

    for (int j = start; j < end; j++) {
        int neighbor = col_indices[j];
        int neighbor_community = communities[neighbor];

        if (neighbor_community != old_community) {
            float gain = compute_modularity_gain(
                node, neighbor_community, old_community,
                row_ptrs, col_indices, weights, communities,
                community_weights, total_weight, resolution);

            if (gain > best_gain) {
                best_gain = gain;
                best_community = neighbor_community;
            }
        }
    }

    if (best_community != old_community) {
        // Move node to best community
        communities[node] = best_community;
        *changed = 1;

        // Update community weights
        float ki = 0.0f;
        for (int j = start; j < end; j++) {
            ki += weights ? weights[j] : 1.0f;
        }
        atomicAdd(&community_weights[old_community], -ki);
        atomicAdd(&community_weights[best_community], ki);
    }
}

/**
 * @brief Count community sizes
 */
__global__ void kernel_count_community_sizes(
    const int* communities,
    int* community_sizes,
    int num_nodes,
    int max_communities)
{
    // First zero out sizes
    for (int i = threadIdx.x; i < max_communities; i += blockDim.x) {
        community_sizes[i] = 0;
    }
    __syncthreads();

    for (int i = blockIdx.x * blockDim.x + threadIdx.x; i < num_nodes; i += blockDim.x * gridDim.x) {
        int c = communities[i];
        atomicAdd(&community_sizes[c], 1);
    }
}

//=============================================================================
// Label Propagation Community Detection Kernels
//=============================================================================

/**
 * @brief Label propagation iteration
 */
__global__ void kernel_label_propagation_csr(
    const int* row_ptrs,
    const int* col_indices,
    const float* weights,
    int* labels,
    int* changed,
    int num_nodes)
{
    int node = blockIdx.x * blockDim.x + threadIdx.x;
    if (node >= num_nodes) return;

    // Count neighbor labels
    int start = row_ptrs[node];
    int end = row_ptrs[node + 1];
    int deg = end - start;

    if (deg == 0) return;

    // Find most common label among neighbors (weighted)
    // Using simple approach for smaller neighbor sets
    int best_label = labels[node];
    float best_weight = 0.0f;

    // For each neighbor, count its label weight
    for (int j = start; j < end; j++) {
        int neighbor = col_indices[j];
        int neighbor_label = labels[neighbor];
        float w = weights ? weights[j] : 1.0f;

        float total_weight = w;
        for (int k = j + 1; k < end; k++) {
            if (labels[col_indices[k]] == neighbor_label) {
                total_weight += weights ? weights[k] : 1.0f;
            }
        }

        if (total_weight > best_weight) {
            best_weight = total_weight;
            best_label = neighbor_label;
        }
    }

    if (best_label != labels[node]) {
        labels[node] = best_label;
        *changed = 1;
    }
}

//=============================================================================
// Modularity Computation Kernels
//=============================================================================

/**
 * @brief Compute modularity contribution per edge
 */
__global__ void kernel_modularity_contribution_csr(
    const int* row_ptrs,
    const int* col_indices,
    const float* weights,
    const int* communities,
    const float* degrees,
    float* contribution,
    float total_weight,
    float resolution,
    int num_nodes)
{
    extern __shared__ float sdata[];

    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int tid = threadIdx.x;

    float local_contrib = 0.0f;

    if (idx < num_nodes) {
        int c_i = communities[idx];
        int start = row_ptrs[idx];
        int end = row_ptrs[idx + 1];
        float k_i = degrees[idx];

        for (int j = start; j < end; j++) {
            int neighbor = col_indices[j];
            int c_j = communities[neighbor];
            float w = weights ? weights[j] : 1.0f;
            float k_j = degrees[neighbor];

            if (c_i == c_j) {
                local_contrib += w - resolution * k_i * k_j / (2.0f * total_weight);
            }
        }
    }

    sdata[tid] = local_contrib;
    __syncthreads();

    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] += sdata[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        atomicAdd(contribution, sdata[0]);
    }
}

//=============================================================================
// Network Generation Kernels
//=============================================================================

/**
 * @brief Generate Erdos-Renyi random graph
 */
__global__ void kernel_erdos_renyi_generate(
    float* adjacency,
    curandState* states,
    float edge_prob,
    int num_nodes)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int i = idx / num_nodes;
    int j = idx % num_nodes;

    if (i >= num_nodes || j >= num_nodes) return;

    if (i < j) {  // Only upper triangle
        float r = curand_uniform(&states[idx % (BLOCK_SIZE * GRID_SIZE(num_nodes))]);
        float edge = (r < edge_prob) ? 1.0f : 0.0f;
        adjacency[i * num_nodes + j] = edge;
        adjacency[j * num_nodes + i] = edge;  // Symmetric
    } else if (i == j) {
        adjacency[i * num_nodes + j] = 0.0f;  // No self-loops
    }
}

/**
 * @brief Generate ring lattice for Watts-Strogatz
 */
__global__ void kernel_ring_lattice(
    float* adjacency,
    int num_nodes,
    int k)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= num_nodes) return;

    // Clear row first
    for (int j = 0; j < num_nodes; j++) {
        adjacency[i * num_nodes + j] = 0.0f;
    }

    // Connect to k/2 neighbors on each side
    int half_k = k / 2;
    for (int offset = 1; offset <= half_k; offset++) {
        int left = (i - offset + num_nodes) % num_nodes;
        int right = (i + offset) % num_nodes;
        adjacency[i * num_nodes + left] = 1.0f;
        adjacency[i * num_nodes + right] = 1.0f;
    }
}

/**
 * @brief Rewire edges for Watts-Strogatz
 */
__global__ void kernel_watts_strogatz_rewire(
    float* adjacency,
    curandState* states,
    float rewire_prob,
    int num_nodes,
    int k)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= num_nodes) return;

    curandState local_state = states[i];
    int half_k = k / 2;

    for (int offset = 1; offset <= half_k; offset++) {
        int j = (i + offset) % num_nodes;

        if (curand_uniform(&local_state) < rewire_prob) {
            // Remove old edge
            adjacency[i * num_nodes + j] = 0.0f;
            adjacency[j * num_nodes + i] = 0.0f;

            // Add new random edge
            int attempts = 0;
            while (attempts < 100) {
                int new_target = (int)(curand_uniform(&local_state) * num_nodes);
                if (new_target != i && adjacency[i * num_nodes + new_target] == 0.0f) {
                    adjacency[i * num_nodes + new_target] = 1.0f;
                    adjacency[new_target * num_nodes + i] = 1.0f;
                    break;
                }
                attempts++;
            }
        }
    }

    states[i] = local_state;
}

/**
 * @brief Compute degree distribution for Barabasi-Albert preferential attachment
 */
__global__ void kernel_compute_degree_cumsum(
    const float* adjacency,
    float* cumsum,
    int num_nodes,
    int active_nodes)
{
    if (threadIdx.x != 0 || blockIdx.x != 0) return;

    float total = 0.0f;
    for (int i = 0; i < active_nodes; i++) {
        float deg = 0.0f;
        for (int j = 0; j < active_nodes; j++) {
            deg += adjacency[i * num_nodes + j];
        }
        total += deg + 1.0f;  // +1 to avoid 0 degree
        cumsum[i] = total;
    }
}

//=============================================================================
// Graph Utility Kernels
//=============================================================================

/**
 * @brief Convert dense adjacency to CSR row pointers
 */
__global__ void kernel_dense_to_csr_count(
    const float* adjacency,
    int* row_counts,
    float threshold,
    int num_nodes)
{
    int row = blockIdx.x * blockDim.x + threadIdx.x;
    if (row >= num_nodes) return;

    int count = 0;
    for (int col = 0; col < num_nodes; col++) {
        if (fabsf(adjacency[row * num_nodes + col]) > threshold) {
            count++;
        }
    }
    row_counts[row] = count;
}

/**
 * @brief Extract CSR column indices and values from dense adjacency
 */
__global__ void kernel_dense_to_csr_extract(
    const float* adjacency,
    const int* row_ptrs,
    int* col_indices,
    float* values,
    float threshold,
    int num_nodes)
{
    int row = blockIdx.x * blockDim.x + threadIdx.x;
    if (row >= num_nodes) return;

    int write_pos = row_ptrs[row];
    for (int col = 0; col < num_nodes; col++) {
        float val = adjacency[row * num_nodes + col];
        if (fabsf(val) > threshold) {
            col_indices[write_pos] = col;
            values[write_pos] = val;
            write_pos++;
        }
    }
}

/**
 * @brief Symmetrize adjacency matrix
 */
__global__ void kernel_symmetrize(
    float* adjacency,
    int num_nodes)
{
    int i = blockIdx.y * blockDim.y + threadIdx.y;
    int j = blockIdx.x * blockDim.x + threadIdx.x;

    if (i >= num_nodes || j >= num_nodes || i >= j) return;

    float avg = (adjacency[i * num_nodes + j] + adjacency[j * num_nodes + i]) / 2.0f;
    adjacency[i * num_nodes + j] = avg;
    adjacency[j * num_nodes + i] = avg;
}

/**
 * @brief Remove self-loops
 */
__global__ void kernel_remove_self_loops(
    float* adjacency,
    int num_nodes)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= num_nodes) return;

    adjacency[i * num_nodes + i] = 0.0f;
}

//=============================================================================
// API Implementation - Graph Lifecycle
//=============================================================================

nimcp_graph_gpu_t* nimcp_graph_gpu_create(
    nimcp_gpu_context_t* ctx,
    int num_nodes,
    bool sparse)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || num_nodes <= 0) {
        LOG_ERROR("Invalid parameters for graph creation");
        return NULL;
    }

    nimcp_graph_gpu_t* graph = (nimcp_graph_gpu_t*)nimcp_calloc(1, sizeof(nimcp_graph_gpu_t));
    if (!graph) {
        LOG_ERROR("Failed to allocate graph structure");
        return NULL;
    }

    graph->ctx = ctx;
    graph->num_nodes = num_nodes;
    graph->num_edges = 0;
    graph->is_sparse = sparse;

    if (!sparse) {
        // Allocate dense adjacency matrix
        int64_t dims[2] = {num_nodes, num_nodes};
        graph->adjacency = nimcp_gpu_tensor_create(ctx, 2, dims, NIMCP_DTYPE_FLOAT32);
        if (!graph->adjacency) {
            LOG_ERROR("Failed to allocate adjacency matrix");
            nimcp_free(graph);
            return NULL;
        }

        // Zero initialize
        cudaMemset(graph->adjacency->data, 0, num_nodes * num_nodes * sizeof(float));
    }

    LOG_DEBUG("Created GPU graph with %d nodes (sparse=%d)", num_nodes, sparse);
    return graph;
}

nimcp_graph_gpu_t* nimcp_graph_gpu_from_dense(
    nimcp_gpu_context_t* ctx,
    const float* adjacency,
    int num_nodes)
{
    if (!ctx || !adjacency || num_nodes <= 0) {
        return NULL;
    }

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_create(ctx, num_nodes, false);
    if (!graph) return NULL;

    // Copy adjacency to device
    size_t size = (size_t)num_nodes * num_nodes * sizeof(float);
    NIMCP_CUDA_CHECK_IMMUNE_NULL(cudaMemcpy(graph->adjacency->data, adjacency, size, cudaMemcpyHostToDevice));

    // Count edges
    int* d_degree;
    NIMCP_CUDA_CHECK_IMMUNE_NULL(cudaMalloc(&d_degree, num_nodes * sizeof(int)));

    kernel_compute_degree_dense<<<GRID_SIZE(num_nodes), BLOCK_SIZE>>>(
        (float*)graph->adjacency->data, d_degree, num_nodes, 0.0f);

    thrust::device_ptr<int> dev_ptr(d_degree);
    graph->num_edges = thrust::reduce(dev_ptr, dev_ptr + num_nodes) / 2;  // Undirected

    cudaFree(d_degree);

    return graph;
}

nimcp_graph_gpu_t* nimcp_graph_gpu_from_csr(
    nimcp_gpu_context_t* ctx,
    const int* row_ptrs,
    const int* col_indices,
    const float* weights,
    int num_nodes,
    int num_edges)
{
    if (!ctx || !row_ptrs || !col_indices || num_nodes <= 0) {
        return NULL;
    }

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_create(ctx, num_nodes, true);
    if (!graph) return NULL;

    graph->num_edges = num_edges;

    // Allocate and copy row pointers
    int64_t row_dims[1] = {num_nodes + 1};
    graph->row_ptrs = nimcp_gpu_tensor_create(ctx, 1, row_dims, NIMCP_DTYPE_INT32);
    if (!graph->row_ptrs) {
        nimcp_graph_gpu_destroy(graph);
        return NULL;
    }
    NIMCP_CUDA_CHECK_IMMUNE_NULL(cudaMemcpy(graph->row_ptrs->data, row_ptrs,
                              (num_nodes + 1) * sizeof(int), cudaMemcpyHostToDevice));

    // Allocate and copy column indices
    int64_t edge_dims[1] = {num_edges};
    graph->col_indices = nimcp_gpu_tensor_create(ctx, 1, edge_dims, NIMCP_DTYPE_INT32);
    if (!graph->col_indices) {
        nimcp_graph_gpu_destroy(graph);
        return NULL;
    }
    NIMCP_CUDA_CHECK_IMMUNE_NULL(cudaMemcpy(graph->col_indices->data, col_indices,
                              num_edges * sizeof(int), cudaMemcpyHostToDevice));

    // Copy weights if provided
    if (weights) {
        graph->edge_weights = nimcp_gpu_tensor_create(ctx, 1, edge_dims, NIMCP_DTYPE_FLOAT32);
        if (!graph->edge_weights) {
            nimcp_graph_gpu_destroy(graph);
            return NULL;
        }
        NIMCP_CUDA_CHECK_IMMUNE_NULL(cudaMemcpy(graph->edge_weights->data, weights,
                                  num_edges * sizeof(float), cudaMemcpyHostToDevice));
    }

    return graph;
}

void nimcp_graph_gpu_destroy(nimcp_graph_gpu_t* graph)
{
    if (!graph) return;

    if (graph->adjacency) nimcp_gpu_tensor_destroy(graph->adjacency);
    if (graph->edge_weights) nimcp_gpu_tensor_destroy(graph->edge_weights);
    if (graph->node_features) nimcp_gpu_tensor_destroy(graph->node_features);
    if (graph->row_ptrs) nimcp_gpu_tensor_destroy(graph->row_ptrs);
    if (graph->col_indices) nimcp_gpu_tensor_destroy(graph->col_indices);

    nimcp_free(graph);
}

bool nimcp_graph_gpu_is_valid(const nimcp_graph_gpu_t* graph)
{
    if (!graph || !graph->ctx) return false;
    if (graph->num_nodes <= 0) return false;
    if (!graph->is_sparse && !graph->adjacency) return false;
    if (graph->is_sparse && (!graph->row_ptrs || !graph->col_indices)) return false;
    return true;
}

//=============================================================================
// API Implementation - Graph Metrics
//=============================================================================

bool nimcp_topology_compute_degree(
    nimcp_gpu_context_t* ctx,
    nimcp_graph_gpu_t* graph,
    nimcp_gpu_tensor_t* degree_out)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !graph || !degree_out) return false;

    if (graph->is_sparse) {
        kernel_compute_degree_csr<<<GRID_SIZE(graph->num_nodes), BLOCK_SIZE>>>(
            (int*)graph->row_ptrs->data,
            (int*)degree_out->data,
            graph->num_nodes);
    } else {
        kernel_compute_degree_dense<<<GRID_SIZE(graph->num_nodes), BLOCK_SIZE>>>(
            (float*)graph->adjacency->data,
            (int*)degree_out->data,
            graph->num_nodes,
            0.0f);
    }

    NIMCP_CUDA_CHECK_IMMUNE(cudaGetLastError());
    return true;
}

bool nimcp_topology_compute_weighted_degree(
    nimcp_gpu_context_t* ctx,
    nimcp_graph_gpu_t* graph,
    nimcp_gpu_tensor_t* weighted_degree_out)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !graph || !weighted_degree_out) return false;
    if (!graph->is_sparse || !graph->edge_weights) {
        LOG_WARN("Weighted degree requires sparse graph with weights");
        return false;
    }

    kernel_compute_weighted_degree_csr<<<GRID_SIZE(graph->num_nodes), BLOCK_SIZE>>>(
        (int*)graph->row_ptrs->data,
        (float*)graph->edge_weights->data,
        (float*)weighted_degree_out->data,
        graph->num_nodes);

    NIMCP_CUDA_CHECK_IMMUNE(cudaGetLastError());
    return true;
}

bool nimcp_topology_compute_clustering(
    nimcp_gpu_context_t* ctx,
    nimcp_graph_gpu_t* graph,
    nimcp_gpu_tensor_t* clustering_out)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !graph || !clustering_out || !graph->is_sparse) {
        return false;
    }

    int n = graph->num_nodes;

    // Allocate temporary arrays
    int* d_triangles;
    int* d_degree;
    NIMCP_CUDA_CHECK_IMMUNE(cudaMalloc(&d_triangles, n * sizeof(int)));
    NIMCP_CUDA_CHECK_IMMUNE(cudaMalloc(&d_degree, n * sizeof(int)));

    // Compute degrees
    kernel_compute_degree_csr<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (int*)graph->row_ptrs->data, d_degree, n);

    // Count triangles
    kernel_count_triangles_csr<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (int*)graph->row_ptrs->data,
        (int*)graph->col_indices->data,
        d_triangles, n);

    // Compute clustering coefficients
    kernel_clustering_coefficient<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        d_degree, d_triangles, (float*)clustering_out->data, n);

    cudaFree(d_triangles);
    cudaFree(d_degree);

    NIMCP_CUDA_CHECK_IMMUNE(cudaGetLastError());
    return true;
}

bool nimcp_topology_compute_pagerank(
    nimcp_gpu_context_t* ctx,
    nimcp_graph_gpu_t* graph,
    float damping,
    int max_iter,
    float tolerance,
    nimcp_gpu_tensor_t* pagerank_out)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !graph || !pagerank_out || !graph->is_sparse) {
        return false;
    }

    int n = graph->num_nodes;

    // Allocate temporary arrays
    float* d_old_rank;
    int* d_out_degree;
    float* d_diff;
    NIMCP_CUDA_CHECK_IMMUNE(cudaMalloc(&d_old_rank, n * sizeof(float)));
    NIMCP_CUDA_CHECK_IMMUNE(cudaMalloc(&d_out_degree, n * sizeof(int)));
    NIMCP_CUDA_CHECK_IMMUNE(cudaMalloc(&d_diff, sizeof(float)));

    // Initialize PageRank
    float init_val = 1.0f / n;
    thrust::device_ptr<float> pr_ptr((float*)pagerank_out->data);
    thrust::fill(pr_ptr, pr_ptr + n, init_val);

    // Compute out-degrees
    kernel_compute_degree_csr<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (int*)graph->row_ptrs->data, d_out_degree, n);

    // Power iteration
    for (int iter = 0; iter < max_iter; iter++) {
        // Swap ranks
        cudaMemcpy(d_old_rank, pagerank_out->data, n * sizeof(float), cudaMemcpyDeviceToDevice);

        // PageRank iteration
        kernel_pagerank_iteration_csr<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (int*)graph->row_ptrs->data,
            (int*)graph->col_indices->data,
            d_old_rank,
            d_out_degree,
            (float*)pagerank_out->data,
            damping, n);

        // Check convergence
        cudaMemset(d_diff, 0, sizeof(float));
        kernel_pagerank_diff<<<GRID_SIZE(n), BLOCK_SIZE, BLOCK_SIZE * sizeof(float)>>>(
            d_old_rank, (float*)pagerank_out->data, d_diff, n);

        float h_diff;
        cudaMemcpy(&h_diff, d_diff, sizeof(float), cudaMemcpyDeviceToHost);

        if (h_diff < tolerance) {
            LOG_DEBUG("PageRank converged at iteration %d (diff=%.6f)", iter, h_diff);
            break;
        }
    }

    cudaFree(d_old_rank);
    cudaFree(d_out_degree);
    cudaFree(d_diff);

    NIMCP_CUDA_CHECK_IMMUNE(cudaGetLastError());
    return true;
}

//=============================================================================
// API Implementation - Shortest Paths
//=============================================================================

bool nimcp_shortest_path_bfs(
    nimcp_gpu_context_t* ctx,
    nimcp_graph_gpu_t* graph,
    int source,
    nimcp_shortest_path_result_gpu_t* result)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !graph || !result || !graph->is_sparse) {
        return false;
    }

    int n = graph->num_nodes;

    // Allocate result tensors if needed
    if (!result->distances) {
        int64_t dims[1] = {n};
        result->distances = nimcp_gpu_tensor_create(ctx, 1, dims, NIMCP_DTYPE_FLOAT32);
    }
    if (!result->predecessors) {
        int64_t dims[1] = {n};
        result->predecessors = nimcp_gpu_tensor_create(ctx, 1, dims, NIMCP_DTYPE_INT32);
    }

    // Allocate frontiers
    int* d_frontier;
    int* d_next_frontier;
    int* d_frontier_size;
    NIMCP_CUDA_CHECK_IMMUNE(cudaMalloc(&d_frontier, n * sizeof(int)));
    NIMCP_CUDA_CHECK_IMMUNE(cudaMalloc(&d_next_frontier, n * sizeof(int)));
    NIMCP_CUDA_CHECK_IMMUNE(cudaMalloc(&d_frontier_size, sizeof(int)));

    // Initialize distances
    kernel_bfs_init<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)result->distances->data,
        (int*)result->predecessors->data,
        source, n);

    // Set initial frontier
    cudaMemcpy(d_frontier, &source, sizeof(int), cudaMemcpyHostToDevice);
    int h_frontier_size = 1;
    float current_dist = 0.0f;

    while (h_frontier_size > 0) {
        cudaMemset(d_frontier_size, 0, sizeof(int));

        kernel_bfs_expand_csr<<<GRID_SIZE(h_frontier_size), BLOCK_SIZE>>>(
            (int*)graph->row_ptrs->data,
            (int*)graph->col_indices->data,
            d_frontier,
            d_next_frontier,
            d_frontier_size,
            (float*)result->distances->data,
            (int*)result->predecessors->data,
            current_dist,
            h_frontier_size);

        cudaMemcpy(&h_frontier_size, d_frontier_size, sizeof(int), cudaMemcpyDeviceToHost);

        // Swap frontiers
        int* temp = d_frontier;
        d_frontier = d_next_frontier;
        d_next_frontier = temp;

        current_dist += 1.0f;
    }

    result->max_distance = current_dist - 1.0f;

    // Count reachable nodes
    thrust::device_ptr<float> dist_ptr((float*)result->distances->data);
    result->num_reachable = thrust::count_if(dist_ptr, dist_ptr + n,
        [] __device__ (float d) { return d < INF_DISTANCE; });

    cudaFree(d_frontier);
    cudaFree(d_next_frontier);
    cudaFree(d_frontier_size);

    NIMCP_CUDA_CHECK_IMMUNE(cudaGetLastError());
    return true;
}

bool nimcp_shortest_path_floyd_warshall(
    nimcp_gpu_context_t* ctx,
    nimcp_graph_gpu_t* graph,
    nimcp_apsp_result_gpu_t* result)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !graph || !result || graph->is_sparse) {
        LOG_ERROR("Floyd-Warshall requires dense graph");
        return false;
    }

    int n = graph->num_nodes;

    // Allocate distance matrix
    int64_t dims[2] = {n, n};
    result->distances = nimcp_gpu_tensor_create(ctx, 2, dims, NIMCP_DTYPE_FLOAT32);
    if (!result->distances) return false;

    // Initialize distances from adjacency
    dim3 block(16, 16);
    dim3 grid((n + 15) / 16, (n + 15) / 16);

    kernel_floyd_warshall_init<<<grid, block>>>(
        (float*)graph->adjacency->data,
        (float*)result->distances->data,
        n);

    // Run Floyd-Warshall iterations
    for (int k = 0; k < n; k++) {
        kernel_floyd_warshall_iteration<<<grid, block>>>(
            (float*)result->distances->data, k, n);
    }

    // Compute statistics
    thrust::device_ptr<float> dist_ptr((float*)result->distances->data);
    auto minmax = thrust::minmax_element(dist_ptr, dist_ptr + n * n);
    result->diameter = *minmax.second;

    // Compute average path length (excluding infinity and self)
    float sum = thrust::transform_reduce(
        dist_ptr, dist_ptr + n * n,
        [] __device__ (float d) { return (d > 0.0f && d < INF_DISTANCE) ? d : 0.0f; },
        0.0f, thrust::plus<float>());
    int count = thrust::count_if(dist_ptr, dist_ptr + n * n,
        [] __device__ (float d) { return d > 0.0f && d < INF_DISTANCE; });
    result->avg_path_length = (count > 0) ? sum / count : 0.0f;

    NIMCP_CUDA_CHECK_IMMUNE(cudaGetLastError());
    return true;
}

//=============================================================================
// API Implementation - Community Detection
//=============================================================================

nimcp_community_result_gpu_t* nimcp_community_detect_louvain(
    nimcp_gpu_context_t* ctx,
    nimcp_graph_gpu_t* graph,
    float resolution,
    int max_iterations,
    float min_modularity_gain)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !graph || !graph->is_sparse) {
        LOG_ERROR("Louvain requires sparse graph");
        return NULL;
    }

    int n = graph->num_nodes;

    nimcp_community_result_gpu_t* result = (nimcp_community_result_gpu_t*)nimcp_calloc(1, sizeof(nimcp_community_result_gpu_t));
    if (!result) return NULL;

    // Allocate result tensors
    int64_t dims[1] = {n};
    result->node_communities = nimcp_gpu_tensor_create(ctx, 1, dims, NIMCP_DTYPE_INT32);
    result->community_sizes = nimcp_gpu_tensor_create(ctx, 1, dims, NIMCP_DTYPE_INT32);

    // Allocate working arrays
    int* d_degree;
    float* d_community_weights;
    int* d_changed;
    NIMCP_CUDA_CHECK_IMMUNE_NULL(cudaMalloc(&d_degree, n * sizeof(int)));
    NIMCP_CUDA_CHECK_IMMUNE_NULL(cudaMalloc(&d_community_weights, n * sizeof(float)));
    NIMCP_CUDA_CHECK_IMMUNE_NULL(cudaMalloc(&d_changed, sizeof(int)));

    // Compute degrees
    kernel_compute_degree_csr<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (int*)graph->row_ptrs->data, d_degree, n);

    // Compute total weight
    thrust::device_ptr<int> deg_ptr(d_degree);
    float total_weight = (float)thrust::reduce(deg_ptr, deg_ptr + n) / 2.0f;

    // Initialize communities
    kernel_init_communities<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (int*)result->node_communities->data,
        d_community_weights,
        d_degree, n);

    // Local moving phase
    for (int iter = 0; iter < max_iterations; iter++) {
        cudaMemset(d_changed, 0, sizeof(int));

        kernel_louvain_local_move<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (int*)graph->row_ptrs->data,
            (int*)graph->col_indices->data,
            graph->edge_weights ? (float*)graph->edge_weights->data : NULL,
            (int*)result->node_communities->data,
            d_community_weights,
            d_degree,
            total_weight,
            resolution,
            d_changed,
            n);

        int h_changed;
        cudaMemcpy(&h_changed, d_changed, sizeof(int), cudaMemcpyDeviceToHost);

        if (!h_changed) {
            LOG_DEBUG("Louvain converged at iteration %d", iter);
            break;
        }
    }

    // Count communities and compute sizes
    kernel_count_community_sizes<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (int*)result->node_communities->data,
        (int*)result->community_sizes->data,
        n, n);

    // Count non-empty communities
    thrust::device_ptr<int> size_ptr((int*)result->community_sizes->data);
    result->num_communities = thrust::count_if(size_ptr, size_ptr + n,
        [] __device__ (int s) { return s > 0; });

    // Compute modularity
    float* d_contribution;
    cudaMalloc(&d_contribution, sizeof(float));
    cudaMemset(d_contribution, 0, sizeof(float));

    float* d_degrees_f;
    cudaMalloc(&d_degrees_f, n * sizeof(float));
    thrust::device_ptr<float> deg_f_ptr(d_degrees_f);
    thrust::transform(deg_ptr, deg_ptr + n, deg_f_ptr,
        [] __device__ (int d) { return (float)d; });

    kernel_modularity_contribution_csr<<<GRID_SIZE(n), BLOCK_SIZE, BLOCK_SIZE * sizeof(float)>>>(
        (int*)graph->row_ptrs->data,
        (int*)graph->col_indices->data,
        graph->edge_weights ? (float*)graph->edge_weights->data : NULL,
        (int*)result->node_communities->data,
        d_degrees_f,
        d_contribution,
        total_weight,
        resolution,
        n);

    cudaMemcpy(&result->modularity, d_contribution, sizeof(float), cudaMemcpyDeviceToHost);
    result->modularity /= (2.0f * total_weight);

    // Cleanup
    cudaFree(d_degree);
    cudaFree(d_community_weights);
    cudaFree(d_changed);
    cudaFree(d_contribution);
    cudaFree(d_degrees_f);

    LOG_DEBUG("Louvain found %d communities with modularity %.4f",
              result->num_communities, result->modularity);
    return result;
}

nimcp_community_result_gpu_t* nimcp_community_detect_label_prop(
    nimcp_gpu_context_t* ctx,
    nimcp_graph_gpu_t* graph,
    int max_iter)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !graph || !graph->is_sparse) {
        return NULL;
    }

    int n = graph->num_nodes;

    nimcp_community_result_gpu_t* result = (nimcp_community_result_gpu_t*)nimcp_calloc(1, sizeof(nimcp_community_result_gpu_t));
    if (!result) return NULL;

    int64_t dims[1] = {n};
    result->node_communities = nimcp_gpu_tensor_create(ctx, 1, dims, NIMCP_DTYPE_INT32);
    result->community_sizes = nimcp_gpu_tensor_create(ctx, 1, dims, NIMCP_DTYPE_INT32);

    // Initialize labels (each node is its own label)
    thrust::device_ptr<int> label_ptr((int*)result->node_communities->data);
    thrust::sequence(label_ptr, label_ptr + n);

    int* d_changed;
    cudaMalloc(&d_changed, sizeof(int));

    for (int iter = 0; iter < max_iter; iter++) {
        cudaMemset(d_changed, 0, sizeof(int));

        kernel_label_propagation_csr<<<GRID_SIZE(n), BLOCK_SIZE>>>(
            (int*)graph->row_ptrs->data,
            (int*)graph->col_indices->data,
            graph->edge_weights ? (float*)graph->edge_weights->data : NULL,
            (int*)result->node_communities->data,
            d_changed,
            n);

        int h_changed;
        cudaMemcpy(&h_changed, d_changed, sizeof(int), cudaMemcpyDeviceToHost);

        if (!h_changed) {
            LOG_DEBUG("Label propagation converged at iteration %d", iter);
            break;
        }
    }

    // Count community sizes
    kernel_count_community_sizes<<<1, 256>>>(
        (int*)result->node_communities->data,
        (int*)result->community_sizes->data,
        n, n);

    thrust::device_ptr<int> size_ptr((int*)result->community_sizes->data);
    result->num_communities = thrust::count_if(size_ptr, size_ptr + n,
        [] __device__ (int s) { return s > 0; });

    cudaFree(d_changed);

    return result;
}

void nimcp_community_result_gpu_destroy(nimcp_community_result_gpu_t* result)
{
    if (!result) return;
    if (result->node_communities) nimcp_gpu_tensor_destroy(result->node_communities);
    if (result->community_sizes) nimcp_gpu_tensor_destroy(result->community_sizes);
    nimcp_free(result);
}

//=============================================================================
// API Implementation - Network Generation
//=============================================================================

nimcp_graph_gpu_t* nimcp_graph_generate_erdos_renyi(
    nimcp_gpu_context_t* ctx,
    int num_nodes,
    float edge_probability,
    uint32_t seed)
{
    if (!ctx || num_nodes <= 0 || edge_probability < 0.0f || edge_probability > 1.0f) {
        return NULL;
    }

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_create(ctx, num_nodes, false);
    if (!graph) return NULL;

    // Allocate random states
    int total_pairs = num_nodes * num_nodes;
    int num_states = BLOCK_SIZE * GRID_SIZE(total_pairs);
    curandState* d_states;
    cudaMalloc(&d_states, num_states * sizeof(curandState));

    // Initialize random states
    uint64_t actual_seed = (seed == 0) ? time(NULL) : seed;
    kernel_init_random<<<GRID_SIZE(num_states), BLOCK_SIZE>>>(d_states, actual_seed, num_states);

    // Generate random edges
    kernel_erdos_renyi_generate<<<GRID_SIZE(total_pairs), BLOCK_SIZE>>>(
        (float*)graph->adjacency->data,
        d_states,
        edge_probability,
        num_nodes);

    cudaFree(d_states);

    // Count edges
    thrust::device_ptr<float> adj_ptr((float*)graph->adjacency->data);
    graph->num_edges = (int)thrust::reduce(adj_ptr, adj_ptr + total_pairs) / 2;

    LOG_DEBUG("Generated Erdos-Renyi graph: %d nodes, %d edges (p=%.3f)",
              num_nodes, graph->num_edges, edge_probability);
    return graph;
}

nimcp_graph_gpu_t* nimcp_graph_generate_watts_strogatz(
    nimcp_gpu_context_t* ctx,
    int num_nodes,
    int k,
    float rewire_prob,
    uint32_t seed)
{
    if (!ctx || num_nodes <= 0 || k <= 0 || k >= num_nodes) {
        return NULL;
    }

    nimcp_graph_gpu_t* graph = nimcp_graph_gpu_create(ctx, num_nodes, false);
    if (!graph) return NULL;

    // Generate ring lattice
    kernel_ring_lattice<<<GRID_SIZE(num_nodes), BLOCK_SIZE>>>(
        (float*)graph->adjacency->data,
        num_nodes, k);

    if (rewire_prob > 0.0f) {
        // Allocate random states
        curandState* d_states;
        cudaMalloc(&d_states, num_nodes * sizeof(curandState));

        uint64_t actual_seed = (seed == 0) ? time(NULL) : seed;
        kernel_init_random<<<GRID_SIZE(num_nodes), BLOCK_SIZE>>>(d_states, actual_seed, num_nodes);

        // Rewire edges
        kernel_watts_strogatz_rewire<<<GRID_SIZE(num_nodes), BLOCK_SIZE>>>(
            (float*)graph->adjacency->data,
            d_states,
            rewire_prob,
            num_nodes, k);

        cudaFree(d_states);
    }

    // Count edges
    int total = num_nodes * num_nodes;
    thrust::device_ptr<float> adj_ptr((float*)graph->adjacency->data);
    graph->num_edges = (int)thrust::reduce(adj_ptr, adj_ptr + total) / 2;

    LOG_DEBUG("Generated Watts-Strogatz graph: %d nodes, %d edges (k=%d, p=%.3f)",
              num_nodes, graph->num_edges, k, rewire_prob);
    return graph;
}

//=============================================================================
// API Implementation - Graph Utilities
//=============================================================================

bool nimcp_graph_gpu_symmetrize(nimcp_graph_gpu_t* graph)
{
    if (!graph || graph->is_sparse) return false;

    int n = graph->num_nodes;
    dim3 block(16, 16);
    dim3 grid((n + 15) / 16, (n + 15) / 16);

    kernel_symmetrize<<<grid, block>>>(
        (float*)graph->adjacency->data, n);

    NIMCP_CUDA_CHECK_IMMUNE(cudaGetLastError());
    return true;
}

bool nimcp_graph_gpu_remove_self_loops(nimcp_graph_gpu_t* graph)
{
    if (!graph || graph->is_sparse) return false;

    kernel_remove_self_loops<<<GRID_SIZE(graph->num_nodes), BLOCK_SIZE>>>(
        (float*)graph->adjacency->data, graph->num_nodes);

    NIMCP_CUDA_CHECK_IMMUNE(cudaGetLastError());
    return true;
}

bool nimcp_graph_gpu_to_csr(nimcp_graph_gpu_t* graph, float threshold)
{
    if (!graph || graph->is_sparse || !graph->adjacency) return false;

    int n = graph->num_nodes;

    // Count non-zeros per row
    int* d_row_counts;
    cudaMalloc(&d_row_counts, n * sizeof(int));

    kernel_dense_to_csr_count<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)graph->adjacency->data,
        d_row_counts, threshold, n);

    // Exclusive scan to get row pointers
    int64_t row_dims[1] = {n + 1};
    graph->row_ptrs = nimcp_gpu_tensor_create(graph->ctx, 1, row_dims, NIMCP_DTYPE_INT32);

    thrust::device_ptr<int> count_ptr(d_row_counts);
    thrust::device_ptr<int> row_ptr((int*)graph->row_ptrs->data);
    thrust::exclusive_scan(count_ptr, count_ptr + n, row_ptr);

    // Get total nnz
    int last_count;
    cudaMemcpy(&last_count, d_row_counts + n - 1, sizeof(int), cudaMemcpyDeviceToHost);
    int last_ptr;
    cudaMemcpy(&last_ptr, (int*)graph->row_ptrs->data + n - 1, sizeof(int), cudaMemcpyDeviceToHost);
    int nnz = last_ptr + last_count;

    // Set final row pointer
    cudaMemcpy((int*)graph->row_ptrs->data + n, &nnz, sizeof(int), cudaMemcpyHostToDevice);

    // Allocate and fill column indices and values
    int64_t edge_dims[1] = {nnz};
    graph->col_indices = nimcp_gpu_tensor_create(graph->ctx, 1, edge_dims, NIMCP_DTYPE_INT32);
    graph->edge_weights = nimcp_gpu_tensor_create(graph->ctx, 1, edge_dims, NIMCP_DTYPE_FLOAT32);

    kernel_dense_to_csr_extract<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        (float*)graph->adjacency->data,
        (int*)graph->row_ptrs->data,
        (int*)graph->col_indices->data,
        (float*)graph->edge_weights->data,
        threshold, n);

    graph->num_edges = nnz;
    graph->is_sparse = true;

    // Free dense adjacency
    nimcp_gpu_tensor_destroy(graph->adjacency);
    graph->adjacency = NULL;

    cudaFree(d_row_counts);

    NIMCP_CUDA_CHECK_IMMUNE(cudaGetLastError());
    return true;
}

void nimcp_shortest_path_result_gpu_destroy(nimcp_shortest_path_result_gpu_t* result)
{
    if (!result) return;
    if (result->distances) nimcp_gpu_tensor_destroy(result->distances);
    if (result->predecessors) nimcp_gpu_tensor_destroy(result->predecessors);
}

void nimcp_apsp_result_gpu_destroy(nimcp_apsp_result_gpu_t* result)
{
    if (!result) return;
    if (result->distances) nimcp_gpu_tensor_destroy(result->distances);
}

void nimcp_topology_metrics_gpu_destroy(nimcp_topology_metrics_gpu_t* metrics)
{
    if (!metrics) return;
    if (metrics->degree) nimcp_gpu_tensor_destroy(metrics->degree);
    if (metrics->weighted_degree) nimcp_gpu_tensor_destroy(metrics->weighted_degree);
    if (metrics->clustering_coeff) nimcp_gpu_tensor_destroy(metrics->clustering_coeff);
    if (metrics->betweenness) nimcp_gpu_tensor_destroy(metrics->betweenness);
    if (metrics->pagerank) nimcp_gpu_tensor_destroy(metrics->pagerank);
    nimcp_free(metrics);
}

#else // !NIMCP_ENABLE_CUDA

// Stub implementations when CUDA is not available

nimcp_graph_gpu_t* nimcp_graph_gpu_create(nimcp_gpu_context_t* ctx, int num_nodes, bool sparse) {
    (void)ctx; (void)num_nodes; (void)sparse;
    return NULL;
}

void nimcp_graph_gpu_destroy(nimcp_graph_gpu_t* graph) {
    (void)graph;
}

bool nimcp_graph_gpu_is_valid(const nimcp_graph_gpu_t* graph) {
    (void)graph;
    return false;
}

nimcp_community_result_gpu_t* nimcp_community_detect_louvain(
    nimcp_gpu_context_t* ctx, nimcp_graph_gpu_t* graph,
    float resolution, int max_iterations, float min_modularity_gain) {
    (void)ctx; (void)graph; (void)resolution; (void)max_iterations; (void)min_modularity_gain;
    return NULL;
}

void nimcp_community_result_gpu_destroy(nimcp_community_result_gpu_t* result) {
    (void)result;
}

#endif // NIMCP_ENABLE_CUDA
