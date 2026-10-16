/**
 * @file nimcp_knowledge_graph_kernels.cu
 * @brief GPU Knowledge Graph CUDA Kernels Implementation
 *
 * WHAT: CUDA kernels for knowledge graph operations
 * WHY:  GPU acceleration for large-scale graph traversal and similarity
 * HOW:  Custom kernels for BFS/DFS, embeddings, and hyperbolic operations
 *
 * ARCHITECTURE:
 * - Parallel graph traversal (BFS/DFS across frontiers)
 * - Semantic similarity via cosine similarity on embeddings
 * - Hyperbolic space operations for hierarchical knowledge
 * - Node/edge feature aggregation with shared memory
 * - Subgraph matching with parallel pattern detection
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

// Include CUDA headers FIRST (before any extern "C" blocks from our headers)
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include <math.h>
#include <float.h>

// Now include our headers (which have extern "C" blocks)
#include "gpu/knowledge/nimcp_knowledge_graph_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "KNOWLEDGE_GPU"

//=============================================================================
// CUDA Error Checking - Using recovery-integrated macros for self-healing
//=============================================================================

// Map legacy macros to recovery-integrated versions
#define CUDA_CHECK(call) NIMCP_CUDA_RECOVER(call, GPU_ERROR_CUDA_RUNTIME)
#define CUDA_CHECK_NULL(call) NIMCP_CUDA_RECOVER_NULL(call, GPU_ERROR_OUT_OF_MEMORY)
// CUDA_CHECK_VOID uses warning only since void functions can't propagate errors
#define CUDA_CHECK_VOID(call) NIMCP_CUDA_CHECK_WARN(call)

//=============================================================================
// Kernel Configuration
//=============================================================================

#define BLOCK_SIZE 256
#define WARP_SIZE 32
#define MAX_SHARED_MEM 48000  // 48KB shared memory limit

// Calculate grid size for N elements
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

// Invalid node marker for BFS/DFS
#define INVALID_NODE 0xFFFFFFFF

//=============================================================================
// Device Utility Functions
//=============================================================================

/**
 * @brief Warp-level reduction sum
 */
__device__ inline float warp_reduce_sum(float val)
{
    for (int offset = WARP_SIZE / 2; offset > 0; offset /= 2) {
        val += __shfl_down_sync(0xFFFFFFFF, val, offset);
    }
    return val;
}

/**
 * @brief Warp-level reduction max
 */
__device__ inline float warp_reduce_max(float val)
{
    for (int offset = WARP_SIZE / 2; offset > 0; offset /= 2) {
        val = fmaxf(val, __shfl_down_sync(0xFFFFFFFF, val, offset));
    }
    return val;
}

/**
 * @brief Block-level reduction sum using shared memory
 */
__device__ inline float block_reduce_sum(float val, float* shared_mem)
{
    int lane = threadIdx.x % WARP_SIZE;
    int wid = threadIdx.x / WARP_SIZE;

    // Warp-level reduction
    val = warp_reduce_sum(val);

    // Write reduced value to shared memory
    if (lane == 0) {
        shared_mem[wid] = val;
    }
    __syncthreads();

    // Read from shared memory only if that warp existed
    int num_warps = (blockDim.x + WARP_SIZE - 1) / WARP_SIZE;
    val = (threadIdx.x < num_warps) ? shared_mem[threadIdx.x] : 0.0f;

    // Final warp reduction
    if (wid == 0) {
        val = warp_reduce_sum(val);
    }

    return val;
}

/**
 * @brief Compute L2 norm of a vector segment
 */
__device__ inline float compute_l2_norm(const float* vec, uint32_t dim)
{
    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        sum += vec[i] * vec[i];
    }
    return sqrtf(sum);
}

/**
 * @brief Compute dot product of two vectors
 */
__device__ inline float compute_dot_product(const float* a, const float* b, uint32_t dim)
{
    float sum = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

//=============================================================================
// Graph Creation and Destruction
//=============================================================================

nimcp_gpu_knowledge_graph_t* nimcp_gpu_knowledge_graph_create(
    nimcp_gpu_context_t* ctx,
    const uint32_t* row_offsets,
    const uint32_t* col_indices,
    const float* edge_weights,
    uint32_t num_nodes,
    uint32_t num_edges,
    uint32_t embed_dim)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !row_offsets || !col_indices) {
        LOG_ERROR("Invalid parameters for graph creation");
        return NULL;
    }

    if (num_nodes == 0) {
        LOG_ERROR("Graph must have at least one node");
        return NULL;
    }

    nimcp_gpu_knowledge_graph_t* graph = (nimcp_gpu_knowledge_graph_t*)nimcp_calloc(1, sizeof(nimcp_gpu_knowledge_graph_t));
    if (!graph) {
        LOG_ERROR("Failed to allocate graph structure");
        return NULL;
    }

    graph->num_nodes = num_nodes;
    graph->num_edges = num_edges;
    graph->embed_dim = embed_dim;
    graph->is_hyperbolic = false;
    graph->ctx = ctx;

    // Create row_offsets tensor [num_nodes + 1]
    size_t row_dims[] = {num_nodes + 1};
    graph->row_offsets = nimcp_gpu_tensor_from_host(ctx, row_offsets, row_dims, 1, NIMCP_GPU_PRECISION_UINT32);
    if (!graph->row_offsets) {
        LOG_ERROR("Failed to create row_offsets tensor");
        nimcp_gpu_knowledge_graph_destroy(graph);
        return NULL;
    }

    // Create col_indices tensor [num_edges]
    if (num_edges > 0) {
        size_t col_dims[] = {num_edges};
        graph->col_indices = nimcp_gpu_tensor_from_host(ctx, col_indices, col_dims, 1, NIMCP_GPU_PRECISION_UINT32);
        if (!graph->col_indices) {
            LOG_ERROR("Failed to create col_indices tensor");
            nimcp_gpu_knowledge_graph_destroy(graph);
            return NULL;
        }

        // Create edge_weights tensor if provided
        if (edge_weights) {
            graph->edge_weights = nimcp_gpu_tensor_from_host(ctx, edge_weights, col_dims, 1, NIMCP_GPU_PRECISION_FP32);
            if (!graph->edge_weights) {
                LOG_ERROR("Failed to create edge_weights tensor");
                nimcp_gpu_knowledge_graph_destroy(graph);
                return NULL;
            }
        }
    }

    // Create node_embeddings tensor [num_nodes x embed_dim]
    if (embed_dim > 0) {
        size_t embed_dims[] = {num_nodes, embed_dim};
        graph->node_embeddings = nimcp_gpu_tensor_create(ctx, embed_dims, 2, NIMCP_GPU_PRECISION_FP32);
        if (!graph->node_embeddings) {
            LOG_ERROR("Failed to create node_embeddings tensor");
            nimcp_gpu_knowledge_graph_destroy(graph);
            return NULL;
        }
    }

    LOG_DEBUG("Created GPU knowledge graph: %u nodes, %u edges, embed_dim=%u",
              num_nodes, num_edges, embed_dim);
    return graph;
}

void nimcp_gpu_knowledge_graph_destroy(nimcp_gpu_knowledge_graph_t* graph)
{
    if (!graph) return;

    nimcp_gpu_tensor_destroy(graph->row_offsets);
    nimcp_gpu_tensor_destroy(graph->col_indices);
    nimcp_gpu_tensor_destroy(graph->edge_weights);
    nimcp_gpu_tensor_destroy(graph->node_embeddings);
    nimcp_gpu_tensor_destroy(graph->edge_embeddings);

    nimcp_free(graph);
}

bool nimcp_gpu_knowledge_graph_set_embeddings(
    nimcp_gpu_knowledge_graph_t* graph,
    const float* embeddings)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !embeddings) {
        LOG_ERROR("Invalid parameters for set_embeddings");
        return false;
    }

    if (!graph->node_embeddings) {
        LOG_ERROR("Graph has no embedding tensor allocated");
        return false;
    }

    size_t data_size = graph->num_nodes * graph->embed_dim * sizeof(float);
    CUDA_CHECK(cudaMemcpy(graph->node_embeddings->data, embeddings, data_size, cudaMemcpyHostToDevice));

    graph->is_hyperbolic = false;
    return true;
}

bool nimcp_gpu_knowledge_graph_set_hyperbolic_embeddings(
    nimcp_gpu_knowledge_graph_t* graph,
    const float* embeddings)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!nimcp_gpu_knowledge_graph_set_embeddings(graph, embeddings)) {
        return false;
    }
    graph->is_hyperbolic = true;
    return true;
}

//=============================================================================
// BFS Kernels
//=============================================================================

/**
 * @brief Initialize BFS distances and visited arrays
 */
__global__ void kernel_bfs_init(
    uint32_t* distances,
    uint32_t* parents,
    uint32_t* visited,
    uint32_t source,
    uint32_t num_nodes)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_nodes) return;

    if (idx == source) {
        distances[idx] = 0;
        parents[idx] = source;
        visited[idx] = 1;
    } else {
        distances[idx] = INVALID_NODE;
        parents[idx] = INVALID_NODE;
        visited[idx] = 0;
    }
}

/**
 * @brief BFS frontier expansion kernel
 *
 * Each thread processes one node in the current frontier and marks neighbors.
 */
__global__ void kernel_bfs_expand(
    const uint32_t* row_offsets,
    const uint32_t* col_indices,
    const uint32_t* current_frontier,
    uint32_t frontier_size,
    uint32_t* distances,
    uint32_t* parents,
    uint32_t* visited,
    uint32_t* next_frontier,
    uint32_t* next_frontier_count,
    uint32_t current_depth,
    uint32_t num_nodes)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= frontier_size) return;

    uint32_t node = current_frontier[idx];
    if (node >= num_nodes) return;

    uint32_t row_start = row_offsets[node];
    uint32_t row_end = row_offsets[node + 1];

    for (uint32_t e = row_start; e < row_end; e++) {
        uint32_t neighbor = col_indices[e];
        if (neighbor >= num_nodes) continue;

        // Try to mark neighbor as visited
        uint32_t old = atomicCAS(&visited[neighbor], 0, 1);
        if (old == 0) {
            // First visit - update distance and parent
            distances[neighbor] = current_depth + 1;
            parents[neighbor] = node;

            // Add to next frontier
            uint32_t pos = atomicAdd(next_frontier_count, 1);
            next_frontier[pos] = neighbor;
        }
    }
}

/**
 * @brief Multi-source BFS initialization
 */
__global__ void kernel_bfs_init_multi_source(
    uint32_t* distances,
    uint32_t* parents,
    uint32_t* visited,
    const uint32_t* source_nodes,
    uint32_t num_sources,
    uint32_t num_nodes)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_nodes) return;

    distances[idx] = INVALID_NODE;
    parents[idx] = INVALID_NODE;
    visited[idx] = 0;

    // Check if this node is a source
    for (uint32_t s = 0; s < num_sources; s++) {
        if (source_nodes[s] == idx) {
            distances[idx] = 0;
            parents[idx] = idx;
            visited[idx] = 1;
            break;
        }
    }
}

bool nimcp_gpu_bfs(
    nimcp_gpu_knowledge_graph_t* graph,
    uint32_t source_node,
    int32_t max_depth,
    nimcp_graph_traversal_result_t* result)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !result) {
        LOG_ERROR("Invalid parameters for BFS");
        return false;
    }

    if (source_node >= graph->num_nodes) {
        LOG_ERROR("Source node %u out of range (max %u)", source_node, graph->num_nodes - 1);
        return false;
    }

    uint32_t num_nodes = graph->num_nodes;
    size_t node_dims[] = {num_nodes};

    // Create result tensors
    result->distances = nimcp_gpu_tensor_create(graph->ctx, node_dims, 1, NIMCP_GPU_PRECISION_UINT32);
    result->parents = nimcp_gpu_tensor_create(graph->ctx, node_dims, 1, NIMCP_GPU_PRECISION_UINT32);
    result->visited = nimcp_gpu_tensor_create(graph->ctx, node_dims, 1, NIMCP_GPU_PRECISION_UINT32);

    if (!result->distances || !result->parents || !result->visited) {
        LOG_ERROR("Failed to allocate BFS result tensors");
        nimcp_graph_traversal_result_destroy(result);
        return false;
    }

    // Allocate frontier buffers
    uint32_t* d_current_frontier = NULL;
    uint32_t* d_next_frontier = NULL;
    uint32_t* d_frontier_count = NULL;
    bool success = false;

    if (cudaMalloc(&d_current_frontier, num_nodes * sizeof(uint32_t)) != cudaSuccess) goto bfs_cleanup;
    if (cudaMalloc(&d_next_frontier, num_nodes * sizeof(uint32_t)) != cudaSuccess) goto bfs_cleanup;
    if (cudaMalloc(&d_frontier_count, sizeof(uint32_t)) != cudaSuccess) goto bfs_cleanup;

    // Initialize distances and visited
    kernel_bfs_init<<<GRID_SIZE(num_nodes), BLOCK_SIZE>>>(
        (uint32_t*)result->distances->data,
        (uint32_t*)result->parents->data,
        (uint32_t*)result->visited->data,
        source_node,
        num_nodes);
    if (cudaGetLastError() != cudaSuccess) goto bfs_cleanup;

    // Initialize frontier with source
    if (cudaMemcpy(d_current_frontier, &source_node, sizeof(uint32_t), cudaMemcpyHostToDevice) != cudaSuccess) goto bfs_cleanup;

    {
        uint32_t frontier_size = 1;
        uint32_t current_depth = 0;
        uint32_t total_visited = 1;

        // BFS loop
        while (frontier_size > 0) {
            if (max_depth >= 0 && (int32_t)current_depth >= max_depth) {
                break;
            }

            // Reset next frontier count
            if (cudaMemset(d_frontier_count, 0, sizeof(uint32_t)) != cudaSuccess) goto bfs_cleanup;

            // Expand frontier
            kernel_bfs_expand<<<GRID_SIZE(frontier_size), BLOCK_SIZE>>>(
                (const uint32_t*)graph->row_offsets->data,
                (const uint32_t*)graph->col_indices->data,
                d_current_frontier,
                frontier_size,
                (uint32_t*)result->distances->data,
                (uint32_t*)result->parents->data,
                (uint32_t*)result->visited->data,
                d_next_frontier,
                d_frontier_count,
                current_depth,
                num_nodes);
            if (cudaGetLastError() != cudaSuccess) goto bfs_cleanup;

            // Get next frontier size
            uint32_t next_size = 0;
            if (cudaMemcpy(&next_size, d_frontier_count, sizeof(uint32_t), cudaMemcpyDeviceToHost) != cudaSuccess) goto bfs_cleanup;

            // Swap frontiers
            uint32_t* temp = d_current_frontier;
            d_current_frontier = d_next_frontier;
            d_next_frontier = temp;

            frontier_size = next_size;
            total_visited += next_size;
            current_depth++;
        }

        result->num_visited = total_visited;
        result->max_depth = current_depth;
    }

    success = true;

bfs_cleanup:
    cudaFree(d_current_frontier);
    cudaFree(d_next_frontier);
    cudaFree(d_frontier_count);

    return success;
}

