/**
 * @file nimcp_warp_primitives.cuh
 * @brief Shared Warp and Block Reduction Primitives
 *
 * WHAT: Common warp-level and block-level reduction operations
 * WHY:  Eliminate duplication across 5+ GPU kernel files
 * HOW:  Optimized inline functions using warp shuffle intrinsics
 *
 * USAGE:
 *   #include "gpu/common/nimcp_warp_primitives.cuh"
 *
 *   __global__ void reduce_sum(float* data, float* result, int n) {
 *       float val = (idx < n) ? data[idx] : 0.0f;
 *       val = nimcp_warp_reduce_sum(val);
 *       // ... block reduction ...
 *   }
 *
 * CONSOLIDATES DUPLICATES FROM:
 *   - tensor/nimcp_tensor_kernels.cu
 *   - training/nimcp_gradient_kernels.cu
 *   - snn/nimcp_snn_kernels.cu
 *   - swarm/nimcp_swarm_memory_gpu.cu
 *   - knowledge/nimcp_knowledge_graph_kernels.cu
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_WARP_PRIMITIVES_CUH
#define NIMCP_WARP_PRIMITIVES_CUH

#ifdef __CUDACC__

#include <cuda_runtime.h>
#include <float.h>

//=============================================================================
// Constants
//=============================================================================

#define NIMCP_WARP_SIZE 32
#define NIMCP_FULL_WARP_MASK 0xFFFFFFFF

//=============================================================================
// Warp-Level Reductions
//=============================================================================

/**
 * @brief Warp-level sum reduction using shuffle
 *
 * All threads in warp participate; result is broadcast to all.
 *
 * @param val Thread's input value
 * @return Sum of all values in warp
 */
__device__ __forceinline__ float nimcp_warp_reduce_sum(float val)
{
    #pragma unroll
    for (int offset = NIMCP_WARP_SIZE / 2; offset > 0; offset /= 2) {
        val += __shfl_down_sync(NIMCP_FULL_WARP_MASK, val, offset);
    }
    return val;
}

/**
 * @brief Warp-level maximum reduction using shuffle
 *
 * @param val Thread's input value
 * @return Maximum of all values in warp
 */
__device__ __forceinline__ float nimcp_warp_reduce_max(float val)
{
    #pragma unroll
    for (int offset = NIMCP_WARP_SIZE / 2; offset > 0; offset /= 2) {
        val = fmaxf(val, __shfl_down_sync(NIMCP_FULL_WARP_MASK, val, offset));
    }
    return val;
}

/**
 * @brief Warp-level minimum reduction using shuffle
 *
 * @param val Thread's input value
 * @return Minimum of all values in warp
 */
__device__ __forceinline__ float nimcp_warp_reduce_min(float val)
{
    #pragma unroll
    for (int offset = NIMCP_WARP_SIZE / 2; offset > 0; offset /= 2) {
        val = fminf(val, __shfl_down_sync(NIMCP_FULL_WARP_MASK, val, offset));
    }
    return val;
}

/**
 * @brief Warp-level product reduction using shuffle
 *
 * @param val Thread's input value
 * @return Product of all values in warp
 */
__device__ __forceinline__ float nimcp_warp_reduce_prod(float val)
{
    #pragma unroll
    for (int offset = NIMCP_WARP_SIZE / 2; offset > 0; offset /= 2) {
        val *= __shfl_down_sync(NIMCP_FULL_WARP_MASK, val, offset);
    }
    return val;
}

/**
 * @brief Warp-level argmax reduction
 *
 * @param val Thread's input value
 * @param idx Thread's index
 * @param out_idx Output: index of maximum value
 * @return Maximum value
 */
__device__ __forceinline__ float nimcp_warp_reduce_argmax(float val, int idx, int* out_idx)
{
    #pragma unroll
    for (int offset = NIMCP_WARP_SIZE / 2; offset > 0; offset /= 2) {
        float other_val = __shfl_down_sync(NIMCP_FULL_WARP_MASK, val, offset);
        int other_idx = __shfl_down_sync(NIMCP_FULL_WARP_MASK, idx, offset);
        if (other_val > val) {
            val = other_val;
            idx = other_idx;
        }
    }
    *out_idx = idx;
    return val;
}