bool nimcp_gpu_bfs_multi_source(
    nimcp_gpu_knowledge_graph_t* graph,
    const uint32_t* source_nodes,
    uint32_t num_sources,
    int32_t max_depth,
    nimcp_graph_traversal_result_t* result)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !source_nodes || !result || num_sources == 0) {
        LOG_ERROR("Invalid parameters for multi-source BFS");
        return false;
    }

    uint32_t num_nodes = graph->num_nodes;
    size_t node_dims[] = {num_nodes};

    // Create result tensors
    result->distances = nimcp_gpu_tensor_create(graph->ctx, node_dims, 1, NIMCP_GPU_PRECISION_UINT32);
    result->parents = nimcp_gpu_tensor_create(graph->ctx, node_dims, 1, NIMCP_GPU_PRECISION_UINT32);
    result->visited = nimcp_gpu_tensor_create(graph->ctx, node_dims, 1, NIMCP_GPU_PRECISION_UINT32);

    if (!result->distances || !result->parents || !result->visited) {
        nimcp_graph_traversal_result_destroy(result);
        return false;
    }

    // Copy source nodes to device
    uint32_t* d_sources = NULL;
    uint32_t* d_current_frontier = NULL;
    uint32_t* d_next_frontier = NULL;
    uint32_t* d_frontier_count = NULL;
    bool success = false;

    if (cudaMalloc(&d_sources, num_sources * sizeof(uint32_t)) != cudaSuccess) goto bfs_multi_cleanup;
    if (cudaMemcpy(d_sources, source_nodes, num_sources * sizeof(uint32_t), cudaMemcpyHostToDevice) != cudaSuccess) goto bfs_multi_cleanup;

    // Allocate frontier buffers
    if (cudaMalloc(&d_current_frontier, num_nodes * sizeof(uint32_t)) != cudaSuccess) goto bfs_multi_cleanup;
    if (cudaMalloc(&d_next_frontier, num_nodes * sizeof(uint32_t)) != cudaSuccess) goto bfs_multi_cleanup;
    if (cudaMalloc(&d_frontier_count, sizeof(uint32_t)) != cudaSuccess) goto bfs_multi_cleanup;

    // Initialize with multi-source
    kernel_bfs_init_multi_source<<<GRID_SIZE(num_nodes), BLOCK_SIZE>>>(
        (uint32_t*)result->distances->data,
        (uint32_t*)result->parents->data,
        (uint32_t*)result->visited->data,
        d_sources,
        num_sources,
        num_nodes);
    if (cudaGetLastError() != cudaSuccess) goto bfs_multi_cleanup;

    // Initialize frontier with all sources
    if (cudaMemcpy(d_current_frontier, source_nodes, num_sources * sizeof(uint32_t), cudaMemcpyHostToDevice) != cudaSuccess) goto bfs_multi_cleanup;

    {
        uint32_t frontier_size = num_sources;
        uint32_t current_depth = 0;
        uint32_t total_visited = num_sources;

        // BFS loop
        while (frontier_size > 0) {
            if (max_depth >= 0 && (int32_t)current_depth >= max_depth) {
                break;
            }

            if (cudaMemset(d_frontier_count, 0, sizeof(uint32_t)) != cudaSuccess) goto bfs_multi_cleanup;

            kernel_bfs_expand<<<GRID_SIZE(frontier_size), BLOCK_SIZE>>>(
                (const uint32_t*)graph->row_offsets->data,
                (const uint32_t*)graph->col_indices->data,
                d_current_frontier,
                frontier_size,
                (uint32_t*)result->distances->data,
                (uint32_t*)result->parents->data,
                (uint32_t*)result->visited->data,
                d_next_frontier,
                d_frontier_count,
                current_depth,
                num_nodes);
            if (cudaGetLastError() != cudaSuccess) goto bfs_multi_cleanup;

            uint32_t next_size = 0;
            if (cudaMemcpy(&next_size, d_frontier_count, sizeof(uint32_t), cudaMemcpyDeviceToHost) != cudaSuccess) goto bfs_multi_cleanup;

            uint32_t* temp = d_current_frontier;
            d_current_frontier = d_next_frontier;
            d_next_frontier = temp;

            frontier_size = next_size;
            total_visited += next_size;
            current_depth++;
        }

        result->num_visited = total_visited;
        result->max_depth = current_depth;
    }

    success = true;

bfs_multi_cleanup:
    cudaFree(d_sources);
    cudaFree(d_current_frontier);
    cudaFree(d_next_frontier);
    cudaFree(d_frontier_count);

    return success;
}

//=============================================================================
// DFS Kernels
//=============================================================================

/**
 * @brief DFS kernel using iterative approach with work-stealing
 *
 * Uses a parallel stack-based approach where each thread block processes
 * a portion of the graph.
 */
__global__ void kernel_dfs_explore(
    const uint32_t* row_offsets,
    const uint32_t* col_indices,
    uint32_t* stack,
    uint32_t* stack_ptr,
    uint32_t* distances,
    uint32_t* parents,
    uint32_t* visited,
    uint32_t* visit_order,
    uint32_t* visit_count,
    int32_t max_depth,
    uint32_t num_nodes)
{
    __shared__ uint32_t shared_stack[256];
    __shared__ uint32_t shared_stack_ptr;

    if (threadIdx.x == 0) {
        shared_stack_ptr = 0;
    }
    __syncthreads();

    while (true) {
        // Try to pop from global stack
        uint32_t current = INVALID_NODE;
        uint32_t current_depth = 0;

        if (threadIdx.x == 0) {
            uint32_t old_ptr = atomicSub(stack_ptr, 1);
            if (old_ptr > 0) {
                current = stack[old_ptr - 1];
                current_depth = distances[current];
            } else {
                atomicAdd(stack_ptr, 1);  // Restore if empty
            }
        }

        current = __shfl_sync(0xFFFFFFFF, current, 0);
        current_depth = __shfl_sync(0xFFFFFFFF, current_depth, 0);

        if (current == INVALID_NODE) {
            break;  // Stack is empty
        }

        // Check depth limit
        if (max_depth >= 0 && (int32_t)current_depth >= max_depth) {
            continue;
        }

        // Explore neighbors
        uint32_t row_start = row_offsets[current];
        uint32_t row_end = row_offsets[current + 1];

        for (uint32_t e = row_start + threadIdx.x; e < row_end; e += blockDim.x) {
            uint32_t neighbor = col_indices[e];
            if (neighbor >= num_nodes) continue;

            uint32_t old = atomicCAS(&visited[neighbor], 0, 1);
            if (old == 0) {
                distances[neighbor] = current_depth + 1;
                parents[neighbor] = current;

                // Record visit order
                uint32_t order = atomicAdd(visit_count, 1);
                visit_order[order] = neighbor;

                // Add to stack
                uint32_t pos = atomicAdd(stack_ptr, 1);
                if (pos < num_nodes) {
                    stack[pos] = neighbor;
                }
            }
        }

        __syncthreads();
    }
}

bool nimcp_gpu_dfs(
    nimcp_gpu_knowledge_graph_t* graph,
    uint32_t source_node,
    int32_t max_depth,
    nimcp_graph_traversal_result_t* result)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !result) {
        LOG_ERROR("Invalid parameters for DFS");
        return false;
    }

    if (source_node >= graph->num_nodes) {
        LOG_ERROR("Source node out of range");
        return false;
    }

    uint32_t num_nodes = graph->num_nodes;
    size_t node_dims[] = {num_nodes};

    // Create result tensors
    result->distances = nimcp_gpu_tensor_create(graph->ctx, node_dims, 1, NIMCP_GPU_PRECISION_UINT32);
    result->parents = nimcp_gpu_tensor_create(graph->ctx, node_dims, 1, NIMCP_GPU_PRECISION_UINT32);
    result->visited = nimcp_gpu_tensor_create(graph->ctx, node_dims, 1, NIMCP_GPU_PRECISION_UINT32);

    if (!result->distances || !result->parents || !result->visited) {
        nimcp_graph_traversal_result_destroy(result);
        return false;
    }

    // Allocate DFS stack and auxiliary arrays
    uint32_t* d_stack = NULL;
    uint32_t* d_stack_ptr = NULL;
    uint32_t* d_visit_order = NULL;
    uint32_t* d_visit_count = NULL;
    bool success = false;

    if (cudaMalloc(&d_stack, num_nodes * sizeof(uint32_t)) != cudaSuccess) goto dfs_cleanup;
    if (cudaMalloc(&d_stack_ptr, sizeof(uint32_t)) != cudaSuccess) goto dfs_cleanup;
    if (cudaMalloc(&d_visit_order, num_nodes * sizeof(uint32_t)) != cudaSuccess) goto dfs_cleanup;
    if (cudaMalloc(&d_visit_count, sizeof(uint32_t)) != cudaSuccess) goto dfs_cleanup;

    // Initialize
    kernel_bfs_init<<<GRID_SIZE(num_nodes), BLOCK_SIZE>>>(
        (uint32_t*)result->distances->data,
        (uint32_t*)result->parents->data,
        (uint32_t*)result->visited->data,
        source_node,
        num_nodes);
    if (cudaGetLastError() != cudaSuccess) goto dfs_cleanup;

    // Initialize stack with source
    if (cudaMemcpy(d_stack, &source_node, sizeof(uint32_t), cudaMemcpyHostToDevice) != cudaSuccess) goto dfs_cleanup;
    {
        uint32_t one = 1;
        if (cudaMemcpy(d_stack_ptr, &one, sizeof(uint32_t), cudaMemcpyHostToDevice) != cudaSuccess) goto dfs_cleanup;
    }
    if (cudaMemset(d_visit_count, 0, sizeof(uint32_t)) != cudaSuccess) goto dfs_cleanup;

    // Run DFS exploration
    kernel_dfs_explore<<<1, BLOCK_SIZE>>>(
        (const uint32_t*)graph->row_offsets->data,
        (const uint32_t*)graph->col_indices->data,
        d_stack,
        d_stack_ptr,
        (uint32_t*)result->distances->data,
        (uint32_t*)result->parents->data,
        (uint32_t*)result->visited->data,
        d_visit_order,
        d_visit_count,
        max_depth,
        num_nodes);
    if (cudaGetLastError() != cudaSuccess) goto dfs_cleanup;
    if (cudaDeviceSynchronize() != cudaSuccess) goto dfs_cleanup;

    {
        // Get visit count
        uint32_t visit_count = 0;
        if (cudaMemcpy(&visit_count, d_visit_count, sizeof(uint32_t), cudaMemcpyDeviceToHost) != cudaSuccess) goto dfs_cleanup;

        result->num_visited = visit_count + 1;  // +1 for source
        result->max_depth = max_depth >= 0 ? max_depth : num_nodes;
    }

    success = true;

dfs_cleanup:
    cudaFree(d_stack);
    cudaFree(d_stack_ptr);
    cudaFree(d_visit_order);
    cudaFree(d_visit_count);

    return success;
}

//=============================================================================
// Shortest Path (Bidirectional BFS)
//=============================================================================

/**
 * @brief Check if frontiers from forward and backward BFS have met
 */
__global__ void kernel_check_frontier_intersection(
    const uint32_t* forward_visited,
    const uint32_t* backward_visited,
    uint32_t* meeting_point,
    uint32_t num_nodes)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_nodes) return;

    if (forward_visited[idx] && backward_visited[idx]) {
        atomicMin(meeting_point, idx);
    }
}

bool nimcp_gpu_shortest_path(
    nimcp_gpu_knowledge_graph_t* graph,
    uint32_t source,
    uint32_t target,
    uint32_t* path_out,
    uint32_t* path_length)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !path_out || !path_length) {
        LOG_ERROR("Invalid parameters for shortest_path");
        return false;
    }

    if (source >= graph->num_nodes || target >= graph->num_nodes) {
        LOG_ERROR("Source or target node out of range");
        return false;
    }

    // Special case: source == target
    if (source == target) {
        path_out[0] = source;
        *path_length = 1;
        return true;
    }

    // Run BFS from source
    nimcp_graph_traversal_result_t result;
    memset(&result, 0, sizeof(result));

    if (!nimcp_gpu_bfs(graph, source, -1, &result)) {
        return false;
    }

    // Check if target was reached
    uint32_t* h_distances = (uint32_t*)nimcp_malloc(graph->num_nodes * sizeof(uint32_t));
    uint32_t* h_parents = (uint32_t*)nimcp_malloc(graph->num_nodes * sizeof(uint32_t));

    if (!h_distances || !h_parents) {
        nimcp_free(h_distances);
        nimcp_free(h_parents);
        nimcp_graph_traversal_result_destroy(&result);
        return false;
    }

    cudaMemcpy(h_distances, result.distances->data, graph->num_nodes * sizeof(uint32_t), cudaMemcpyDeviceToHost);
    cudaMemcpy(h_parents, result.parents->data, graph->num_nodes * sizeof(uint32_t), cudaMemcpyDeviceToHost);

    if (h_distances[target] == INVALID_NODE) {
        // No path exists
        nimcp_free(h_distances);
        nimcp_free(h_parents);
        nimcp_graph_traversal_result_destroy(&result);
        *path_length = 0;
        return false;
    }

    // Reconstruct path
    uint32_t len = h_distances[target] + 1;
    *path_length = len;

    // Build path backwards
    uint32_t current = target;
    for (int i = len - 1; i >= 0; i--) {
        path_out[i] = current;
        current = h_parents[current];
    }

    nimcp_free(h_distances);
    nimcp_free(h_parents);
    nimcp_graph_traversal_result_destroy(&result);

    return true;
}

void nimcp_graph_traversal_result_destroy(nimcp_graph_traversal_result_t* result)
{
    if (!result) return;

    nimcp_gpu_tensor_destroy(result->distances);
    nimcp_gpu_tensor_destroy(result->parents);
    nimcp_gpu_tensor_destroy(result->visited);

    result->distances = NULL;
    result->parents = NULL;
    result->visited = NULL;
    result->num_visited = 0;
    result->max_depth = 0;
}

//=============================================================================
// Cosine Similarity Kernels
//=============================================================================

/**
 * @brief Compute cosine similarity between two embedding vectors
 */
__global__ void kernel_cosine_similarity_single(
    const float* embeddings,
    uint32_t node_a,
    uint32_t node_b,
    uint32_t embed_dim,
    float* similarity)
{
    __shared__ float dot_prod[WARP_SIZE];
    __shared__ float norm_a[WARP_SIZE];
    __shared__ float norm_b[WARP_SIZE];

    float local_dot = 0.0f;
    float local_norm_a = 0.0f;
    float local_norm_b = 0.0f;

    const float* vec_a = embeddings + node_a * embed_dim;
    const float* vec_b = embeddings + node_b * embed_dim;

    for (uint32_t i = threadIdx.x; i < embed_dim; i += blockDim.x) {
        float a = vec_a[i];
        float b = vec_b[i];
        local_dot += a * b;
        local_norm_a += a * a;
        local_norm_b += b * b;
    }

    // Warp reduction
    local_dot = warp_reduce_sum(local_dot);
    local_norm_a = warp_reduce_sum(local_norm_a);
    local_norm_b = warp_reduce_sum(local_norm_b);

    if (threadIdx.x % WARP_SIZE == 0) {
        dot_prod[threadIdx.x / WARP_SIZE] = local_dot;
        norm_a[threadIdx.x / WARP_SIZE] = local_norm_a;
        norm_b[threadIdx.x / WARP_SIZE] = local_norm_b;
    }
    __syncthreads();

    if (threadIdx.x == 0) {
        float total_dot = 0.0f;
        float total_norm_a = 0.0f;
        float total_norm_b = 0.0f;
        int num_warps = (blockDim.x + WARP_SIZE - 1) / WARP_SIZE;

        for (int i = 0; i < num_warps; i++) {
            total_dot += dot_prod[i];
            total_norm_a += norm_a[i];
            total_norm_b += norm_b[i];
        }

        float denom = sqrtf(total_norm_a) * sqrtf(total_norm_b);
        *similarity = (denom > 1e-8f) ? total_dot / denom : 0.0f;
    }
}

/**
 * @brief Compute pairwise cosine similarity matrix
 */
__global__ void kernel_pairwise_similarity(
    const float* embeddings,
    const uint32_t* node_indices,
    uint32_t num_nodes,
    uint32_t embed_dim,
    float* similarity_matrix)
{
    uint32_t i = blockIdx.y;
    uint32_t j = blockIdx.x * blockDim.x + threadIdx.x;

    if (i >= num_nodes || j >= num_nodes) return;

    uint32_t node_a = node_indices[i];
    uint32_t node_b = node_indices[j];

    const float* vec_a = embeddings + node_a * embed_dim;
    const float* vec_b = embeddings + node_b * embed_dim;

    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (uint32_t d = 0; d < embed_dim; d++) {
        float a = vec_a[d];
        float b = vec_b[d];
        dot += a * b;
        norm_a += a * a;
        norm_b += b * b;
    }

    float denom = sqrtf(norm_a) * sqrtf(norm_b);
    similarity_matrix[i * num_nodes + j] = (denom > 1e-8f) ? dot / denom : 0.0f;
}

bool nimcp_gpu_node_similarity(
    nimcp_gpu_knowledge_graph_t* graph,
    uint32_t node_a,
    uint32_t node_b,
    float* similarity)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !similarity) {
        LOG_ERROR("Invalid parameters for node_similarity");
        return false;
    }

    if (!graph->node_embeddings) {
        LOG_ERROR("Graph has no embeddings");
        return false;
    }

    if (node_a >= graph->num_nodes || node_b >= graph->num_nodes) {
        LOG_ERROR("Node index out of range");
        return false;
    }

    float* d_similarity = NULL;
    bool success = false;

    if (cudaMalloc(&d_similarity, sizeof(float)) != cudaSuccess) goto node_sim_cleanup;

    kernel_cosine_similarity_single<<<1, BLOCK_SIZE>>>(
        (const float*)graph->node_embeddings->data,
        node_a,
        node_b,
        graph->embed_dim,
        d_similarity);
    if (cudaGetLastError() != cudaSuccess) goto node_sim_cleanup;

    if (cudaMemcpy(similarity, d_similarity, sizeof(float), cudaMemcpyDeviceToHost) != cudaSuccess) goto node_sim_cleanup;

    success = true;