//=============================================================================
// Integer Warp Reductions
//=============================================================================

/**
 * @brief Warp-level integer sum reduction
 */
__device__ __forceinline__ int nimcp_warp_reduce_sum_int(int val)
{
    #pragma unroll
    for (int offset = NIMCP_WARP_SIZE / 2; offset > 0; offset /= 2) {
        val += __shfl_down_sync(NIMCP_FULL_WARP_MASK, val, offset);
    }
    return val;
}

/**
 * @brief Warp-level integer maximum reduction
 */
__device__ __forceinline__ int nimcp_warp_reduce_max_int(int val)
{
    #pragma unroll
    for (int offset = NIMCP_WARP_SIZE / 2; offset > 0; offset /= 2) {
        val = max(val, __shfl_down_sync(NIMCP_FULL_WARP_MASK, val, offset));
    }
    return val;
}

/**
 * @brief Warp-level integer minimum reduction
 */
__device__ __forceinline__ int nimcp_warp_reduce_min_int(int val)
{
    #pragma unroll
    for (int offset = NIMCP_WARP_SIZE / 2; offset > 0; offset /= 2) {
        val = min(val, __shfl_down_sync(NIMCP_FULL_WARP_MASK, val, offset));
    }
    return val;
}

//=============================================================================
// Block-Level Reductions
//=============================================================================

/**
 * @brief Block-level sum reduction using shared memory
 *
 * Requires shared memory array of size (blockDim.x / WARP_SIZE).
 * Result is only valid in thread 0.
 *
 * @param val Thread's input value
 * @param shared Shared memory array [blockDim.x / WARP_SIZE]
 * @return Sum (valid only in thread 0)
 */
__device__ __forceinline__ float nimcp_block_reduce_sum(float val, float* shared)
{
    int lane = threadIdx.x % NIMCP_WARP_SIZE;
    int wid = threadIdx.x / NIMCP_WARP_SIZE;

    // Warp-level reduction
    val = nimcp_warp_reduce_sum(val);

    // Write warp result to shared memory
    if (lane == 0) {
        shared[wid] = val;
    }
    __syncthreads();

    // First warp reduces warp results
    int num_warps = (blockDim.x + NIMCP_WARP_SIZE - 1) / NIMCP_WARP_SIZE;
    val = (threadIdx.x < num_warps) ? shared[lane] : 0.0f;
    if (wid == 0) {
        val = nimcp_warp_reduce_sum(val);
    }

    return val;
}

/**
 * @brief Block-level maximum reduction using shared memory
 *
 * @param val Thread's input value
 * @param shared Shared memory array [blockDim.x / WARP_SIZE]
 * @return Maximum (valid only in thread 0)
 */
__device__ __forceinline__ float nimcp_block_reduce_max(float val, float* shared)
{
    int lane = threadIdx.x % NIMCP_WARP_SIZE;
    int wid = threadIdx.x / NIMCP_WARP_SIZE;

    // Warp-level reduction
    val = nimcp_warp_reduce_max(val);

    // Write warp result to shared memory
    if (lane == 0) {
        shared[wid] = val;
    }
    __syncthreads();

    // First warp reduces warp results
    int num_warps = (blockDim.x + NIMCP_WARP_SIZE - 1) / NIMCP_WARP_SIZE;
    val = (threadIdx.x < num_warps) ? shared[lane] : -FLT_MAX;
    if (wid == 0) {
        val = nimcp_warp_reduce_max(val);
    }

    return val;
}

/**
 * @brief Block-level minimum reduction using shared memory
 *
 * @param val Thread's input value
 * @param shared Shared memory array [blockDim.x / WARP_SIZE]
 * @return Minimum (valid only in thread 0)
 */
__device__ __forceinline__ float nimcp_block_reduce_min(float val, float* shared)
{
    int lane = threadIdx.x % NIMCP_WARP_SIZE;
    int wid = threadIdx.x / NIMCP_WARP_SIZE;

    // Warp-level reduction
    val = nimcp_warp_reduce_min(val);

    // Write warp result to shared memory
    if (lane == 0) {
        shared[wid] = val;
    }
    __syncthreads();

    // First warp reduces warp results
    int num_warps = (blockDim.x + NIMCP_WARP_SIZE - 1) / NIMCP_WARP_SIZE;
    val = (threadIdx.x < num_warps) ? shared[lane] : FLT_MAX;
    if (wid == 0) {
        val = nimcp_warp_reduce_min(val);
    }

    return val;
}