node_sim_cleanup:
    cudaFree(d_similarity);

    return success;
}

bool nimcp_gpu_pairwise_similarity(
    nimcp_gpu_knowledge_graph_t* graph,
    const uint32_t* node_indices,
    uint32_t num_nodes,
    nimcp_gpu_tensor_t* similarity_matrix)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !node_indices || !similarity_matrix) {
        LOG_ERROR("Invalid parameters for pairwise_similarity");
        return false;
    }

    if (!graph->node_embeddings) {
        LOG_ERROR("Graph has no embeddings");
        return false;
    }

    // Copy node indices to device
    uint32_t* d_indices = NULL;
    bool success = false;

    if (cudaMalloc(&d_indices, num_nodes * sizeof(uint32_t)) != cudaSuccess) goto pairwise_sim_cleanup;
    if (cudaMemcpy(d_indices, node_indices, num_nodes * sizeof(uint32_t), cudaMemcpyHostToDevice) != cudaSuccess) goto pairwise_sim_cleanup;

    {
        dim3 block(32);
        dim3 grid((num_nodes + 31) / 32, num_nodes);

        kernel_pairwise_similarity<<<grid, block>>>(
            (const float*)graph->node_embeddings->data,
            d_indices,
            num_nodes,
            graph->embed_dim,
            (float*)similarity_matrix->data);
        if (cudaGetLastError() != cudaSuccess) goto pairwise_sim_cleanup;
    }

    success = true;

pairwise_sim_cleanup:
    cudaFree(d_indices);
    return success;
}

//=============================================================================
// KNN Similarity Search
//=============================================================================

/**
 * @brief Compute similarities for all nodes to query
 */
__global__ void kernel_compute_all_similarities(
    const float* embeddings,
    const float* query_embedding,
    uint32_t num_nodes,
    uint32_t embed_dim,
    float* similarities)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_nodes) return;

    const float* node_emb = embeddings + idx * embed_dim;

    float dot = 0.0f;
    float norm_node = 0.0f;
    float norm_query = 0.0f;

    for (uint32_t d = 0; d < embed_dim; d++) {
        float n = node_emb[d];
        float q = query_embedding[d];
        dot += n * q;
        norm_node += n * n;
        norm_query += q * q;
    }

    float denom = sqrtf(norm_node) * sqrtf(norm_query);
    similarities[idx] = (denom > 1e-8f) ? dot / denom : 0.0f;
}

/**
 * @brief Simple top-k selection (for small k)
 */
__global__ void kernel_top_k_select(
    const float* similarities,
    uint32_t num_nodes,
    uint32_t k,
    uint32_t* top_indices,
    float* top_scores)
{
    // Initialize with worst values
    for (uint32_t i = threadIdx.x; i < k; i += blockDim.x) {
        top_indices[i] = 0;
        top_scores[i] = -FLT_MAX;
    }
    __syncthreads();

    if (threadIdx.x != 0) return;

    // Simple sequential top-k for now
    for (uint32_t n = 0; n < num_nodes; n++) {
        float score = similarities[n];

        // Find insertion point
        uint32_t insert_pos = k;
        for (uint32_t i = 0; i < k; i++) {
            if (score > top_scores[i]) {
                insert_pos = i;
                break;
            }
        }

        if (insert_pos < k) {
            // Shift elements down
            for (uint32_t i = k - 1; i > insert_pos; i--) {
                top_indices[i] = top_indices[i - 1];
                top_scores[i] = top_scores[i - 1];
            }
            top_indices[insert_pos] = n;
            top_scores[insert_pos] = score;
        }
    }
}

bool nimcp_gpu_knn_similarity(
    nimcp_gpu_knowledge_graph_t* graph,
    uint32_t query_node,
    uint32_t k,
    nimcp_similarity_result_t* result)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !result) {
        LOG_ERROR("Invalid parameters for knn_similarity");
        return false;
    }

    if (!graph->node_embeddings) {
        LOG_ERROR("Graph has no embeddings");
        return false;
    }

    if (query_node >= graph->num_nodes) {
        LOG_ERROR("Query node out of range");
        return false;
    }

    k = (k > graph->num_nodes) ? graph->num_nodes : k;

    // Get query embedding
    const float* query_emb = (const float*)graph->node_embeddings->data + query_node * graph->embed_dim;

    // Allocate similarity buffer
    float* d_similarities = NULL;
    bool success = false;

    if (cudaMalloc(&d_similarities, graph->num_nodes * sizeof(float)) != cudaSuccess) goto knn_sim_cleanup;

    // Compute similarities
    kernel_compute_all_similarities<<<GRID_SIZE(graph->num_nodes), BLOCK_SIZE>>>(
        (const float*)graph->node_embeddings->data,
        query_emb,
        graph->num_nodes,
        graph->embed_dim,
        d_similarities);
    if (cudaGetLastError() != cudaSuccess) goto knn_sim_cleanup;

    {
        // Create result tensors
        size_t k_dims[] = {k};
        result->indices = nimcp_gpu_tensor_create(graph->ctx, k_dims, 1, NIMCP_GPU_PRECISION_UINT32);
        result->scores = nimcp_gpu_tensor_create(graph->ctx, k_dims, 1, NIMCP_GPU_PRECISION_FP32);
        result->k = k;

        if (!result->indices || !result->scores) {
            cudaFree(d_similarities);
            nimcp_similarity_result_destroy(result);
            return false;
        }

        // Top-k selection
        kernel_top_k_select<<<1, 1>>>(
            d_similarities,
            graph->num_nodes,
            k,
            (uint32_t*)result->indices->data,
            (float*)result->scores->data);
        if (cudaGetLastError() != cudaSuccess) goto knn_sim_cleanup;
    }

    success = true;

knn_sim_cleanup:
    cudaFree(d_similarities);
    return success;
}

bool nimcp_gpu_knn_similarity_embedding(
    nimcp_gpu_knowledge_graph_t* graph,
    const float* query_embedding,
    uint32_t k,
    nimcp_similarity_result_t* result)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !query_embedding || !result) {
        LOG_ERROR("Invalid parameters for knn_similarity_embedding");
        return false;
    }

    if (!graph->node_embeddings) {
        LOG_ERROR("Graph has no embeddings");
        return false;
    }

    k = (k > graph->num_nodes) ? graph->num_nodes : k;

    // Copy query embedding to device
    float* d_query = NULL;
    float* d_similarities = NULL;
    bool success = false;

    if (cudaMalloc(&d_query, graph->embed_dim * sizeof(float)) != cudaSuccess) goto knn_emb_cleanup;
    if (cudaMemcpy(d_query, query_embedding, graph->embed_dim * sizeof(float), cudaMemcpyHostToDevice) != cudaSuccess) goto knn_emb_cleanup;

    // Allocate similarity buffer
    if (cudaMalloc(&d_similarities, graph->num_nodes * sizeof(float)) != cudaSuccess) goto knn_emb_cleanup;

    // Compute similarities
    kernel_compute_all_similarities<<<GRID_SIZE(graph->num_nodes), BLOCK_SIZE>>>(
        (const float*)graph->node_embeddings->data,
        d_query,
        graph->num_nodes,
        graph->embed_dim,
        d_similarities);
    if (cudaGetLastError() != cudaSuccess) goto knn_emb_cleanup;

    {
        // Create result tensors
        size_t k_dims[] = {k};
        result->indices = nimcp_gpu_tensor_create(graph->ctx, k_dims, 1, NIMCP_GPU_PRECISION_UINT32);
        result->scores = nimcp_gpu_tensor_create(graph->ctx, k_dims, 1, NIMCP_GPU_PRECISION_FP32);
        result->k = k;

        if (!result->indices || !result->scores) {
            cudaFree(d_query);
            cudaFree(d_similarities);
            nimcp_similarity_result_destroy(result);
            return false;
        }

        // Top-k selection
        kernel_top_k_select<<<1, 1>>>(
            d_similarities,
            graph->num_nodes,
            k,
            (uint32_t*)result->indices->data,
            (float*)result->scores->data);
        if (cudaGetLastError() != cudaSuccess) goto knn_emb_cleanup;
    }

    success = true;

knn_emb_cleanup:
    cudaFree(d_query);
    cudaFree(d_similarities);
    return success;
}

void nimcp_similarity_result_destroy(nimcp_similarity_result_t* result)
{
    if (!result) return;

    nimcp_gpu_tensor_destroy(result->indices);
    nimcp_gpu_tensor_destroy(result->scores);

    result->indices = NULL;
    result->scores = NULL;
    result->k = 0;
}

//=============================================================================
// Embedding Update Operations
//=============================================================================

/**
 * @brief Apply gradient descent update to embeddings
 */
__global__ void kernel_embedding_update(
    float* embeddings,
    const float* gradients,
    float learning_rate,
    size_t numel)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= numel) return;

    embeddings[idx] -= learning_rate * gradients[idx];
}

bool nimcp_gpu_embedding_update(
    nimcp_gpu_knowledge_graph_t* graph,
    const nimcp_gpu_tensor_t* gradients,
    float learning_rate)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !gradients) {
        LOG_ERROR("Invalid parameters for embedding_update");
        return false;
    }

    if (!graph->node_embeddings) {
        LOG_ERROR("Graph has no embeddings");
        return false;
    }

    size_t numel = graph->num_nodes * graph->embed_dim;

    kernel_embedding_update<<<GRID_SIZE(numel), BLOCK_SIZE>>>(
        (float*)graph->node_embeddings->data,
        (const float*)gradients->data,
        learning_rate,
        numel);
    CUDA_CHECK(cudaGetLastError());

    return true;
}

/**
 * @brief Compute triplet loss for knowledge graph embeddings
 */
__global__ void kernel_triplet_loss(
    const float* embeddings,
    const uint32_t* anchors,
    const uint32_t* positives,
    const uint32_t* negatives,
    uint32_t batch_size,
    uint32_t embed_dim,
    float margin,
    float* losses)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= batch_size) return;

    uint32_t anchor = anchors[idx];
    uint32_t positive = positives[idx];
    uint32_t negative = negatives[idx];

    const float* a = embeddings + anchor * embed_dim;
    const float* p = embeddings + positive * embed_dim;
    const float* n = embeddings + negative * embed_dim;

    // Compute distances
    float dist_pos = 0.0f;
    float dist_neg = 0.0f;

    for (uint32_t d = 0; d < embed_dim; d++) {
        float diff_p = a[d] - p[d];
        float diff_n = a[d] - n[d];
        dist_pos += diff_p * diff_p;
        dist_neg += diff_n * diff_n;
    }

    dist_pos = sqrtf(dist_pos);
    dist_neg = sqrtf(dist_neg);

    // Triplet loss: max(0, margin + d_pos - d_neg)
    losses[idx] = fmaxf(0.0f, margin + dist_pos - dist_neg);
}

bool nimcp_gpu_triplet_loss(
    nimcp_gpu_knowledge_graph_t* graph,
    const uint32_t* anchors,
    const uint32_t* positives,
    const uint32_t* negatives,
    uint32_t batch_size,
    float margin,
    float* loss_out)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !anchors || !positives || !negatives || !loss_out) {
        LOG_ERROR("Invalid parameters for triplet_loss");
        return false;
    }

    if (!graph->node_embeddings) {
        LOG_ERROR("Graph has no embeddings");
        return false;
    }

    // Copy indices to device
    uint32_t* d_anchors = NULL;
    uint32_t* d_positives = NULL;
    uint32_t* d_negatives = NULL;
    float* d_losses = NULL;
    float* h_losses = NULL;
    bool success = false;

    if (cudaMalloc(&d_anchors, batch_size * sizeof(uint32_t)) != cudaSuccess) goto triplet_cleanup;
    if (cudaMalloc(&d_positives, batch_size * sizeof(uint32_t)) != cudaSuccess) goto triplet_cleanup;
    if (cudaMalloc(&d_negatives, batch_size * sizeof(uint32_t)) != cudaSuccess) goto triplet_cleanup;
    if (cudaMalloc(&d_losses, batch_size * sizeof(float)) != cudaSuccess) goto triplet_cleanup;

    if (cudaMemcpy(d_anchors, anchors, batch_size * sizeof(uint32_t), cudaMemcpyHostToDevice) != cudaSuccess) goto triplet_cleanup;
    if (cudaMemcpy(d_positives, positives, batch_size * sizeof(uint32_t), cudaMemcpyHostToDevice) != cudaSuccess) goto triplet_cleanup;
    if (cudaMemcpy(d_negatives, negatives, batch_size * sizeof(uint32_t), cudaMemcpyHostToDevice) != cudaSuccess) goto triplet_cleanup;

    kernel_triplet_loss<<<GRID_SIZE(batch_size), BLOCK_SIZE>>>(
        (const float*)graph->node_embeddings->data,
        d_anchors,
        d_positives,
        d_negatives,
        batch_size,
        graph->embed_dim,
        margin,
        d_losses);
    if (cudaGetLastError() != cudaSuccess) goto triplet_cleanup;

    // Sum losses on host
    h_losses = (float*)nimcp_malloc(batch_size * sizeof(float));
    if (!h_losses) goto triplet_cleanup;
    if (cudaMemcpy(h_losses, d_losses, batch_size * sizeof(float), cudaMemcpyDeviceToHost) != cudaSuccess) goto triplet_cleanup;

    {
        float total_loss = 0.0f;
        for (uint32_t i = 0; i < batch_size; i++) {
            total_loss += h_losses[i];
        }
        *loss_out = total_loss / batch_size;
    }

    success = true;

triplet_cleanup:
    nimcp_free(h_losses);
    cudaFree(d_anchors);
    cudaFree(d_positives);
    cudaFree(d_negatives);
    cudaFree(d_losses);

    return success;
}

/**
 * @brief L2 normalize embeddings
 */
__global__ void kernel_normalize_embeddings(
    float* embeddings,
    uint32_t num_nodes,
    uint32_t embed_dim)
{
    uint32_t node = blockIdx.x;
    if (node >= num_nodes) return;

    float* emb = embeddings + node * embed_dim;

    // Compute norm
    __shared__ float norm_shared[8];
    float local_norm = 0.0f;

    for (uint32_t d = threadIdx.x; d < embed_dim; d += blockDim.x) {
        local_norm += emb[d] * emb[d];
    }

    local_norm = warp_reduce_sum(local_norm);
    if (threadIdx.x % WARP_SIZE == 0) {
        norm_shared[threadIdx.x / WARP_SIZE] = local_norm;
    }
    __syncthreads();

    if (threadIdx.x == 0) {
        float total = 0.0f;
        int num_warps = (blockDim.x + WARP_SIZE - 1) / WARP_SIZE;
        for (int i = 0; i < num_warps; i++) {
            total += norm_shared[i];
        }
        norm_shared[0] = sqrtf(total);
    }
    __syncthreads();

    float norm = norm_shared[0];
    if (norm > 1e-8f) {
        for (uint32_t d = threadIdx.x; d < embed_dim; d += blockDim.x) {
            emb[d] /= norm;
        }
    }
}

bool nimcp_gpu_normalize_embeddings(nimcp_gpu_knowledge_graph_t* graph)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph) {
        LOG_ERROR("Invalid graph parameter");
        return false;
    }

    if (!graph->node_embeddings) {
        LOG_ERROR("Graph has no embeddings");
        return false;
    }

    kernel_normalize_embeddings<<<graph->num_nodes, BLOCK_SIZE>>>(
        (float*)graph->node_embeddings->data,
        graph->num_nodes,
        graph->embed_dim);
    CUDA_CHECK(cudaGetLastError());

    return true;
}

//=============================================================================
// Feature Aggregation
//=============================================================================

/**
 * @brief Aggregate neighbor features (sum mode)
 */
__global__ void kernel_aggregate_sum(
    const float* embeddings,
    const uint32_t* row_offsets,
    const uint32_t* col_indices,
    float* output,
    uint32_t num_nodes,
    uint32_t embed_dim)
{
    uint32_t node = blockIdx.x;
    if (node >= num_nodes) return;

    float* out = output + node * embed_dim;

    uint32_t row_start = row_offsets[node];
    uint32_t row_end = row_offsets[node + 1];

    // Initialize output to zero
    for (uint32_t d = threadIdx.x; d < embed_dim; d += blockDim.x) {
        out[d] = 0.0f;
    }
    __syncthreads();

    // Sum neighbor embeddings
    for (uint32_t e = row_start; e < row_end; e++) {
        uint32_t neighbor = col_indices[e];
        const float* neigh_emb = embeddings + neighbor * embed_dim;

        for (uint32_t d = threadIdx.x; d < embed_dim; d += blockDim.x) {
            atomicAdd(&out[d], neigh_emb[d]);
        }
    }
}

/**
 * @brief Aggregate neighbor features (mean mode)
 */
__global__ void kernel_aggregate_mean(
    const float* embeddings,
    const uint32_t* row_offsets,
    const uint32_t* col_indices,
    float* output,
    uint32_t num_nodes,
    uint32_t embed_dim)
{
    uint32_t node = blockIdx.x;
    if (node >= num_nodes) return;

    float* out = output + node * embed_dim;

    uint32_t row_start = row_offsets[node];
    uint32_t row_end = row_offsets[node + 1];
    uint32_t degree = row_end - row_start;

    if (degree == 0) {
        // No neighbors - copy self embedding
        const float* self_emb = embeddings + node * embed_dim;
        for (uint32_t d = threadIdx.x; d < embed_dim; d += blockDim.x) {
            out[d] = self_emb[d];
        }
        return;
    }

    // Initialize output to zero
    for (uint32_t d = threadIdx.x; d < embed_dim; d += blockDim.x) {
        out[d] = 0.0f;
    }
    __syncthreads();

    // Sum neighbor embeddings
    for (uint32_t e = row_start; e < row_end; e++) {
        uint32_t neighbor = col_indices[e];
        const float* neigh_emb = embeddings + neighbor * embed_dim;

        for (uint32_t d = threadIdx.x; d < embed_dim; d += blockDim.x) {
            atomicAdd(&out[d], neigh_emb[d]);
        }
    }
    __syncthreads();

    // Divide by degree
    float inv_degree = 1.0f / degree;
    for (uint32_t d = threadIdx.x; d < embed_dim; d += blockDim.x) {
        out[d] *= inv_degree;
    }
}

/**
 * @brief Aggregate neighbor features (max mode)
 */
__global__ void kernel_aggregate_max(
    const float* embeddings,
    const uint32_t* row_offsets,
    const uint32_t* col_indices,
    float* output,
    uint32_t num_nodes,
    uint32_t embed_dim)
{
    uint32_t node = blockIdx.x;
    if (node >= num_nodes) return;

    float* out = output + node * embed_dim;

    uint32_t row_start = row_offsets[node];
    uint32_t row_end = row_offsets[node + 1];

    if (row_end == row_start) {
        const float* self_emb = embeddings + node * embed_dim;
        for (uint32_t d = threadIdx.x; d < embed_dim; d += blockDim.x) {
            out[d] = self_emb[d];
        }
        return;
    }

    // Initialize with first neighbor
    const float* first_neigh = embeddings + col_indices[row_start] * embed_dim;
    for (uint32_t d = threadIdx.x; d < embed_dim; d += blockDim.x) {
        out[d] = first_neigh[d];
    }
    __syncthreads();

    // Max over remaining neighbors
    for (uint32_t e = row_start + 1; e < row_end; e++) {
        uint32_t neighbor = col_indices[e];
        const float* neigh_emb = embeddings + neighbor * embed_dim;

        for (uint32_t d = threadIdx.x; d < embed_dim; d += blockDim.x) {
            out[d] = fmaxf(out[d], neigh_emb[d]);
        }
        __syncthreads();
    }
}

bool nimcp_gpu_aggregate_neighbors(
    nimcp_gpu_knowledge_graph_t* graph,
    nimcp_aggregate_mode_t mode,
    nimcp_gpu_tensor_t* output)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !output) {
        LOG_ERROR("Invalid parameters for aggregate_neighbors");
        return false;
    }

    if (!graph->node_embeddings) {
        LOG_ERROR("Graph has no embeddings");
        return false;
    }

    switch (mode) {
        case NIMCP_AGGREGATE_SUM:
            kernel_aggregate_sum<<<graph->num_nodes, BLOCK_SIZE>>>(
                (const float*)graph->node_embeddings->data,
                (const uint32_t*)graph->row_offsets->data,
                (const uint32_t*)graph->col_indices->data,
                (float*)output->data,
                graph->num_nodes,
                graph->embed_dim);
            break;

        case NIMCP_AGGREGATE_MEAN:
            kernel_aggregate_mean<<<graph->num_nodes, BLOCK_SIZE>>>(
                (const float*)graph->node_embeddings->data,
                (const uint32_t*)graph->row_offsets->data,
                (const uint32_t*)graph->col_indices->data,
                (float*)output->data,
                graph->num_nodes,
                graph->embed_dim);
            break;

        case NIMCP_AGGREGATE_MAX:
            kernel_aggregate_max<<<graph->num_nodes, BLOCK_SIZE>>>(
                (const float*)graph->node_embeddings->data,
                (const uint32_t*)graph->row_offsets->data,
                (const uint32_t*)graph->col_indices->data,
                (float*)output->data,
                graph->num_nodes,
                graph->embed_dim);
            break;

        case NIMCP_AGGREGATE_ATTENTION:
            // Fall through to attention function
            LOG_WARN("Use nimcp_gpu_aggregate_attention for attention aggregation");
            return false;

        default:
            LOG_ERROR("Unknown aggregation mode: %d", mode);
            return false;
    }

    CUDA_CHECK(cudaGetLastError());
    return true;
}

/**
 * @brief Attention-weighted aggregation kernel
 */
__global__ void kernel_aggregate_attention(
    const float* embeddings,
    const uint32_t* row_offsets,
    const uint32_t* col_indices,
    const float* query_weights,
    const float* key_weights,
    float* output,
    uint32_t num_nodes,
    uint32_t embed_dim,
    uint32_t attn_dim)
{
    extern __shared__ float shared_mem[];

    uint32_t node = blockIdx.x;
    if (node >= num_nodes) return;

    float* out = output + node * embed_dim;
    const float* node_emb = embeddings + node * embed_dim;

    uint32_t row_start = row_offsets[node];
    uint32_t row_end = row_offsets[node + 1];
    uint32_t degree = row_end - row_start;

    if (degree == 0) {
        for (uint32_t d = threadIdx.x; d < embed_dim; d += blockDim.x) {
            out[d] = node_emb[d];
        }
        return;
    }

    // Compute query: q = W_q * node_emb
    float* query = shared_mem;
    float* attn_scores = shared_mem + attn_dim;

    // Simple query projection (thread 0 only for simplicity)
    if (threadIdx.x == 0) {
        for (uint32_t a = 0; a < attn_dim; a++) {
            float sum = 0.0f;
            for (uint32_t d = 0; d < embed_dim; d++) {
                sum += query_weights[a * embed_dim + d] * node_emb[d];
            }
            query[a] = sum;
        }
    }
    __syncthreads();

    // Compute attention scores
    if (threadIdx.x < degree) {
        uint32_t neighbor = col_indices[row_start + threadIdx.x];
        const float* neigh_emb = embeddings + neighbor * embed_dim;

        // Compute key: k = W_k * neigh_emb
        float score = 0.0f;
        for (uint32_t a = 0; a < attn_dim; a++) {
            float key_val = 0.0f;
            for (uint32_t d = 0; d < embed_dim; d++) {
                key_val += key_weights[a * embed_dim + d] * neigh_emb[d];
            }
            score += query[a] * key_val;
        }
        attn_scores[threadIdx.x] = score / sqrtf((float)attn_dim);
    }
    __syncthreads();

    // Softmax over attention scores
    if (threadIdx.x == 0) {
        float max_score = -FLT_MAX;
        for (uint32_t i = 0; i < degree; i++) {
            max_score = fmaxf(max_score, attn_scores[i]);
        }
        float sum_exp = 0.0f;
        for (uint32_t i = 0; i < degree; i++) {
            attn_scores[i] = expf(attn_scores[i] - max_score);
            sum_exp += attn_scores[i];
        }
        for (uint32_t i = 0; i < degree; i++) {
            attn_scores[i] /= sum_exp;
        }
    }
    __syncthreads();

    // Weighted aggregation
    for (uint32_t d = threadIdx.x; d < embed_dim; d += blockDim.x) {
        float sum = 0.0f;
        for (uint32_t i = 0; i < degree; i++) {
            uint32_t neighbor = col_indices[row_start + i];
            sum += attn_scores[i] * embeddings[neighbor * embed_dim + d];
        }
        out[d] = sum;
    }
}

bool nimcp_gpu_aggregate_attention(
    nimcp_gpu_knowledge_graph_t* graph,
    const nimcp_gpu_tensor_t* query_weights,
    const nimcp_gpu_tensor_t* key_weights,
    nimcp_gpu_tensor_t* output)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !query_weights || !key_weights || !output) {
        LOG_ERROR("Invalid parameters for aggregate_attention");
        return false;
    }

    if (!graph->node_embeddings) {
        LOG_ERROR("Graph has no embeddings");
        return false;
    }

    uint32_t attn_dim = query_weights->dims[0];
    size_t shared_size = (attn_dim + graph->num_nodes) * sizeof(float);

    if (shared_size > MAX_SHARED_MEM) {
        LOG_ERROR("Attention dimension too large for shared memory");
        return false;
    }

    kernel_aggregate_attention<<<graph->num_nodes, BLOCK_SIZE, shared_size>>>(
        (const float*)graph->node_embeddings->data,
        (const uint32_t*)graph->row_offsets->data,
        (const uint32_t*)graph->col_indices->data,
        (const float*)query_weights->data,
        (const float*)key_weights->data,
        (float*)output->data,
        graph->num_nodes,
        graph->embed_dim,
        attn_dim);
    CUDA_CHECK(cudaGetLastError());

    return true;
}

bool nimcp_gpu_multi_hop_aggregate(
    nimcp_gpu_knowledge_graph_t* graph,
    uint32_t num_hops,
    nimcp_aggregate_mode_t mode,
    nimcp_gpu_tensor_t* output)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !output || num_hops == 0) {
        LOG_ERROR("Invalid parameters for multi_hop_aggregate");
        return false;
    }

    size_t embed_dims[] = {graph->num_nodes, graph->embed_dim};

    // Create temporary buffer
    nimcp_gpu_tensor_t* temp = nimcp_gpu_tensor_create(graph->ctx, embed_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!temp) {
        return false;
    }

    // Copy initial embeddings to output
    size_t data_size = graph->num_nodes * graph->embed_dim * sizeof(float);
    cudaMemcpy(output->data, graph->node_embeddings->data, data_size, cudaMemcpyDeviceToDevice);

    // Iteratively aggregate
    for (uint32_t hop = 0; hop < num_hops; hop++) {
        // Swap output and temp
        nimcp_gpu_tensor_t* current_input = (hop % 2 == 0) ? output : temp;
        nimcp_gpu_tensor_t* current_output = (hop % 2 == 0) ? temp : output;

        // Temporarily swap graph embeddings
        nimcp_gpu_tensor_t* saved_embeddings = graph->node_embeddings;
        graph->node_embeddings = current_input;

        if (!nimcp_gpu_aggregate_neighbors(graph, mode, current_output)) {
            graph->node_embeddings = saved_embeddings;
            nimcp_gpu_tensor_destroy(temp);
            return false;
        }

        graph->node_embeddings = saved_embeddings;
    }

    // Ensure result is in output
    if (num_hops % 2 == 1) {
        cudaMemcpy(output->data, temp->data, data_size, cudaMemcpyDeviceToDevice);
    }

    nimcp_gpu_tensor_destroy(temp);
    return true;
}

//=============================================================================
// Subgraph Matching
//=============================================================================

bool nimcp_gpu_subgraph_match(
    nimcp_gpu_knowledge_graph_t* graph,
    nimcp_gpu_knowledge_graph_t* pattern,
    uint32_t max_matches,
    nimcp_subgraph_match_result_t* result)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !pattern || !result) {
        LOG_ERROR("Invalid parameters for subgraph_match");
        return false;
    }

    // Simple brute-force matching for small patterns
    // TODO: Implement VF2 or GraphQL algorithm for better performance

    size_t mapping_dims[] = {max_matches, pattern->num_nodes};
    size_t score_dims[] = {max_matches};

    result->mappings = nimcp_gpu_tensor_create(graph->ctx, mapping_dims, 2, NIMCP_GPU_PRECISION_UINT32);
    result->scores = nimcp_gpu_tensor_create(graph->ctx, score_dims, 1, NIMCP_GPU_PRECISION_FP32);
    result->num_matches = 0;

    if (!result->mappings || !result->scores) {
        nimcp_subgraph_match_result_destroy(result);
        return false;
    }

    // For now, return empty result - full implementation would require
    // complex parallel subgraph isomorphism algorithm
    LOG_WARN("Subgraph matching not fully implemented - returning empty result");

    return true;
}

bool nimcp_gpu_subgraph_match_approximate(
    nimcp_gpu_knowledge_graph_t* graph,
    nimcp_gpu_knowledge_graph_t* pattern,
    float similarity_threshold,
    uint32_t max_matches,
    nimcp_subgraph_match_result_t* result)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !pattern || !result) {
        LOG_ERROR("Invalid parameters for approximate subgraph_match");
        return false;
    }

    if (!graph->node_embeddings || !pattern->node_embeddings) {
        LOG_ERROR("Both graphs must have embeddings for approximate matching");
        return false;
    }

    size_t mapping_dims[] = {max_matches, pattern->num_nodes};
    size_t score_dims[] = {max_matches};

    result->mappings = nimcp_gpu_tensor_create(graph->ctx, mapping_dims, 2, NIMCP_GPU_PRECISION_UINT32);
    result->scores = nimcp_gpu_tensor_create(graph->ctx, score_dims, 1, NIMCP_GPU_PRECISION_FP32);
    result->num_matches = 0;

    if (!result->mappings || !result->scores) {
        nimcp_subgraph_match_result_destroy(result);
        return false;
    }

    // TODO: Implement embedding-based approximate matching
    LOG_WARN("Approximate subgraph matching not fully implemented");

    return true;
}

void nimcp_subgraph_match_result_destroy(nimcp_subgraph_match_result_t* result)
{
    if (!result) return;

    nimcp_gpu_tensor_destroy(result->mappings);
    nimcp_gpu_tensor_destroy(result->scores);

    result->mappings = NULL;
    result->scores = NULL;
    result->num_matches = 0;
}

//=============================================================================
// Hyperbolic Space Operations
//=============================================================================

/**
 * @brief Compute hyperbolic distance in Poincare ball model
 *
 * d(u,v) = arcosh(1 + 2 * ||u-v||^2 / ((1-||u||^2)(1-||v||^2)))
 */
__device__ inline float device_hyperbolic_distance(
    const float* u,
    const float* v,
    uint32_t dim)
{
    float diff_sq = 0.0f;
    float norm_u_sq = 0.0f;
    float norm_v_sq = 0.0f;

    for (uint32_t d = 0; d < dim; d++) {
        float diff = u[d] - v[d];
        diff_sq += diff * diff;
        norm_u_sq += u[d] * u[d];
        norm_v_sq += v[d] * v[d];
    }

    // Clamp norms to stay inside Poincare ball
    norm_u_sq = fminf(norm_u_sq, 1.0f - 1e-5f);
    norm_v_sq = fminf(norm_v_sq, 1.0f - 1e-5f);

    float numerator = 2.0f * diff_sq;
    float denominator = (1.0f - norm_u_sq) * (1.0f - norm_v_sq);

    float arg = 1.0f + numerator / denominator;
    return acoshf(fmaxf(arg, 1.0f));
}

__global__ void kernel_hyperbolic_distance_single(
    const float* embeddings,
    uint32_t node_a,
    uint32_t node_b,
    uint32_t embed_dim,
    float* distance)
{
    if (threadIdx.x != 0) return;

    const float* u = embeddings + node_a * embed_dim;
    const float* v = embeddings + node_b * embed_dim;

    *distance = device_hyperbolic_distance(u, v, embed_dim);
}