//=============================================================================
// Warp Broadcast
//=============================================================================

/**
 * @brief Broadcast value from lane 0 to all lanes in warp
 *
 * @param val Value (only lane 0's value is used)
 * @return Broadcast value
 */
__device__ __forceinline__ float nimcp_warp_broadcast(float val)
{
    return __shfl_sync(NIMCP_FULL_WARP_MASK, val, 0);
}

/**
 * @brief Broadcast value from specified lane to all lanes
 *
 * @param val Value
 * @param src_lane Source lane index
 * @return Broadcast value
 */
__device__ __forceinline__ float nimcp_warp_broadcast_lane(float val, int src_lane)
{
    return __shfl_sync(NIMCP_FULL_WARP_MASK, val, src_lane);
}

//=============================================================================
// Warp Scan (Prefix Sum)
//=============================================================================

/**
 * @brief Warp-level inclusive prefix sum
 *
 * @param val Thread's input value
 * @return Inclusive prefix sum
 */
__device__ __forceinline__ float nimcp_warp_scan_inclusive(float val)
{
    #pragma unroll
    for (int offset = 1; offset < NIMCP_WARP_SIZE; offset *= 2) {
        float n = __shfl_up_sync(NIMCP_FULL_WARP_MASK, val, offset);
        if ((threadIdx.x % NIMCP_WARP_SIZE) >= offset) {
            val += n;
        }
    }
    return val;
}

/**
 * @brief Warp-level exclusive prefix sum
 *
 * @param val Thread's input value
 * @return Exclusive prefix sum
 */
__device__ __forceinline__ float nimcp_warp_scan_exclusive(float val)
{
    float inclusive = nimcp_warp_scan_inclusive(val);
    return inclusive - val;
}

//=============================================================================
// Warp Vote Functions
//=============================================================================

/**
 * @brief Check if all threads in warp satisfy predicate
 *
 * @param predicate Thread's boolean condition
 * @return true if all threads have predicate==true
 */
__device__ __forceinline__ bool nimcp_warp_all(bool predicate)
{
    return __all_sync(NIMCP_FULL_WARP_MASK, predicate);
}

/**
 * @brief Check if any thread in warp satisfies predicate
 *
 * @param predicate Thread's boolean condition
 * @return true if any thread has predicate==true
 */
__device__ __forceinline__ bool nimcp_warp_any(bool predicate)
{
    return __any_sync(NIMCP_FULL_WARP_MASK, predicate);
}

/**
 * @brief Count threads in warp with predicate true
 *
 * @param predicate Thread's boolean condition
 * @return Number of threads with predicate==true
 */
__device__ __forceinline__ int nimcp_warp_count(bool predicate)
{
    return __popc(__ballot_sync(NIMCP_FULL_WARP_MASK, predicate));
}

//=============================================================================
// Atomic Helpers
//=============================================================================

/**
 * @brief Atomic max for float using CAS
 *
 * CUDA doesn't have native atomicMax for float.
 *
 * @param address Memory location
 * @param val Value to compare
 * @return Old value at address
 */
__device__ __forceinline__ float nimcp_atomic_max_float(float* address, float val)
{
    int* address_as_int = (int*)address;
    int old = *address_as_int;
    int assumed;
    do {
        assumed = old;
        old = atomicCAS(address_as_int, assumed,
                        __float_as_int(fmaxf(val, __int_as_float(assumed))));
    } while (assumed != old);
    return __int_as_float(old);
}

/**
 * @brief Atomic min for float using CAS
 *
 * @param address Memory location
 * @param val Value to compare
 * @return Old value at address
 */
__device__ __forceinline__ float nimcp_atomic_min_float(float* address, float val)
{
    int* address_as_int = (int*)address;
    int old = *address_as_int;
    int assumed;
    do {
        assumed = old;
        old = atomicCAS(address_as_int, assumed,
                        __float_as_int(fminf(val, __int_as_float(assumed))));
    } while (assumed != old);
    return __int_as_float(old);
}

#endif // __CUDACC__

#endif // NIMCP_WARP_PRIMITIVES_CUH