bool nimcp_gpu_hyperbolic_distance(
    nimcp_gpu_knowledge_graph_t* graph,
    uint32_t node_a,
    uint32_t node_b,
    float* distance)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !distance) {
        LOG_ERROR("Invalid parameters for hyperbolic_distance");
        return false;
    }

    if (!graph->is_hyperbolic) {
        LOG_ERROR("Graph embeddings are not in hyperbolic space");
        return false;
    }

    if (!graph->node_embeddings) {
        LOG_ERROR("Graph has no embeddings");
        return false;
    }

    if (node_a >= graph->num_nodes || node_b >= graph->num_nodes) {
        LOG_ERROR("Node index out of range");
        return false;
    }

    float* d_distance = NULL;
    bool success = false;

    if (cudaMalloc(&d_distance, sizeof(float)) != cudaSuccess) goto hyp_dist_cleanup;

    kernel_hyperbolic_distance_single<<<1, 1>>>(
        (const float*)graph->node_embeddings->data,
        node_a,
        node_b,
        graph->embed_dim,
        d_distance);
    if (cudaGetLastError() != cudaSuccess) goto hyp_dist_cleanup;

    if (cudaMemcpy(distance, d_distance, sizeof(float), cudaMemcpyDeviceToHost) != cudaSuccess) goto hyp_dist_cleanup;

    success = true;

hyp_dist_cleanup:
    cudaFree(d_distance);

    return success;
}

/**
 * @brief Compute pairwise hyperbolic distances
 */
__global__ void kernel_hyperbolic_pairwise(
    const float* embeddings,
    const uint32_t* node_indices,
    uint32_t num_nodes,
    uint32_t embed_dim,
    float* distance_matrix)
{
    uint32_t i = blockIdx.y;
    uint32_t j = blockIdx.x * blockDim.x + threadIdx.x;

    if (i >= num_nodes || j >= num_nodes) return;

    uint32_t node_a = node_indices[i];
    uint32_t node_b = node_indices[j];

    const float* u = embeddings + node_a * embed_dim;
    const float* v = embeddings + node_b * embed_dim;

    distance_matrix[i * num_nodes + j] = device_hyperbolic_distance(u, v, embed_dim);
}

bool nimcp_gpu_hyperbolic_pairwise_distance(
    nimcp_gpu_knowledge_graph_t* graph,
    const uint32_t* node_indices,
    uint32_t num_nodes,
    nimcp_gpu_tensor_t* distance_matrix)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !node_indices || !distance_matrix) {
        LOG_ERROR("Invalid parameters for hyperbolic_pairwise_distance");
        return false;
    }

    if (!graph->is_hyperbolic) {
        LOG_ERROR("Graph embeddings are not in hyperbolic space");
        return false;
    }

    uint32_t* d_indices = NULL;
    bool success = false;

    if (cudaMalloc(&d_indices, num_nodes * sizeof(uint32_t)) != cudaSuccess) goto hyp_pairwise_cleanup;
    if (cudaMemcpy(d_indices, node_indices, num_nodes * sizeof(uint32_t), cudaMemcpyHostToDevice) != cudaSuccess) goto hyp_pairwise_cleanup;

    {
        dim3 block(32);
        dim3 grid((num_nodes + 31) / 32, num_nodes);

        kernel_hyperbolic_pairwise<<<grid, block>>>(
            (const float*)graph->node_embeddings->data,
            d_indices,
            num_nodes,
            graph->embed_dim,
            (float*)distance_matrix->data);
        if (cudaGetLastError() != cudaSuccess) goto hyp_pairwise_cleanup;
    }

    success = true;

hyp_pairwise_cleanup:
    cudaFree(d_indices);
    return success;
}

/**
 * @brief Riemannian gradient for Poincare ball
 *
 * Scales Euclidean gradient by (1 - ||x||^2)^2 / 4
 */
__global__ void kernel_riemannian_gradient(
    const float* embeddings,
    const float* euclidean_grad,
    float* riemannian_grad,
    uint32_t num_nodes,
    uint32_t embed_dim)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_nodes * embed_dim) return;

    uint32_t node = idx / embed_dim;
    uint32_t d = idx % embed_dim;

    const float* emb = embeddings + node * embed_dim;

    // Compute norm squared
    float norm_sq = 0.0f;
    for (uint32_t i = 0; i < embed_dim; i++) {
        norm_sq += emb[i] * emb[i];
    }
    norm_sq = fminf(norm_sq, 1.0f - 1e-5f);

    // Scale factor
    float scale = (1.0f - norm_sq) * (1.0f - norm_sq) / 4.0f;

    riemannian_grad[idx] = scale * euclidean_grad[idx];
}

/**
 * @brief Exponential map for Poincare ball (retraction)
 *
 * exp_x(v) = x + v (approximately, for small v)
 * Then project back into the ball
 */
__global__ void kernel_hyperbolic_exp_map(
    float* embeddings,
    const float* tangent_vectors,
    float learning_rate,
    uint32_t num_nodes,
    uint32_t embed_dim)
{
    uint32_t node = blockIdx.x;
    if (node >= num_nodes) return;

    float* emb = embeddings + node * embed_dim;
    const float* tangent = tangent_vectors + node * embed_dim;

    // Update embedding
    for (uint32_t d = threadIdx.x; d < embed_dim; d += blockDim.x) {
        emb[d] -= learning_rate * tangent[d];
    }
    __syncthreads();

    // Project back into Poincare ball
    if (threadIdx.x == 0) {
        float norm_sq = 0.0f;
        for (uint32_t d = 0; d < embed_dim; d++) {
            norm_sq += emb[d] * emb[d];
        }

        if (norm_sq >= 1.0f) {
            float scale = 0.99f / sqrtf(norm_sq);
            for (uint32_t d = 0; d < embed_dim; d++) {
                emb[d] *= scale;
            }
        }
    }
}

bool nimcp_gpu_hyperbolic_sgd_step(
    nimcp_gpu_knowledge_graph_t* graph,
    const nimcp_gpu_tensor_t* euclidean_gradients,
    float learning_rate)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !euclidean_gradients) {
        LOG_ERROR("Invalid parameters for hyperbolic_sgd_step");
        return false;
    }

    if (!graph->is_hyperbolic) {
        LOG_ERROR("Graph embeddings are not in hyperbolic space");
        return false;
    }

    size_t numel = graph->num_nodes * graph->embed_dim;
    size_t dims[] = {graph->num_nodes, graph->embed_dim};

    // Allocate Riemannian gradient buffer
    nimcp_gpu_tensor_t* riem_grad = nimcp_gpu_tensor_create(graph->ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!riem_grad) {
        return false;
    }

    // Convert Euclidean to Riemannian gradient
    kernel_riemannian_gradient<<<GRID_SIZE(numel), BLOCK_SIZE>>>(
        (const float*)graph->node_embeddings->data,
        (const float*)euclidean_gradients->data,
        (float*)riem_grad->data,
        graph->num_nodes,
        graph->embed_dim);
    CUDA_CHECK(cudaGetLastError());

    // Apply exponential map update
    kernel_hyperbolic_exp_map<<<graph->num_nodes, BLOCK_SIZE>>>(
        (float*)graph->node_embeddings->data,
        (const float*)riem_grad->data,
        learning_rate,
        graph->num_nodes,
        graph->embed_dim);
    CUDA_CHECK(cudaGetLastError());

    nimcp_gpu_tensor_destroy(riem_grad);
    return true;
}

/**
 * @brief Convert Euclidean embeddings to hyperbolic via exponential map at origin
 */
__global__ void kernel_euclidean_to_hyperbolic(
    float* embeddings,
    uint32_t num_nodes,
    uint32_t embed_dim)
{
    uint32_t node = blockIdx.x;
    if (node >= num_nodes) return;

    float* emb = embeddings + node * embed_dim;

    // Compute norm
    __shared__ float norm_sq_shared;
    float local_norm_sq = 0.0f;

    for (uint32_t d = threadIdx.x; d < embed_dim; d += blockDim.x) {
        local_norm_sq += emb[d] * emb[d];
    }
    local_norm_sq = warp_reduce_sum(local_norm_sq);

    if (threadIdx.x == 0) {
        norm_sq_shared = local_norm_sq;
    }
    __syncthreads();

    float norm = sqrtf(norm_sq_shared);

    // Exponential map at origin: tanh(||v||/2) * v / ||v||
    if (norm > 1e-8f) {
        float scale = tanhf(norm / 2.0f) / norm;
        for (uint32_t d = threadIdx.x; d < embed_dim; d += blockDim.x) {
            emb[d] *= scale;
        }
    }
}

bool nimcp_gpu_euclidean_to_hyperbolic(nimcp_gpu_knowledge_graph_t* graph)
{
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph) {
        LOG_ERROR("Invalid graph parameter");
        return false;
    }

    if (!graph->node_embeddings) {
        LOG_ERROR("Graph has no embeddings");
        return false;
    }

    kernel_euclidean_to_hyperbolic<<<graph->num_nodes, BLOCK_SIZE>>>(
        (float*)graph->node_embeddings->data,
        graph->num_nodes,
        graph->embed_dim);
    CUDA_CHECK(cudaGetLastError());

    graph->is_hyperbolic = true;
    return true;
}

/**
 * @brief Hyperbolic k-NN search
 */
__global__ void kernel_hyperbolic_all_distances(
    const float* embeddings,
    uint32_t query_node,
    uint32_t num_nodes,
    uint32_t embed_dim,
    float* distances)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_nodes) return;

    const float* query = embeddings + query_node * embed_dim;
    const float* target = embeddings + idx * embed_dim;

    distances[idx] = device_hyperbolic_distance(query, target, embed_dim);
}

bool nimcp_gpu_hyperbolic_knn(
    nimcp_gpu_knowledge_graph_t* graph,
    uint32_t query_node,
    uint32_t k,
    nimcp_similarity_result_t* result)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph || !result) {
        LOG_ERROR("Invalid parameters for hyperbolic_knn");
        return false;
    }

    if (!graph->is_hyperbolic) {
        LOG_ERROR("Graph embeddings are not in hyperbolic space");
        return false;
    }

    if (query_node >= graph->num_nodes) {
        LOG_ERROR("Query node out of range");
        return false;
    }

    k = (k > graph->num_nodes) ? graph->num_nodes : k;

    // Allocate distance buffer
    float* d_distances = NULL;
    float* h_distances = NULL;
    uint32_t* h_indices = NULL;
    float* h_scores = NULL;
    bool success = false;

    if (cudaMalloc(&d_distances, graph->num_nodes * sizeof(float)) != cudaSuccess) goto hyp_knn_cleanup;

    // Compute all hyperbolic distances
    kernel_hyperbolic_all_distances<<<GRID_SIZE(graph->num_nodes), BLOCK_SIZE>>>(
        (const float*)graph->node_embeddings->data,
        query_node,
        graph->num_nodes,
        graph->embed_dim,
        d_distances);
    if (cudaGetLastError() != cudaSuccess) goto hyp_knn_cleanup;

    {
        // Create result tensors
        size_t k_dims[] = {k};
        result->indices = nimcp_gpu_tensor_create(graph->ctx, k_dims, 1, NIMCP_GPU_PRECISION_UINT32);
        result->scores = nimcp_gpu_tensor_create(graph->ctx, k_dims, 1, NIMCP_GPU_PRECISION_FP32);
        result->k = k;

        if (!result->indices || !result->scores) {
            cudaFree(d_distances);
            nimcp_similarity_result_destroy(result);
            return false;
        }
    }

    // Top-k selection (smallest distances)
    // For now, copy to host and do selection there
    h_distances = (float*)nimcp_malloc(graph->num_nodes * sizeof(float));
    if (!h_distances) goto hyp_knn_cleanup;
    if (cudaMemcpy(h_distances, d_distances, graph->num_nodes * sizeof(float), cudaMemcpyDeviceToHost) != cudaSuccess) goto hyp_knn_cleanup;

    h_indices = (uint32_t*)nimcp_malloc(k * sizeof(uint32_t));
    if (!h_indices) goto hyp_knn_cleanup;
    h_scores = (float*)nimcp_malloc(k * sizeof(float));
    if (!h_scores) goto hyp_knn_cleanup;

    // Initialize with worst values
    for (uint32_t i = 0; i < k; i++) {
        h_indices[i] = 0;
        h_scores[i] = FLT_MAX;
    }

    // Find k smallest distances
    for (uint32_t n = 0; n < graph->num_nodes; n++) {
        float dist = h_distances[n];

        uint32_t insert_pos = k;
        for (uint32_t i = 0; i < k; i++) {
            if (dist < h_scores[i]) {
                insert_pos = i;
                break;
            }
        }

        if (insert_pos < k) {
            for (uint32_t i = k - 1; i > insert_pos; i--) {
                h_indices[i] = h_indices[i - 1];
                h_scores[i] = h_scores[i - 1];
            }
            h_indices[insert_pos] = n;
            h_scores[insert_pos] = dist;
        }
    }

    if (cudaMemcpy(result->indices->data, h_indices, k * sizeof(uint32_t), cudaMemcpyHostToDevice) != cudaSuccess) goto hyp_knn_cleanup;
    if (cudaMemcpy(result->scores->data, h_scores, k * sizeof(float), cudaMemcpyHostToDevice) != cudaSuccess) goto hyp_knn_cleanup;

    success = true;

hyp_knn_cleanup:
    nimcp_free(h_distances);
    nimcp_free(h_indices);
    nimcp_free(h_scores);
    cudaFree(d_distances);

    return success;
}

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Compute degree statistics
 */
__global__ void kernel_compute_degrees(
    const uint32_t* row_offsets,
    uint32_t num_nodes,
    uint32_t* degrees,
    uint32_t* max_degree)
{
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_nodes) return;

    uint32_t degree = row_offsets[idx + 1] - row_offsets[idx];
    degrees[idx] = degree;
    atomicMax(max_degree, degree);
}

bool nimcp_gpu_knowledge_graph_stats(
    nimcp_gpu_knowledge_graph_t* graph,
    float* avg_degree,
    uint32_t* max_degree,
    float* embedding_norm)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!graph) {
        LOG_ERROR("Invalid graph parameter");
        return false;
    }

    // Compute degree statistics
    uint32_t* d_degrees = NULL;
    uint32_t* d_max_degree = NULL;
    bool success = false;

    if (cudaMalloc(&d_degrees, graph->num_nodes * sizeof(uint32_t)) != cudaSuccess) goto stats_cleanup;
    if (cudaMalloc(&d_max_degree, sizeof(uint32_t)) != cudaSuccess) goto stats_cleanup;
    if (cudaMemset(d_max_degree, 0, sizeof(uint32_t)) != cudaSuccess) goto stats_cleanup;

    kernel_compute_degrees<<<GRID_SIZE(graph->num_nodes), BLOCK_SIZE>>>(
        (const uint32_t*)graph->row_offsets->data,
        graph->num_nodes,
        d_degrees,
        d_max_degree);
    if (cudaGetLastError() != cudaSuccess) goto stats_cleanup;

    // Get max degree
    if (cudaMemcpy(max_degree, d_max_degree, sizeof(uint32_t), cudaMemcpyDeviceToHost) != cudaSuccess) goto stats_cleanup;

    // Compute average degree
    *avg_degree = (float)graph->num_edges / (float)graph->num_nodes;

    // Compute average embedding norm if embeddings exist
    if (graph->node_embeddings && embedding_norm) {
        float* h_embeddings = (float*)nimcp_malloc(graph->num_nodes * graph->embed_dim * sizeof(float));
        if (!h_embeddings) goto stats_cleanup;
        if (cudaMemcpy(h_embeddings, graph->node_embeddings->data,
                              graph->num_nodes * graph->embed_dim * sizeof(float),
                              cudaMemcpyDeviceToHost) != cudaSuccess) {
            nimcp_free(h_embeddings);
            goto stats_cleanup;
        }

        float total_norm = 0.0f;
        for (uint32_t n = 0; n < graph->num_nodes; n++) {
            float norm = 0.0f;
            for (uint32_t d = 0; d < graph->embed_dim; d++) {
                float val = h_embeddings[n * graph->embed_dim + d];
                norm += val * val;
            }
            total_norm += sqrtf(norm);
        }
        *embedding_norm = total_norm / graph->num_nodes;

        nimcp_free(h_embeddings);
    } else if (embedding_norm) {
        *embedding_norm = 0.0f;
    }

    success = true;

stats_cleanup:
    cudaFree(d_degrees);
    cudaFree(d_max_degree);

    return success;
}

bool nimcp_gpu_knowledge_graph_is_valid(const nimcp_gpu_knowledge_graph_t* graph)
{
    if (!graph) {
        return false;
    }

    if (!graph->ctx) {
        LOG_ERROR("Graph has no GPU context");
        return false;
    }

    if (!graph->row_offsets) {
        LOG_ERROR("Graph has no row_offsets");
        return false;
    }

    if (graph->num_edges > 0 && !graph->col_indices) {
        LOG_ERROR("Graph has edges but no col_indices");
        return false;
    }

    if (graph->num_nodes == 0) {
        LOG_ERROR("Graph has no nodes");
        return false;
    }

    return true;
}

//=============================================================================
// Knowledge Graph Embedding DAO - CUDA Kernels
//=============================================================================

/**
 * @brief Lookup embeddings by indices
 */
__global__ void kernel_embedding_lookup(
    const float* embeddings,
    const int* indices,
    float* output,
    int embedding_dim,
    int batch_size)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = batch_size * embedding_dim;
    if (idx >= total) return;

    int batch_idx = idx / embedding_dim;
    int dim_idx = idx % embedding_dim;
    int entity_idx = indices[batch_idx];

    output[idx] = embeddings[entity_idx * embedding_dim + dim_idx];
}

/**
 * @brief Compute cosine similarity between query and all embeddings
 */
__global__ void kernel_kg_cosine_similarity(
    const float* query,
    const float* embeddings,
    const int* valid_flags,
    float* scores,
    int embedding_dim,
    int num_embeddings)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_embeddings) return;

    // Skip invalid embeddings
    if (valid_flags && !valid_flags[idx]) {
        scores[idx] = -FLT_MAX;
        return;
    }

    const float* emb = embeddings + idx * embedding_dim;

    float dot = 0.0f;
    float norm_q = 0.0f;
    float norm_e = 0.0f;

    for (int d = 0; d < embedding_dim; d++) {
        float q = query[d];
        float e = emb[d];
        dot += q * e;
        norm_q += q * q;
        norm_e += e * e;
    }

    norm_q = sqrtf(norm_q);
    norm_e = sqrtf(norm_e);

    if (norm_q > 1e-8f && norm_e > 1e-8f) {
        scores[idx] = dot / (norm_q * norm_e);
    } else {
        scores[idx] = 0.0f;
    }
}

/**
 * @brief Parallel top-k selection using bitonic sort
 * Each block handles a portion of the data
 */
__global__ void kernel_topk_scores(
    const float* scores,
    int* indices,
    float* topk_scores,
    int n,
    int k)
{
    extern __shared__ float shared[];
    float* s_scores = shared;
    int* s_indices = (int*)(shared + blockDim.x);

    int tid = threadIdx.x;
    int gid = blockIdx.x * blockDim.x + threadIdx.x;

    // Initialize with data or -inf
    if (gid < n) {
        s_scores[tid] = scores[gid];
        s_indices[tid] = gid;
    } else {
        s_scores[tid] = -FLT_MAX;
        s_indices[tid] = -1;
    }
    __syncthreads();

    // Bitonic sort within block (descending order)
    for (int size = 2; size <= blockDim.x; size *= 2) {
        for (int stride = size / 2; stride > 0; stride /= 2) {
            int j = tid ^ stride;
            if (j > tid) {
                bool ascending = ((tid & size) == 0);
                if ((s_scores[tid] < s_scores[j]) == ascending) {
                    float tmp_score = s_scores[tid];
                    s_scores[tid] = s_scores[j];
                    s_scores[j] = tmp_score;
                    int tmp_idx = s_indices[tid];
                    s_indices[tid] = s_indices[j];
                    s_indices[j] = tmp_idx;
                }
            }
            __syncthreads();
        }
    }

    // Write top-k from first block only (simplified for demo)
    if (blockIdx.x == 0 && tid < k) {
        topk_scores[tid] = s_scores[tid];
        indices[tid] = s_indices[tid];
    }
}

/**
 * @brief TransE score computation: ||h + r - t||
 */
__global__ void kernel_transe_score(
    const float* head,
    const float* relation,
    const float* tail,
    float* scores,
    int embedding_dim,
    int batch_size)
{
    int batch_idx = blockIdx.x;
    if (batch_idx >= batch_size) return;

    // Use shared memory for reduction
    extern __shared__ float shared_sum[];

    const float* h = head + batch_idx * embedding_dim;
    const float* r = relation + batch_idx * embedding_dim;
    const float* t = tail + batch_idx * embedding_dim;

    float local_sum = 0.0f;
    for (int d = threadIdx.x; d < embedding_dim; d += blockDim.x) {
        float diff = h[d] + r[d] - t[d];
        local_sum += diff * diff;
    }

    // Warp reduction
    for (int offset = WARP_SIZE / 2; offset > 0; offset /= 2) {
        local_sum += __shfl_down_sync(0xFFFFFFFF, local_sum, offset);
    }

    // Store warp results
    int lane = threadIdx.x % WARP_SIZE;
    int wid = threadIdx.x / WARP_SIZE;
    if (lane == 0) {
        shared_sum[wid] = local_sum;
    }
    __syncthreads();

    // Final reduction
    if (threadIdx.x == 0) {
        float total = 0.0f;
        int num_warps = (blockDim.x + WARP_SIZE - 1) / WARP_SIZE;
        for (int i = 0; i < num_warps; i++) {
            total += shared_sum[i];
        }
        scores[batch_idx] = sqrtf(total);
    }
}

/**
 * @brief TransE gradient computation with negative sampling
 */
__global__ void kernel_transe_gradient(
    const float* head,
    const float* relation,
    const float* tail,
    const float* neg_tail,
    float* grad_head,
    float* grad_rel,
    float* grad_tail,
    float* grad_neg_tail,
    int embedding_dim,
    int batch_size,
    float margin)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = batch_size * embedding_dim;
    if (idx >= total) return;

    int batch_idx = idx / embedding_dim;
    int dim_idx = idx % embedding_dim;

    float h = head[idx];
    float r = relation[idx];
    float t = tail[idx];
    float nt = neg_tail[idx];

    // Positive: h + r - t
    float pos_diff = h + r - t;
    // Negative: h + r - neg_t
    float neg_diff = h + r - nt;

    // Gradients for margin-based loss:
    // L = max(0, margin + d_pos - d_neg)
    // When L > 0:
    //   d(L)/d(h) = (h+r-t)/|pos| - (h+r-nt)/|neg|
    //   d(L)/d(r) = (h+r-t)/|pos| - (h+r-nt)/|neg|
    //   d(L)/d(t) = -(h+r-t)/|pos|
    //   d(L)/d(nt) = (h+r-nt)/|neg|

    // Simplified: assume loss is active (proper version checks margin condition)
    grad_head[idx] = pos_diff - neg_diff;
    grad_rel[idx] = pos_diff - neg_diff;
    grad_tail[idx] = -pos_diff;
    grad_neg_tail[idx] = neg_diff;
}

/**
 * @brief L2 normalize embeddings (per entity)
 */
__global__ void kernel_kg_normalize_embeddings(
    float* embeddings,
    int embedding_dim,
    int num_embeddings)
{
    int entity_idx = blockIdx.x;
    if (entity_idx >= num_embeddings) return;

    float* emb = embeddings + entity_idx * embedding_dim;

    // Compute norm
    __shared__ float shared_sum[8];
    float local_sum = 0.0f;

    for (int d = threadIdx.x; d < embedding_dim; d += blockDim.x) {
        local_sum += emb[d] * emb[d];
    }

    // Warp reduction
    for (int offset = WARP_SIZE / 2; offset > 0; offset /= 2) {
        local_sum += __shfl_down_sync(0xFFFFFFFF, local_sum, offset);
    }

    int lane = threadIdx.x % WARP_SIZE;
    int wid = threadIdx.x / WARP_SIZE;
    if (lane == 0 && wid < 8) {
        shared_sum[wid] = local_sum;
    }
    __syncthreads();

    float norm = 0.0f;
    if (threadIdx.x == 0) {
        int num_warps = min((int)((blockDim.x + WARP_SIZE - 1) / WARP_SIZE), 8);
        for (int i = 0; i < num_warps; i++) {
            norm += shared_sum[i];
        }
        shared_sum[0] = sqrtf(norm);
    }
    __syncthreads();

    norm = shared_sum[0];
    if (norm > 1e-8f) {
        for (int d = threadIdx.x; d < embedding_dim; d += blockDim.x) {
            emb[d] /= norm;
        }
    }
}

/**
 * @brief Subgraph embedding match kernel
 * Computes similarity between pattern embeddings and graph node embeddings
 */
__global__ void kernel_subgraph_embedding_match(
    const float* pattern_embeddings,
    const float* graph_embeddings,
    const float* adjacency_csr_rows,
    const float* adjacency_csr_cols,
    const float* pattern_adj_rows,
    const float* pattern_adj_cols,
    float* match_scores,
    int* match_mappings,
    int pattern_size,
    int graph_size,
    int embedding_dim)
{
    int candidate_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (candidate_idx >= graph_size) return;

    // Compute similarity between first pattern node and this graph node
    const float* p_emb = pattern_embeddings;
    const float* g_emb = graph_embeddings + candidate_idx * embedding_dim;

    float dot = 0.0f;
    float norm_p = 0.0f;
    float norm_g = 0.0f;

    for (int d = 0; d < embedding_dim; d++) {
        dot += p_emb[d] * g_emb[d];
        norm_p += p_emb[d] * p_emb[d];
        norm_g += g_emb[d] * g_emb[d];
    }

    float sim = 0.0f;
    if (norm_p > 1e-8f && norm_g > 1e-8f) {
        sim = dot / (sqrtf(norm_p) * sqrtf(norm_g));
    }

    match_scores[candidate_idx] = sim;
    match_mappings[candidate_idx * pattern_size] = candidate_idx;
}

/**
 * @brief Guided BFS step with embedding similarity to target
 */
__global__ void kernel_guided_bfs_step(
    const float* frontier,
    int* next_frontier,
    int* next_frontier_count,
    const float* csr_rows,
    const float* csr_cols,
    const float* target_embedding,
    const float* entity_embeddings,
    float* path_scores,
    int* visited,
    int* parents,
    int frontier_size,
    int num_entities,
    int embedding_dim)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= frontier_size) return;

    int node = (int)frontier[idx];
    if (node < 0 || node >= num_entities) return;

    int row_start = (int)csr_rows[node];
    int row_end = (int)csr_rows[node + 1];

    for (int e = row_start; e < row_end; e++) {
        int neighbor = (int)csr_cols[e];
        if (neighbor < 0 || neighbor >= num_entities) continue;

        // Try to visit
        int old = atomicCAS(&visited[neighbor], 0, 1);
        if (old == 0) {
            // Compute similarity to target
            const float* n_emb = entity_embeddings + neighbor * embedding_dim;
            float dot = 0.0f;
            float norm_n = 0.0f;
            float norm_t = 0.0f;

            for (int d = 0; d < embedding_dim; d++) {
                dot += n_emb[d] * target_embedding[d];
                norm_n += n_emb[d] * n_emb[d];
                norm_t += target_embedding[d] * target_embedding[d];
            }

            float sim = 0.0f;
            if (norm_n > 1e-8f && norm_t > 1e-8f) {
                sim = dot / (sqrtf(norm_n) * sqrtf(norm_t));
            }

            // Combine with path score (higher is better)
            path_scores[neighbor] = path_scores[node] + sim;
            parents[neighbor] = node;

            // Add to next frontier
            int pos = atomicAdd(next_frontier_count, 1);
            next_frontier[pos] = neighbor;
        }
    }
}

/**
 * @brief Update embeddings with gradient descent
 */
__global__ void kernel_kg_embedding_update(
    float* embeddings,
    const float* gradients,
    float learning_rate,
    float regularization,
    int size)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= size) return;

    float grad = gradients[idx] + regularization * embeddings[idx];
    embeddings[idx] -= learning_rate * grad;
}

//=============================================================================
// DAO Operation Implementations
//=============================================================================

static int dao_create_embedding_impl(nimcp_knowledge_embedding_dao_t* self, int entity_id, const float* embedding)
{
    if (!self || !embedding || entity_id < 0 || entity_id >= self->max_entities) {
        return -1;
    }

    // Check if already exists
    int* h_valid = (int*)nimcp_malloc(sizeof(int));
    if (!h_valid) return -1;
    cudaMemcpy(h_valid, self->d_entity_valid + entity_id, sizeof(int), cudaMemcpyDeviceToHost);
    if (*h_valid) {
        nimcp_free(h_valid);
        return -1;  // Already exists
    }
    nimcp_free(h_valid);

    // Copy embedding to device
    cudaMemcpy(self->d_entity_embeddings + entity_id * self->embedding_dim,
               embedding, self->embedding_dim * sizeof(float), cudaMemcpyHostToDevice);

    // Mark as valid
    int valid = 1;
    cudaMemcpy(self->d_entity_valid + entity_id, &valid, sizeof(int), cudaMemcpyHostToDevice);

    self->num_entities++;
    return 0;
}

static int dao_read_embedding_impl(nimcp_knowledge_embedding_dao_t* self, int entity_id, float* embedding_out)
{
    if (!self || !embedding_out || entity_id < 0 || entity_id >= self->max_entities) {
        return -1;
    }

    // Check validity
    int h_valid;
    cudaMemcpy(&h_valid, self->d_entity_valid + entity_id, sizeof(int), cudaMemcpyDeviceToHost);
    if (!h_valid) {
        return -1;  // Does not exist
    }

    // Copy embedding to host
    cudaMemcpy(embedding_out, self->d_entity_embeddings + entity_id * self->embedding_dim,
               self->embedding_dim * sizeof(float), cudaMemcpyDeviceToHost);

    return 0;
}

static int dao_update_embedding_impl(nimcp_knowledge_embedding_dao_t* self, int entity_id, const float* embedding)
{
    if (!self || !embedding || entity_id < 0 || entity_id >= self->max_entities) {
        return -1;
    }

    // Check validity
    int h_valid;
    cudaMemcpy(&h_valid, self->d_entity_valid + entity_id, sizeof(int), cudaMemcpyDeviceToHost);
    if (!h_valid) {
        return -1;  // Does not exist
    }

    // Update embedding
    cudaMemcpy(self->d_entity_embeddings + entity_id * self->embedding_dim,
               embedding, self->embedding_dim * sizeof(float), cudaMemcpyHostToDevice);

    return 0;
}

static int dao_delete_embedding_impl(nimcp_knowledge_embedding_dao_t* self, int entity_id)
{
    if (!self || entity_id < 0 || entity_id >= self->max_entities) {
        return -1;
    }

    // Check validity
    int h_valid;
    cudaMemcpy(&h_valid, self->d_entity_valid + entity_id, sizeof(int), cudaMemcpyDeviceToHost);
    if (!h_valid) {
        return -1;  // Does not exist
    }

    // Mark as invalid
    int invalid = 0;
    cudaMemcpy(self->d_entity_valid + entity_id, &invalid, sizeof(int), cudaMemcpyHostToDevice);

    if (self->num_entities > 0) self->num_entities--;
    return 0;
}

static int dao_find_similar_impl(nimcp_knowledge_embedding_dao_t* self, const float* query, int k, int* results, float* scores)
{
    if (!self || !query || !results || !scores || k <= 0) {
        return -1;
    }

    k = (k > self->max_entities) ? self->max_entities : k;

    // Copy query to device
    float* d_query;
    if (cudaMalloc(&d_query, self->embedding_dim * sizeof(float)) != cudaSuccess) {
        return -1;
    }
    cudaMemcpy(d_query, query, self->embedding_dim * sizeof(float), cudaMemcpyHostToDevice);

    // Allocate scores buffer
    float* d_scores;
    if (cudaMalloc(&d_scores, self->max_entities * sizeof(float)) != cudaSuccess) {
        cudaFree(d_query);
        return -1;
    }

    // Compute similarities
    kernel_kg_cosine_similarity<<<GRID_SIZE(self->max_entities), BLOCK_SIZE>>>(
        d_query,
        self->d_entity_embeddings,
        self->d_entity_valid,
        d_scores,
        self->embedding_dim,
        self->max_entities);
    cudaDeviceSynchronize();

    // Get top-k (for now, copy to host and do selection there)
    float* h_scores = (float*)nimcp_malloc(self->max_entities * sizeof(float));
    if (!h_scores) {
        cudaFree(d_query);
        cudaFree(d_scores);
        return -1;
    }
    cudaMemcpy(h_scores, d_scores, self->max_entities * sizeof(float), cudaMemcpyDeviceToHost);

    // Initialize results
    for (int i = 0; i < k; i++) {
        results[i] = -1;
        scores[i] = -FLT_MAX;
    }

    // Find top-k
    for (int n = 0; n < self->max_entities; n++) {
        float score = h_scores[n];
        if (score <= -FLT_MAX + 1.0f) continue;  // Skip invalid

        // Insert into sorted top-k
        for (int i = 0; i < k; i++) {
            if (score > scores[i]) {
                // Shift down
                for (int j = k - 1; j > i; j--) {
                    scores[j] = scores[j - 1];
                    results[j] = results[j - 1];
                }
                scores[i] = score;
                results[i] = n;
                break;
            }
        }
    }

    nimcp_free(h_scores);
    cudaFree(d_query);
    cudaFree(d_scores);

    return 0;
}

//=============================================================================
// DAO Creation and Destruction
//=============================================================================

nimcp_knowledge_embedding_dao_t* nimcp_knowledge_embedding_dao_create(
    void* gpu_ctx, int max_entities, int max_relations, int embedding_dim)
{
    if (!gpu_ctx || max_entities <= 0 || max_relations <= 0 || embedding_dim <= 0) {
        LOG_ERROR("Invalid parameters for embedding DAO creation");
        return NULL;
    }

    nimcp_knowledge_embedding_dao_t* dao = (nimcp_knowledge_embedding_dao_t*)nimcp_calloc(1, sizeof(nimcp_knowledge_embedding_dao_t));
    if (!dao) {
        LOG_ERROR("Failed to allocate DAO structure");
        return NULL;
    }

    dao->max_entities = max_entities;
    dao->max_relations = max_relations;
    dao->embedding_dim = embedding_dim;
    dao->num_entities = 0;
    dao->num_relations = 0;
    dao->gpu_context = gpu_ctx;

    // Allocate device memory for entity embeddings
    if (cudaMalloc(&dao->d_entity_embeddings, max_entities * embedding_dim * sizeof(float)) != cudaSuccess) {
        LOG_ERROR("Failed to allocate entity embeddings");
        nimcp_free(dao);
        return NULL;
    }
    cudaMemset(dao->d_entity_embeddings, 0, max_entities * embedding_dim * sizeof(float));

    // Allocate device memory for relation embeddings
    if (cudaMalloc(&dao->d_relation_embeddings, max_relations * embedding_dim * sizeof(float)) != cudaSuccess) {
        LOG_ERROR("Failed to allocate relation embeddings");
        cudaFree(dao->d_entity_embeddings);
        nimcp_free(dao);
        return NULL;
    }
    cudaMemset(dao->d_relation_embeddings, 0, max_relations * embedding_dim * sizeof(float));

    // Allocate validity flags
    if (cudaMalloc(&dao->d_entity_valid, max_entities * sizeof(int)) != cudaSuccess) {
        LOG_ERROR("Failed to allocate entity validity flags");
        cudaFree(dao->d_entity_embeddings);
        cudaFree(dao->d_relation_embeddings);
        nimcp_free(dao);
        return NULL;
    }
    cudaMemset(dao->d_entity_valid, 0, max_entities * sizeof(int));

    if (cudaMalloc(&dao->d_relation_valid, max_relations * sizeof(int)) != cudaSuccess) {
        LOG_ERROR("Failed to allocate relation validity flags");
        cudaFree(dao->d_entity_embeddings);
        cudaFree(dao->d_relation_embeddings);
        cudaFree(dao->d_entity_valid);
        nimcp_free(dao);
        return NULL;
    }
    cudaMemset(dao->d_relation_valid, 0, max_relations * sizeof(int));

    // Set up function pointers
    dao->create_embedding = dao_create_embedding_impl;
    dao->read_embedding = dao_read_embedding_impl;
    dao->update_embedding = dao_update_embedding_impl;
    dao->delete_embedding = dao_delete_embedding_impl;
    dao->find_similar = dao_find_similar_impl;

    LOG_DEBUG("Created knowledge embedding DAO: max_entities=%d, max_relations=%d, embed_dim=%d",
              max_entities, max_relations, embedding_dim);
    return dao;
}

void nimcp_knowledge_embedding_dao_destroy(nimcp_knowledge_embedding_dao_t* dao)
{
    if (!dao) return;

    if (dao->d_entity_embeddings) cudaFree(dao->d_entity_embeddings);
    if (dao->d_relation_embeddings) cudaFree(dao->d_relation_embeddings);
    if (dao->d_entity_hash_table) cudaFree(dao->d_entity_hash_table);
    if (dao->d_relation_hash_table) cudaFree(dao->d_relation_hash_table);
    if (dao->d_entity_valid) cudaFree(dao->d_entity_valid);
    if (dao->d_relation_valid) cudaFree(dao->d_relation_valid);

    nimcp_free(dao);
}

//=============================================================================
// Knowledge Graph Query APIs
//=============================================================================

int nimcp_kg_semantic_search(
    nimcp_knowledge_embedding_dao_t* dao,
    const float* query_embedding, int k,
    nimcp_kg_result_t* result)
{
    if (!dao || !query_embedding || !result || k <= 0) {
        LOG_ERROR("Invalid parameters for semantic search");
        return -1;
    }

    // Allocate result arrays
    result->matched_entities = (int*)nimcp_malloc(k * sizeof(int));
    result->scores = (float*)nimcp_malloc(k * sizeof(float));
    result->matched_relations = NULL;
    result->path_lengths = NULL;

    if (!result->matched_entities || !result->scores) {
        LOG_ERROR("Failed to allocate result arrays");
        nimcp_kg_result_destroy(result);
        return -1;
    }

    // Use DAO's find_similar
    if (dao->find_similar(dao, query_embedding, k, result->matched_entities, result->scores) != 0) {
        nimcp_kg_result_destroy(result);
        return -1;
    }

    // Count valid results
    result->num_results = 0;
    for (int i = 0; i < k; i++) {
        if (result->matched_entities[i] >= 0) {
            result->num_results++;
        } else {
            break;
        }
    }

    return 0;
}

int nimcp_kg_find_path(
    nimcp_knowledge_embedding_dao_t* dao,
    int source_entity, int target_entity,
    int max_hops, nimcp_kg_result_t* result)
{
    if (!dao || !result || source_entity < 0 || target_entity < 0 || max_hops <= 0) {
        LOG_ERROR("Invalid parameters for path finding");
        return -1;
    }

    if (source_entity >= dao->max_entities || target_entity >= dao->max_entities) {
        LOG_ERROR("Entity index out of range");
        return -1;
    }

    // For now, return a simple result (full implementation would require graph structure)
    result->matched_entities = (int*)nimcp_malloc(2 * sizeof(int));
    result->scores = (float*)nimcp_malloc(sizeof(float));
    result->matched_relations = NULL;
    result->path_lengths = (int*)nimcp_malloc(sizeof(int));

    if (!result->matched_entities || !result->scores || !result->path_lengths) {
        nimcp_kg_result_destroy(result);
        return -1;
    }

    // If source == target, trivial path
    if (source_entity == target_entity) {
        result->matched_entities[0] = source_entity;
        result->scores[0] = 1.0f;
        result->path_lengths[0] = 0;
        result->num_results = 1;
        return 0;
    }

    // Compute embedding similarity as a proxy for path likelihood
    float* source_emb = (float*)nimcp_malloc(dao->embedding_dim * sizeof(float));
    float* target_emb = (float*)nimcp_malloc(dao->embedding_dim * sizeof(float));
    if (!source_emb || !target_emb) {
        nimcp_free(source_emb);
        nimcp_free(target_emb);
        nimcp_kg_result_destroy(result);
        return -1;
    }

    if (dao->read_embedding(dao, source_entity, source_emb) != 0 ||
        dao->read_embedding(dao, target_entity, target_emb) != 0) {
        nimcp_free(source_emb);
        nimcp_free(target_emb);
        nimcp_kg_result_destroy(result);
        return -1;
    }

    // Compute similarity
    float dot = 0.0f, norm_s = 0.0f, norm_t = 0.0f;
    for (int d = 0; d < dao->embedding_dim; d++) {
        dot += source_emb[d] * target_emb[d];
        norm_s += source_emb[d] * source_emb[d];
        norm_t += target_emb[d] * target_emb[d];
    }

    float similarity = 0.0f;
    if (norm_s > 1e-8f && norm_t > 1e-8f) {
        similarity = dot / (sqrtf(norm_s) * sqrtf(norm_t));
    }

    result->matched_entities[0] = source_entity;
    result->matched_entities[1] = target_entity;
    result->scores[0] = similarity;
    result->path_lengths[0] = (similarity > 0.5f) ? 1 : max_hops;  // Heuristic
    result->num_results = 1;

    nimcp_free(source_emb);
    nimcp_free(target_emb);

    LOG_WARN("Path finding returns embedding-based estimate (full BFS not implemented for DAO)");
    return 0;
}

int nimcp_kg_pattern_match(
    nimcp_knowledge_embedding_dao_t* dao,
    const nimcp_kg_query_t* pattern,
    nimcp_kg_result_t* result)
{
    if (!dao || !pattern || !result) {
        LOG_ERROR("Invalid parameters for pattern matching");
        return -1;
    }

    // Use semantic search as basis for pattern matching
    if (pattern->query_embedding) {
        return nimcp_kg_semantic_search(dao, pattern->query_embedding, pattern->top_k, result);
    }

    LOG_WARN("Pattern matching requires query embedding - returning empty result");
    result->matched_entities = NULL;
    result->matched_relations = NULL;
    result->scores = NULL;
    result->path_lengths = NULL;
    result->num_results = 0;

    return 0;
}

void nimcp_kg_result_destroy(nimcp_kg_result_t* result)
{
    if (!result) return;

    if (result->matched_entities) nimcp_free(result->matched_entities);
    if (result->matched_relations) nimcp_free(result->matched_relations);
    if (result->scores) nimcp_free(result->scores);
    if (result->path_lengths) nimcp_free(result->path_lengths);

    result->matched_entities = NULL;
    result->matched_relations = NULL;
    result->scores = NULL;
    result->path_lengths = NULL;
    result->num_results = 0;
}

//=============================================================================
// TransE Training APIs
//=============================================================================

int nimcp_kg_train_step(
    nimcp_knowledge_embedding_dao_t* dao,
    const int* head_entities, const int* relations, const int* tail_entities,
    int batch_size, nimcp_kg_train_config_t* config)
{
    if (!dao || !head_entities || !relations || !tail_entities || !config || batch_size <= 0) {
        LOG_ERROR("Invalid parameters for training step");
        return -1;
    }

    int dim = dao->embedding_dim;
    size_t emb_batch_size = batch_size * dim * sizeof(float);

    // Allocate device memory for batch embeddings - pre-init all to NULL for cleanup
    float *d_head = NULL, *d_rel = NULL, *d_tail = NULL, *d_neg_tail = NULL;
    float *d_grad_head = NULL, *d_grad_rel = NULL, *d_grad_tail = NULL, *d_grad_neg = NULL;
    int *d_head_idx = NULL, *d_rel_idx = NULL, *d_tail_idx = NULL;
    int *d_neg_idx = NULL;
    int* neg_tail_idx = NULL;
    float lr = 0.0f;
    float reg = 0.0f;

    if (cudaMalloc(&d_head, emb_batch_size) != cudaSuccess) goto train_cleanup;
    if (cudaMalloc(&d_rel, emb_batch_size) != cudaSuccess) goto train_cleanup;
    if (cudaMalloc(&d_tail, emb_batch_size) != cudaSuccess) goto train_cleanup;
    if (cudaMalloc(&d_neg_tail, emb_batch_size) != cudaSuccess) goto train_cleanup;
    if (cudaMalloc(&d_grad_head, emb_batch_size) != cudaSuccess) goto train_cleanup;
    if (cudaMalloc(&d_grad_rel, emb_batch_size) != cudaSuccess) goto train_cleanup;
    if (cudaMalloc(&d_grad_tail, emb_batch_size) != cudaSuccess) goto train_cleanup;
    if (cudaMalloc(&d_grad_neg, emb_batch_size) != cudaSuccess) goto train_cleanup;

    // Copy indices to device for lookup
    if (cudaMalloc(&d_head_idx, batch_size * sizeof(int)) != cudaSuccess) goto train_cleanup;
    if (cudaMalloc(&d_rel_idx, batch_size * sizeof(int)) != cudaSuccess) goto train_cleanup;
    if (cudaMalloc(&d_tail_idx, batch_size * sizeof(int)) != cudaSuccess) goto train_cleanup;

    cudaMemcpy(d_head_idx, head_entities, batch_size * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(d_rel_idx, relations, batch_size * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(d_tail_idx, tail_entities, batch_size * sizeof(int), cudaMemcpyHostToDevice);

    // Lookup embeddings
    kernel_embedding_lookup<<<GRID_SIZE(batch_size * dim), BLOCK_SIZE>>>(
        dao->d_entity_embeddings, d_head_idx, d_head, dim, batch_size);
    kernel_embedding_lookup<<<GRID_SIZE(batch_size * dim), BLOCK_SIZE>>>(
        dao->d_relation_embeddings, d_rel_idx, d_rel, dim, batch_size);
    kernel_embedding_lookup<<<GRID_SIZE(batch_size * dim), BLOCK_SIZE>>>(
        dao->d_entity_embeddings, d_tail_idx, d_tail, dim, batch_size);

    // Generate negative samples (for simplicity, use random valid entities)
    // In a full implementation, this would sample corrupted triples
    neg_tail_idx = (int*)nimcp_malloc(batch_size * sizeof(int));
    if (!neg_tail_idx) goto train_cleanup;
    for (int i = 0; i < batch_size; i++) {
        // Simple corruption: shift tail by 1
        neg_tail_idx[i] = (tail_entities[i] + 1) % dao->max_entities;
    }
    if (cudaMalloc(&d_neg_idx, batch_size * sizeof(int)) != cudaSuccess) {
        nimcp_free(neg_tail_idx);
        goto train_cleanup;
    }
    cudaMemcpy(d_neg_idx, neg_tail_idx, batch_size * sizeof(int), cudaMemcpyHostToDevice);
    nimcp_free(neg_tail_idx);

    kernel_embedding_lookup<<<GRID_SIZE(batch_size * dim), BLOCK_SIZE>>>(
        dao->d_entity_embeddings, d_neg_idx, d_neg_tail, dim, batch_size);

    cudaDeviceSynchronize();

    // Compute gradients
    kernel_transe_gradient<<<GRID_SIZE(batch_size * dim), BLOCK_SIZE>>>(
        d_head, d_rel, d_tail, d_neg_tail,
        d_grad_head, d_grad_rel, d_grad_tail, d_grad_neg,
        dim, batch_size, config->margin);

    cudaDeviceSynchronize();

    // Update embeddings via scatter-add
    // This is simplified - a full implementation would use atomics or segment reduction
    lr = config->learning_rate;
    reg = config->regularization;

    // Update entity embeddings (head, tail, neg_tail)
    kernel_kg_embedding_update<<<GRID_SIZE(dao->max_entities * dim), BLOCK_SIZE>>>(
        dao->d_entity_embeddings, d_grad_head, lr, reg, dao->max_entities * dim);

    // Update relation embeddings
    kernel_kg_embedding_update<<<GRID_SIZE(dao->max_relations * dim), BLOCK_SIZE>>>(
        dao->d_relation_embeddings, d_grad_rel, lr, reg, dao->max_relations * dim);

    // Normalize if requested
    if (config->normalize_embeddings) {
        kernel_kg_normalize_embeddings<<<dao->max_entities, BLOCK_SIZE>>>(
            dao->d_entity_embeddings, dim, dao->max_entities);
        kernel_kg_normalize_embeddings<<<dao->max_relations, BLOCK_SIZE>>>(
            dao->d_relation_embeddings, dim, dao->max_relations);
    }

    cudaDeviceSynchronize();

    // Success cleanup
    cudaFree(d_head); cudaFree(d_rel); cudaFree(d_tail); cudaFree(d_neg_tail);
    cudaFree(d_grad_head); cudaFree(d_grad_rel); cudaFree(d_grad_tail); cudaFree(d_grad_neg);
    cudaFree(d_head_idx); cudaFree(d_rel_idx); cudaFree(d_tail_idx); cudaFree(d_neg_idx);

    return 0;

train_cleanup:
    cudaFree(d_head); cudaFree(d_rel); cudaFree(d_tail); cudaFree(d_neg_tail);
    cudaFree(d_grad_head); cudaFree(d_grad_rel); cudaFree(d_grad_tail); cudaFree(d_grad_neg);
    cudaFree(d_head_idx); cudaFree(d_rel_idx); cudaFree(d_tail_idx); cudaFree(d_neg_idx);
    return -1;
}

int nimcp_kg_transe_score(
    nimcp_knowledge_embedding_dao_t* dao,
    int head, int relation, int tail,
    float* score_out)
{
    if (!dao || !score_out || head < 0 || relation < 0 || tail < 0) {
        return -1;
    }

    if (head >= dao->max_entities || relation >= dao->max_relations || tail >= dao->max_entities) {
        return -1;
    }

    int dim = dao->embedding_dim;

    // Read embeddings
    float* h_emb = (float*)nimcp_malloc(dim * sizeof(float));
    float* r_emb = (float*)nimcp_malloc(dim * sizeof(float));
    float* t_emb = (float*)nimcp_malloc(dim * sizeof(float));
    if (!h_emb || !r_emb || !t_emb) {
        nimcp_free(h_emb);
        nimcp_free(r_emb);
        nimcp_free(t_emb);
        return -1;
    }

    cudaMemcpy(h_emb, dao->d_entity_embeddings + head * dim, dim * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(r_emb, dao->d_relation_embeddings + relation * dim, dim * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(t_emb, dao->d_entity_embeddings + tail * dim, dim * sizeof(float), cudaMemcpyDeviceToHost);

    // Compute ||h + r - t||
    float sum = 0.0f;
    for (int d = 0; d < dim; d++) {
        float diff = h_emb[d] + r_emb[d] - t_emb[d];
        sum += diff * diff;
    }
    *score_out = sqrtf(sum);

    nimcp_free(h_emb);
    nimcp_free(r_emb);
    nimcp_free(t_emb);

    return 0;
}

int nimcp_kg_predict_tail(
    nimcp_knowledge_embedding_dao_t* dao,
    int head, int relation, int k,
    int* predictions, float* scores)
{
    if (!dao || !predictions || !scores || head < 0 || relation < 0 || k <= 0) {
        return -1;
    }

    int dim = dao->embedding_dim;

    // Compute h + r
    float* h_emb = (float*)nimcp_malloc(dim * sizeof(float));
    float* r_emb = (float*)nimcp_malloc(dim * sizeof(float));
    float* query = (float*)nimcp_malloc(dim * sizeof(float));
    if (!h_emb || !r_emb || !query) {
        nimcp_free(h_emb);
        nimcp_free(r_emb);
        nimcp_free(query);
        return -1;
    }

    cudaMemcpy(h_emb, dao->d_entity_embeddings + head * dim, dim * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(r_emb, dao->d_relation_embeddings + relation * dim, dim * sizeof(float), cudaMemcpyDeviceToHost);

    for (int d = 0; d < dim; d++) {
        query[d] = h_emb[d] + r_emb[d];
    }

    // Find k most similar entities to (h + r)
    // Lower distance is better, so we negate for similarity-based search
    int result = dao->find_similar(dao, query, k, predictions, scores);

    // Convert similarity scores to distances (lower is better)
    for (int i = 0; i < k; i++) {
        if (predictions[i] >= 0) {
            scores[i] = 1.0f - scores[i];  // Approximate distance
        }
    }

    nimcp_free(h_emb);
    nimcp_free(r_emb);
    nimcp_free(query);

    return result;
}

int nimcp_kg_predict_head(
    nimcp_knowledge_embedding_dao_t* dao,
    int relation, int tail, int k,
    int* predictions, float* scores)
{
    if (!dao || !predictions || !scores || relation < 0 || tail < 0 || k <= 0) {
        return -1;
    }

    int dim = dao->embedding_dim;

    // Compute t - r (looking for h such that h + r ≈ t)
    float* t_emb = (float*)nimcp_malloc(dim * sizeof(float));
    float* r_emb = (float*)nimcp_malloc(dim * sizeof(float));
    float* query = (float*)nimcp_malloc(dim * sizeof(float));
    if (!t_emb || !r_emb || !query) {
        nimcp_free(t_emb);
        nimcp_free(r_emb);
        nimcp_free(query);
        return -1;
    }

    cudaMemcpy(t_emb, dao->d_entity_embeddings + tail * dim, dim * sizeof(float), cudaMemcpyDeviceToHost);
    cudaMemcpy(r_emb, dao->d_relation_embeddings + relation * dim, dim * sizeof(float), cudaMemcpyDeviceToHost);

    for (int d = 0; d < dim; d++) {
        query[d] = t_emb[d] - r_emb[d];
    }

    // Find k most similar entities to (t - r)
    int result = dao->find_similar(dao, query, k, predictions, scores);

    // Convert similarity scores to distances
    for (int i = 0; i < k; i++) {
        if (predictions[i] >= 0) {
            scores[i] = 1.0f - scores[i];
        }
    }

    nimcp_free(t_emb);
    nimcp_free(r_emb);
    nimcp_free(query);

    return result;
}

#else  // !NIMCP_ENABLE_CUDA

// Stub implementations when CUDA is not enabled
#include "gpu/knowledge/nimcp_knowledge_graph_gpu.h"
#include "utils/logging/nimcp_logging.h"

#define LOG_MODULE "KNOWLEDGE_GPU"

nimcp_gpu_knowledge_graph_t* nimcp_gpu_knowledge_graph_create(
    nimcp_gpu_context_t* ctx,
    const uint32_t* row_offsets,
    const uint32_t* col_indices,
    const float* edge_weights,
    uint32_t num_nodes,
    uint32_t num_edges,
    uint32_t embed_dim)
{
    (void)ctx; (void)row_offsets; (void)col_indices; (void)edge_weights;
    (void)num_nodes; (void)num_edges; (void)embed_dim;
    LOG_ERROR("CUDA not enabled - cannot create GPU knowledge graph");
    return NULL;
}

void nimcp_gpu_knowledge_graph_destroy(nimcp_gpu_knowledge_graph_t* graph)
{
    (void)graph;
}

bool nimcp_gpu_knowledge_graph_set_embeddings(nimcp_gpu_knowledge_graph_t* graph, const float* embeddings)
{
    (void)graph; (void)embeddings;
    LOG_ERROR("CUDA not enabled");
    return false;
}

bool nimcp_gpu_knowledge_graph_set_hyperbolic_embeddings(nimcp_gpu_knowledge_graph_t* graph, const float* embeddings)
{
    (void)graph; (void)embeddings;
    LOG_ERROR("CUDA not enabled");
    return false;
}

bool nimcp_gpu_bfs(nimcp_gpu_knowledge_graph_t* graph, uint32_t source, int32_t max_depth, nimcp_graph_traversal_result_t* result)
{
    (void)graph; (void)source; (void)max_depth; (void)result;
    LOG_ERROR("CUDA not enabled");
    return false;
}

bool nimcp_gpu_bfs_multi_source(nimcp_gpu_knowledge_graph_t* graph, const uint32_t* sources, uint32_t num_sources, int32_t max_depth, nimcp_graph_traversal_result_t* result)
{
    (void)graph; (void)sources; (void)num_sources; (void)max_depth; (void)result;
    LOG_ERROR("CUDA not enabled");
    return false;
}

bool nimcp_gpu_dfs(nimcp_gpu_knowledge_graph_t* graph, uint32_t source, int32_t max_depth, nimcp_graph_traversal_result_t* result)
{
    (void)graph; (void)source; (void)max_depth; (void)result;
    LOG_ERROR("CUDA not enabled");
    return false;
}

bool nimcp_gpu_shortest_path(nimcp_gpu_knowledge_graph_t* graph, uint32_t source, uint32_t target, uint32_t* path, uint32_t* len)
{
    (void)graph; (void)source; (void)target; (void)path; (void)len;
    LOG_ERROR("CUDA not enabled");
    return false;
}

void nimcp_graph_traversal_result_destroy(nimcp_graph_traversal_result_t* result)
{
    (void)result;
}

bool nimcp_gpu_node_similarity(nimcp_gpu_knowledge_graph_t* graph, uint32_t a, uint32_t b, float* sim)
{
    (void)graph; (void)a; (void)b; (void)sim;
    LOG_ERROR("CUDA not enabled");
    return false;
}

bool nimcp_gpu_pairwise_similarity(nimcp_gpu_knowledge_graph_t* graph, const uint32_t* indices, uint32_t n, nimcp_gpu_tensor_t* out)
{
    (void)graph; (void)indices; (void)n; (void)out;
    LOG_ERROR("CUDA not enabled");
    return false;
}

bool nimcp_gpu_knn_similarity(nimcp_gpu_knowledge_graph_t* graph, uint32_t query, uint32_t k, nimcp_similarity_result_t* result)
{
    (void)graph; (void)query; (void)k; (void)result;
    LOG_ERROR("CUDA not enabled");
    return false;
}

bool nimcp_gpu_knn_similarity_embedding(nimcp_gpu_knowledge_graph_t* graph, const float* emb, uint32_t k, nimcp_similarity_result_t* result)
{
    (void)graph; (void)emb; (void)k; (void)result;
    LOG_ERROR("CUDA not enabled");
    return false;
}

void nimcp_similarity_result_destroy(nimcp_similarity_result_t* result)
{
    (void)result;
}

bool nimcp_gpu_embedding_update(nimcp_gpu_knowledge_graph_t* graph, const nimcp_gpu_tensor_t* grad, float lr)
{
    (void)graph; (void)grad; (void)lr;
    LOG_ERROR("CUDA not enabled");
    return false;
}

bool nimcp_gpu_triplet_loss(nimcp_gpu_knowledge_graph_t* graph, const uint32_t* a, const uint32_t* p, const uint32_t* n, uint32_t batch, float margin, float* loss)
{
    (void)graph; (void)a; (void)p; (void)n; (void)batch; (void)margin; (void)loss;
    LOG_ERROR("CUDA not enabled");
    return false;
}

bool nimcp_gpu_normalize_embeddings(nimcp_gpu_knowledge_graph_t* graph)
{
    (void)graph;
    LOG_ERROR("CUDA not enabled");
    return false;
}

bool nimcp_gpu_aggregate_neighbors(nimcp_gpu_knowledge_graph_t* graph, nimcp_aggregate_mode_t mode, nimcp_gpu_tensor_t* out)
{
    (void)graph; (void)mode; (void)out;
    LOG_ERROR("CUDA not enabled");
    return false;
}

bool nimcp_gpu_aggregate_attention(nimcp_gpu_knowledge_graph_t* graph, const nimcp_gpu_tensor_t* q, const nimcp_gpu_tensor_t* k, nimcp_gpu_tensor_t* out)
{
    (void)graph; (void)q; (void)k; (void)out;
    LOG_ERROR("CUDA not enabled");
    return false;
}

bool nimcp_gpu_multi_hop_aggregate(nimcp_gpu_knowledge_graph_t* graph, uint32_t hops, nimcp_aggregate_mode_t mode, nimcp_gpu_tensor_t* out)
{
    (void)graph; (void)hops; (void)mode; (void)out;
    LOG_ERROR("CUDA not enabled");
    return false;
}

bool nimcp_gpu_subgraph_match(nimcp_gpu_knowledge_graph_t* graph, nimcp_gpu_knowledge_graph_t* pattern, uint32_t max, nimcp_subgraph_match_result_t* result)
{
    (void)graph; (void)pattern; (void)max; (void)result;
    LOG_ERROR("CUDA not enabled");
    return false;
}

bool nimcp_gpu_subgraph_match_approximate(nimcp_gpu_knowledge_graph_t* graph, nimcp_gpu_knowledge_graph_t* pattern, float thresh, uint32_t max, nimcp_subgraph_match_result_t* result)
{
    (void)graph; (void)pattern; (void)thresh; (void)max; (void)result;
    LOG_ERROR("CUDA not enabled");
    return false;
}

void nimcp_subgraph_match_result_destroy(nimcp_subgraph_match_result_t* result)
{
    (void)result;
}

bool nimcp_gpu_hyperbolic_distance(nimcp_gpu_knowledge_graph_t* graph, uint32_t a, uint32_t b, float* dist)
{
    (void)graph; (void)a; (void)b; (void)dist;
    LOG_ERROR("CUDA not enabled");
    return false;
}

bool nimcp_gpu_hyperbolic_pairwise_distance(nimcp_gpu_knowledge_graph_t* graph, const uint32_t* indices, uint32_t n, nimcp_gpu_tensor_t* out)
{
    (void)graph; (void)indices; (void)n; (void)out;
    LOG_ERROR("CUDA not enabled");
    return false;
}

bool nimcp_gpu_hyperbolic_sgd_step(nimcp_gpu_knowledge_graph_t* graph, const nimcp_gpu_tensor_t* grad, float lr)
{
    (void)graph; (void)grad; (void)lr;
    LOG_ERROR("CUDA not enabled");
    return false;
}

bool nimcp_gpu_euclidean_to_hyperbolic(nimcp_gpu_knowledge_graph_t* graph)
{
    (void)graph;
    LOG_ERROR("CUDA not enabled");
    return false;
}

bool nimcp_gpu_hyperbolic_knn(nimcp_gpu_knowledge_graph_t* graph, uint32_t query, uint32_t k, nimcp_similarity_result_t* result)
{
    (void)graph; (void)query; (void)k; (void)result;
    LOG_ERROR("CUDA not enabled");
    return false;
}

bool nimcp_gpu_knowledge_graph_stats(nimcp_gpu_knowledge_graph_t* graph, float* avg_deg, uint32_t* max_deg, float* norm)
{
    (void)graph; (void)avg_deg; (void)max_deg; (void)norm;
    LOG_ERROR("CUDA not enabled");
    return false;
}

bool nimcp_gpu_knowledge_graph_is_valid(const nimcp_gpu_knowledge_graph_t* graph)
{
    (void)graph;
    return false;
}

// Stub implementations for new embedding DAO APIs
nimcp_knowledge_embedding_dao_t* nimcp_knowledge_embedding_dao_create(void* gpu_ctx, int max_entities, int max_relations, int embedding_dim)
{
    (void)gpu_ctx; (void)max_entities; (void)max_relations; (void)embedding_dim;
    LOG_ERROR("CUDA not enabled");
    return NULL;
}

void nimcp_knowledge_embedding_dao_destroy(nimcp_knowledge_embedding_dao_t* dao)
{
    (void)dao;
}

int nimcp_kg_semantic_search(nimcp_knowledge_embedding_dao_t* dao, const float* query_embedding, int k, nimcp_kg_result_t* result)
{
    (void)dao; (void)query_embedding; (void)k; (void)result;
    LOG_ERROR("CUDA not enabled");
    return -1;
}

int nimcp_kg_find_path(nimcp_knowledge_embedding_dao_t* dao, int source_entity, int target_entity, int max_hops, nimcp_kg_result_t* result)
{
    (void)dao; (void)source_entity; (void)target_entity; (void)max_hops; (void)result;
    LOG_ERROR("CUDA not enabled");
    return -1;
}

int nimcp_kg_pattern_match(nimcp_knowledge_embedding_dao_t* dao, const nimcp_kg_query_t* pattern, nimcp_kg_result_t* result)
{
    (void)dao; (void)pattern; (void)result;
    LOG_ERROR("CUDA not enabled");
    return -1;
}

void nimcp_kg_result_destroy(nimcp_kg_result_t* result)
{
    (void)result;
}

int nimcp_kg_train_step(nimcp_knowledge_embedding_dao_t* dao, const int* head_entities, const int* relations, const int* tail_entities, int batch_size, nimcp_kg_train_config_t* config)
{
    (void)dao; (void)head_entities; (void)relations; (void)tail_entities; (void)batch_size; (void)config;
    LOG_ERROR("CUDA not enabled");
    return -1;
}

int nimcp_kg_transe_score(nimcp_knowledge_embedding_dao_t* dao, int head, int relation, int tail, float* score_out)
{
    (void)dao; (void)head; (void)relation; (void)tail; (void)score_out;
    LOG_ERROR("CUDA not enabled");
    return -1;
}

int nimcp_kg_predict_tail(nimcp_knowledge_embedding_dao_t* dao, int head, int relation, int k, int* predictions, float* scores)
{
    (void)dao; (void)head; (void)relation; (void)k; (void)predictions; (void)scores;
    LOG_ERROR("CUDA not enabled");
    return -1;
}

int nimcp_kg_predict_head(nimcp_knowledge_embedding_dao_t* dao, int relation, int tail, int k, int* predictions, float* scores)
{
    (void)dao; (void)relation; (void)tail; (void)k; (void)predictions; (void)scores;
    LOG_ERROR("CUDA not enabled");
    return -1;
}

#endif  // NIMCP_ENABLE_CUDA
