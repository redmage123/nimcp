/**
 * @file nimcp_fuzzy_relation_kernels.cu
 * @brief GPU Fuzzy Relation Composition Kernels
 *
 * WHAT: GPU-accelerated fuzzy relation operations
 * WHY:  Fast relation composition for fuzzy inference
 * HOW:  Tiled matrix operations with max-min composition
 *
 * Implements:
 *   - Max-min composition
 *   - Max-product composition
 *   - Other composition operators
 *   - Relation creation (Cartesian product)
 *   - Relation projection
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

#include <cuda_runtime.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <float.h>

#include "gpu/fuzzy/nimcp_fuzzy_gpu.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"

//=============================================================================
// Thread-Local Error Storage
//=============================================================================

static __thread char g_relation_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_relation_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_relation_error, sizeof(g_relation_error), fmt, args);
    va_end(args);
}

//=============================================================================
// Constants
//=============================================================================

#define TILE_SIZE 16

//=============================================================================
// Composition Type Enum
//=============================================================================

typedef enum {
    FUZZY_COMPOSE_MAX_MIN = 0,
    FUZZY_COMPOSE_MAX_PRODUCT,
    FUZZY_COMPOSE_MAX_AVG,
    FUZZY_COMPOSE_MIN_MAX
} fuzzy_compose_type_t;

//=============================================================================
// Device Helper Functions
//=============================================================================

/**
 * @brief T-norm minimum
 */
__device__ __forceinline__ float tnorm_min(float a, float b) {
    return fminf(a, b);
}

/**
 * @brief T-norm product
 */
__device__ __forceinline__ float tnorm_product(float a, float b) {
    return a * b;
}

/**
 * @brief T-conorm maximum
 */
__device__ __forceinline__ float tconorm_max(float a, float b) {
    return fmaxf(a, b);
}

//=============================================================================
// Relation Composition Kernels
//=============================================================================

/**
 * @brief Max-min composition: C[i,k] = max_j(min(A[i,j], B[j,k]))
 *
 * Uses tiled approach for cache efficiency
 */
__global__ void kernel_relation_compose_max_min(
    const float* __restrict__ rel_a,    // [rows_a x cols_a]
    const float* __restrict__ rel_b,    // [cols_a x cols_b]
    float* __restrict__ rel_out,        // [rows_a x cols_b]
    uint32_t rows_a,
    uint32_t cols_a,
    uint32_t cols_b)
{
    __shared__ float tile_a[TILE_SIZE][TILE_SIZE];
    __shared__ float tile_b[TILE_SIZE][TILE_SIZE];

    uint32_t row = blockIdx.y * TILE_SIZE + threadIdx.y;
    uint32_t col = blockIdx.x * TILE_SIZE + threadIdx.x;

    float max_val = 0.0f;

    // Loop over tiles
    for (uint32_t t = 0; t < (cols_a + TILE_SIZE - 1) / TILE_SIZE; t++) {
        // Load tile_a
        uint32_t a_col = t * TILE_SIZE + threadIdx.x;
        if (row < rows_a && a_col < cols_a) {
            tile_a[threadIdx.y][threadIdx.x] = rel_a[row * cols_a + a_col];
        } else {
            tile_a[threadIdx.y][threadIdx.x] = 0.0f;
        }

        // Load tile_b
        uint32_t b_row = t * TILE_SIZE + threadIdx.y;
        if (b_row < cols_a && col < cols_b) {
            tile_b[threadIdx.y][threadIdx.x] = rel_b[b_row * cols_b + col];
        } else {
            tile_b[threadIdx.y][threadIdx.x] = 0.0f;
        }

        __syncthreads();

        // Compute max-min for this tile
        for (uint32_t k = 0; k < TILE_SIZE; k++) {
            float min_val = tnorm_min(tile_a[threadIdx.y][k], tile_b[k][threadIdx.x]);
            max_val = tconorm_max(max_val, min_val);
        }

        __syncthreads();
    }

    if (row < rows_a && col < cols_b) {
        rel_out[row * cols_b + col] = max_val;
    }
}

/**
 * @brief Max-product composition: C[i,k] = max_j(A[i,j] * B[j,k])
 */
__global__ void kernel_relation_compose_max_product(
    const float* __restrict__ rel_a,
    const float* __restrict__ rel_b,
    float* __restrict__ rel_out,
    uint32_t rows_a,
    uint32_t cols_a,
    uint32_t cols_b)
{
    __shared__ float tile_a[TILE_SIZE][TILE_SIZE];
    __shared__ float tile_b[TILE_SIZE][TILE_SIZE];

    uint32_t row = blockIdx.y * TILE_SIZE + threadIdx.y;
    uint32_t col = blockIdx.x * TILE_SIZE + threadIdx.x;

    float max_val = 0.0f;

    for (uint32_t t = 0; t < (cols_a + TILE_SIZE - 1) / TILE_SIZE; t++) {
        uint32_t a_col = t * TILE_SIZE + threadIdx.x;
        if (row < rows_a && a_col < cols_a) {
            tile_a[threadIdx.y][threadIdx.x] = rel_a[row * cols_a + a_col];
        } else {
            tile_a[threadIdx.y][threadIdx.x] = 0.0f;
        }

        uint32_t b_row = t * TILE_SIZE + threadIdx.y;
        if (b_row < cols_a && col < cols_b) {
            tile_b[threadIdx.y][threadIdx.x] = rel_b[b_row * cols_b + col];
        } else {
            tile_b[threadIdx.y][threadIdx.x] = 0.0f;
        }

        __syncthreads();

        for (uint32_t k = 0; k < TILE_SIZE; k++) {
            float prod = tnorm_product(tile_a[threadIdx.y][k], tile_b[k][threadIdx.x]);
            max_val = tconorm_max(max_val, prod);
        }

        __syncthreads();
    }

    if (row < rows_a && col < cols_b) {
        rel_out[row * cols_b + col] = max_val;
    }
}

/**
 * @brief Max-average composition: C[i,k] = max_j((A[i,j] + B[j,k]) / 2)
 */
__global__ void kernel_relation_compose_max_avg(
    const float* __restrict__ rel_a,
    const float* __restrict__ rel_b,
    float* __restrict__ rel_out,
    uint32_t rows_a,
    uint32_t cols_a,
    uint32_t cols_b)
{
    __shared__ float tile_a[TILE_SIZE][TILE_SIZE];
    __shared__ float tile_b[TILE_SIZE][TILE_SIZE];

    uint32_t row = blockIdx.y * TILE_SIZE + threadIdx.y;
    uint32_t col = blockIdx.x * TILE_SIZE + threadIdx.x;

    float max_val = 0.0f;

    for (uint32_t t = 0; t < (cols_a + TILE_SIZE - 1) / TILE_SIZE; t++) {
        uint32_t a_col = t * TILE_SIZE + threadIdx.x;
        if (row < rows_a && a_col < cols_a) {
            tile_a[threadIdx.y][threadIdx.x] = rel_a[row * cols_a + a_col];
        } else {
            tile_a[threadIdx.y][threadIdx.x] = 0.0f;
        }

        uint32_t b_row = t * TILE_SIZE + threadIdx.y;
        if (b_row < cols_a && col < cols_b) {
            tile_b[threadIdx.y][threadIdx.x] = rel_b[b_row * cols_b + col];
        } else {
            tile_b[threadIdx.y][threadIdx.x] = 0.0f;
        }

        __syncthreads();

        for (uint32_t k = 0; k < TILE_SIZE; k++) {
            float avg = 0.5f * (tile_a[threadIdx.y][k] + tile_b[k][threadIdx.x]);
            max_val = tconorm_max(max_val, avg);
        }

        __syncthreads();
    }

    if (row < rows_a && col < cols_b) {
        rel_out[row * cols_b + col] = max_val;
    }
}

//=============================================================================
// Relation Creation Kernels
//=============================================================================

/**
 * @brief Create Cartesian product relation
 *
 * R[i,j] = T-norm(A[i], B[j])
 */
__global__ void kernel_relation_cartesian_min(
    const float* __restrict__ set_a,    // [n_a]
    const float* __restrict__ set_b,    // [n_b]
    float* __restrict__ relation,       // [n_a x n_b]
    uint32_t n_a,
    uint32_t n_b)
{
    uint32_t i = blockIdx.y * blockDim.y + threadIdx.y;
    uint32_t j = blockIdx.x * blockDim.x + threadIdx.x;

    if (i < n_a && j < n_b) {
        relation[i * n_b + j] = tnorm_min(set_a[i], set_b[j]);
    }
}

/**
 * @brief Create Cartesian product with product T-norm
 */
__global__ void kernel_relation_cartesian_product(
    const float* __restrict__ set_a,
    const float* __restrict__ set_b,
    float* __restrict__ relation,
    uint32_t n_a,
    uint32_t n_b)
{
    uint32_t i = blockIdx.y * blockDim.y + threadIdx.y;
    uint32_t j = blockIdx.x * blockDim.x + threadIdx.x;

    if (i < n_a && j < n_b) {
        relation[i * n_b + j] = tnorm_product(set_a[i], set_b[j]);
    }
}

//=============================================================================
// Relation Projection Kernels
//=============================================================================

/**
 * @brief Project relation onto first dimension (row-wise max)
 *
 * proj[i] = max_j(R[i,j])
 */
__global__ void kernel_relation_project_first(
    const float* __restrict__ relation,
    float* __restrict__ projection,
    uint32_t rows,
    uint32_t cols)
{
    extern __shared__ float s_max[];

    uint32_t row = blockIdx.x;
    uint32_t tid = threadIdx.x;

    if (row >= rows) return;

    // Find max in row
    float max_val = 0.0f;
    for (uint32_t j = tid; j < cols; j += blockDim.x) {
        max_val = tconorm_max(max_val, relation[row * cols + j]);
    }

    s_max[tid] = max_val;
    __syncthreads();

    // Parallel reduction
    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_max[tid] = tconorm_max(s_max[tid], s_max[tid + s]);
        }
        __syncthreads();
    }

    if (tid == 0) {
        projection[row] = s_max[0];
    }
}

/**
 * @brief Project relation onto second dimension (column-wise max)
 *
 * proj[j] = max_i(R[i,j])
 */
__global__ void kernel_relation_project_second(
    const float* __restrict__ relation,
    float* __restrict__ projection,
    uint32_t rows,
    uint32_t cols)
{
    extern __shared__ float s_max[];

    uint32_t col = blockIdx.x;
    uint32_t tid = threadIdx.x;

    if (col >= cols) return;

    // Find max in column
    float max_val = 0.0f;
    for (uint32_t i = tid; i < rows; i += blockDim.x) {
        max_val = tconorm_max(max_val, relation[i * cols + col]);
    }

    s_max[tid] = max_val;
    __syncthreads();

    // Parallel reduction
    for (uint32_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            s_max[tid] = tconorm_max(s_max[tid], s_max[tid + s]);
        }
        __syncthreads();
    }

    if (tid == 0) {
        projection[col] = s_max[0];
    }
}

//=============================================================================
// Relation Operations Kernels
//=============================================================================

/**
 * @brief Relation union: C[i,j] = max(A[i,j], B[i,j])
 */
__global__ void kernel_relation_union(
    const float* __restrict__ rel_a,
    const float* __restrict__ rel_b,
    float* __restrict__ rel_out,
    uint32_t n)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        rel_out[i] = tconorm_max(rel_a[i], rel_b[i]);
    }
}

/**
 * @brief Relation intersection: C[i,j] = min(A[i,j], B[i,j])
 */
__global__ void kernel_relation_intersection(
    const float* __restrict__ rel_a,
    const float* __restrict__ rel_b,
    float* __restrict__ rel_out,
    uint32_t n)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        rel_out[i] = tnorm_min(rel_a[i], rel_b[i]);
    }
}

/**
 * @brief Relation complement: C[i,j] = 1 - A[i,j]
 */
__global__ void kernel_relation_complement(
    const float* __restrict__ rel_in,
    float* __restrict__ rel_out,
    uint32_t n)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        rel_out[i] = 1.0f - rel_in[i];
    }
}

/**
 * @brief Relation transpose
 */
__global__ void kernel_relation_transpose(
    const float* __restrict__ rel_in,
    float* __restrict__ rel_out,
    uint32_t rows,
    uint32_t cols)
{
    __shared__ float tile[TILE_SIZE][TILE_SIZE + 1];  // +1 to avoid bank conflicts

    uint32_t x = blockIdx.x * TILE_SIZE + threadIdx.x;
    uint32_t y = blockIdx.y * TILE_SIZE + threadIdx.y;

    if (x < cols && y < rows) {
        tile[threadIdx.y][threadIdx.x] = rel_in[y * cols + x];
    }
    __syncthreads();

    x = blockIdx.y * TILE_SIZE + threadIdx.x;
    y = blockIdx.x * TILE_SIZE + threadIdx.y;

    if (x < rows && y < cols) {
        rel_out[y * rows + x] = tile[threadIdx.x][threadIdx.y];
    }
}

//=============================================================================
// Alpha-Cut Kernel
//=============================================================================

/**
 * @brief Alpha-cut of relation: result[i] = (rel[i] >= alpha) ? 1 : 0
 */
__global__ void kernel_relation_alpha_cut(
    const float* __restrict__ rel_in,
    float* __restrict__ rel_out,
    float alpha,
    uint32_t n)
{
    uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        rel_out[i] = (rel_in[i] >= alpha) ? 1.0f : 0.0f;
    }
}

//=============================================================================
// Host API Implementation
//=============================================================================

extern "C" {

bool nimcp_gpu_fuzzy_relation_compose(
    nimcp_gpu_context_t* ctx,
    const float* rel_a, uint32_t rows_a, uint32_t cols_a,
    const float* rel_b, uint32_t cols_b,
    float* rel_out,
    const nimcp_gpu_relation_params_t* params)
{
    // Initialize recovery system if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Parameter validation with recovery attempt
    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        nimcp_gpu_recovery_result_t result;
        if (!nimcp_gpu_try_recover(NULL, GPU_ERROR_CONTEXT_INVALID, cudaSuccess, &result)) {
            set_relation_error("Invalid GPU context");
            NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
            return false;
        }
    }
    if (!rel_a || !rel_b || !rel_out) {
        set_relation_error("Invalid relation pointers");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid relation pointers");
        return false;
    }
    if (rows_a == 0 || cols_a == 0 || cols_b == 0) {
        nimcp_gpu_recovery_result_t result;
        if (!nimcp_gpu_try_recover(NULL, GPU_ERROR_INVALID_PARAMS, cudaSuccess, &result)) {
            set_relation_error("Invalid relation dimensions");
            NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid relation dimensions");
            return false;
        }
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);

    // Pre-declare variables used after potential goto (CUDA C++ requirement)
    dim3 block(TILE_SIZE, TILE_SIZE);
    dim3 grid((cols_b + TILE_SIZE - 1) / TILE_SIZE,
              (rows_a + TILE_SIZE - 1) / TILE_SIZE);
    fuzzy_compose_type_t compose_type = FUZZY_COMPOSE_MAX_MIN;

    // Allocate device memory
    float* d_rel_a = NULL;
    float* d_rel_b = NULL;
    float* d_rel_out = NULL;

    cudaError_t err;

    err = cudaMalloc(&d_rel_a, rows_a * cols_a * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&d_rel_a, rows_a * cols_a * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_compose;
    }

    err = cudaMalloc(&d_rel_b, cols_a * cols_b * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&d_rel_b, cols_a * cols_b * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_compose;
    }

    err = cudaMalloc(&d_rel_out, rows_a * cols_b * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&d_rel_out, rows_a * cols_b * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_compose;
    }

    // Copy to device
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_rel_a, rel_a, rows_a * cols_a * sizeof(float),
                    cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_rel_b, rel_b, cols_a * cols_b * sizeof(float),
                    cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);
    if (params) {
        // Use tnorm field to select composition type
        // tnorm=0 (MIN) => MAX_MIN, tnorm=1 (PRODUCT) => MAX_PRODUCT, etc.
        compose_type = (fuzzy_compose_type_t)params->tnorm;
    }

    switch (compose_type) {
        case FUZZY_COMPOSE_MAX_MIN:
            kernel_relation_compose_max_min<<<grid, block, 0, stream>>>(
                d_rel_a, d_rel_b, d_rel_out, rows_a, cols_a, cols_b);
            break;

        case FUZZY_COMPOSE_MAX_PRODUCT:
            kernel_relation_compose_max_product<<<grid, block, 0, stream>>>(
                d_rel_a, d_rel_b, d_rel_out, rows_a, cols_a, cols_b);
            break;

        case FUZZY_COMPOSE_MAX_AVG:
            kernel_relation_compose_max_avg<<<grid, block, 0, stream>>>(
                d_rel_a, d_rel_b, d_rel_out, rows_a, cols_a, cols_b);
            break;

        default:
            kernel_relation_compose_max_min<<<grid, block, 0, stream>>>(
                d_rel_a, d_rel_b, d_rel_out, rows_a, cols_a, cols_b);
    }

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    // Copy result back
    NIMCP_CUDA_RECOVER(cudaMemcpy(rel_out, d_rel_out, rows_a * cols_b * sizeof(float),
               cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

    cudaFree(d_rel_a);
    cudaFree(d_rel_b);
    cudaFree(d_rel_out);

    return true;

cleanup_compose:
    cudaFree(d_rel_a);
    cudaFree(d_rel_b);
    cudaFree(d_rel_out);
    set_relation_error("Memory allocation failed");
    NIMCP_THROW_GPU(NIMCP_ERROR_NO_MEMORY, 0, (int)err, "Memory allocation failed");
    return false;
}

bool nimcp_gpu_fuzzy_relation_cartesian(
    nimcp_gpu_context_t* ctx,
    const float* set_a,
    const float* set_b,
    float* relation,
    uint32_t n_a,
    uint32_t n_b,
    bool use_product)
{
    // Initialize recovery system if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Parameter validation with recovery attempt
    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        nimcp_gpu_recovery_result_t result;
        if (!nimcp_gpu_try_recover(NULL, GPU_ERROR_CONTEXT_INVALID, cudaSuccess, &result)) {
            set_relation_error("Invalid GPU context");
            NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
            return false;
        }
    }
    if (!set_a || !set_b || !relation) {
        set_relation_error("Invalid pointers");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid pointers");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);

    float* d_set_a = NULL;
    float* d_set_b = NULL;
    float* d_relation = NULL;

    cudaError_t err;

    // Declare grid/block before any goto statements to avoid C++ initialization bypass
    dim3 block(16, 16);
    dim3 grid((n_b + 15) / 16, (n_a + 15) / 16);

    err = cudaMalloc(&d_set_a, n_a * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&d_set_a, n_a * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_cartesian;
    }

    err = cudaMalloc(&d_set_b, n_b * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&d_set_b, n_b * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_cartesian;
    }

    err = cudaMalloc(&d_relation, n_a * n_b * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&d_relation, n_a * n_b * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_cartesian;
    }

    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_set_a, set_a, n_a * sizeof(float),
                    cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_set_b, set_b, n_b * sizeof(float),
                    cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);

    if (use_product) {
        kernel_relation_cartesian_product<<<grid, block, 0, stream>>>(
            d_set_a, d_set_b, d_relation, n_a, n_b);
    } else {
        kernel_relation_cartesian_min<<<grid, block, 0, stream>>>(
            d_set_a, d_set_b, d_relation, n_a, n_b);
    }

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    NIMCP_CUDA_RECOVER(cudaMemcpy(relation, d_relation, n_a * n_b * sizeof(float),
               cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

    cudaFree(d_set_a);
    cudaFree(d_set_b);
    cudaFree(d_relation);

    return true;

cleanup_cartesian:
    cudaFree(d_set_a);
    cudaFree(d_set_b);
    cudaFree(d_relation);
    set_relation_error("Memory allocation failed");
    NIMCP_THROW_GPU(NIMCP_ERROR_NO_MEMORY, 0, (int)err, "Memory allocation failed");
    return false;
}

bool nimcp_gpu_fuzzy_relation_project(
    nimcp_gpu_context_t* ctx,
    const float* relation,
    float* projection,
    uint32_t rows,
    uint32_t cols,
    bool project_first)
{
    // Initialize recovery system if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    // Parameter validation with recovery attempt
    if (!ctx || !nimcp_gpu_context_is_valid(ctx)) {
        nimcp_gpu_recovery_result_t result;
        if (!nimcp_gpu_try_recover(NULL, GPU_ERROR_CONTEXT_INVALID, cudaSuccess, &result)) {
            set_relation_error("Invalid GPU context");
            NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid GPU context");
            return false;
        }
    }
    if (!relation || !projection) {
        set_relation_error("Invalid pointers");
        NIMCP_THROW_GPU(NIMCP_ERROR_INVALID_PARAM, 0, 0, "Invalid pointers");
        return false;
    }

    cudaStream_t stream = nimcp_gpu_get_compute_stream(ctx);
    uint32_t block_size = 256;

    float* d_relation = NULL;
    float* d_projection = NULL;

    cudaError_t err;

    // Declare proj_size before any goto statements to avoid C++ initialization bypass
    uint32_t proj_size = project_first ? rows : cols;

    err = cudaMalloc(&d_relation, rows * cols * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&d_relation, rows * cols * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_project;
    }

    err = cudaMalloc(&d_projection, proj_size * sizeof(float));
    if (err != cudaSuccess) {
        nimcp_gpu_recovery_result_t result;
        if (nimcp_gpu_try_recover(NULL, GPU_ERROR_OUT_OF_MEMORY, err, &result)) {
            err = cudaMalloc(&d_projection, proj_size * sizeof(float));
        }
        if (err != cudaSuccess) goto cleanup_project;
    }

    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_relation, relation, rows * cols * sizeof(float),
                    cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);

    if (project_first) {
        kernel_relation_project_first<<<rows, block_size,
                                        block_size * sizeof(float), stream>>>(
            d_relation, d_projection, rows, cols);
    } else {
        kernel_relation_project_second<<<cols, block_size,
                                         block_size * sizeof(float), stream>>>(
            d_relation, d_projection, rows, cols);
    }

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    NIMCP_CUDA_RECOVER(cudaMemcpy(projection, d_projection, proj_size * sizeof(float),
               cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

    cudaFree(d_relation);
    cudaFree(d_projection);

    return true;

cleanup_project:
    cudaFree(d_relation);
    cudaFree(d_projection);
    set_relation_error("Memory allocation failed");
    NIMCP_THROW_GPU(NIMCP_ERROR_NO_MEMORY, 0, (int)err, "Memory allocation failed");
    return false;
}

const char* nimcp_gpu_fuzzy_relation_get_last_error(void) {
    return g_relation_error;
}

} // extern "C"

#else // !NIMCP_ENABLE_CUDA

//=============================================================================
// CPU Fallback Stubs
//=============================================================================

extern "C" {

bool nimcp_gpu_fuzzy_relation_compose(
    nimcp_gpu_context_t* ctx,
    const float* rel_a, uint32_t rows_a, uint32_t cols_a,
    const float* rel_b, uint32_t cols_b,
    float* rel_out,
    const nimcp_gpu_relation_params_t* params)
{
    (void)ctx; (void)rel_a; (void)rel_b; (void)rel_out;
    (void)rows_a; (void)cols_a; (void)cols_b; (void)params;
    return false;
}

bool nimcp_gpu_fuzzy_relation_cartesian(
    nimcp_gpu_context_t* ctx,
    const float* set_a,
    const float* set_b,
    float* relation,
    uint32_t n_a,
    uint32_t n_b,
    bool use_product)
{
    (void)ctx; (void)set_a; (void)set_b; (void)relation;
    (void)n_a; (void)n_b; (void)use_product;
    return false;
}

bool nimcp_gpu_fuzzy_relation_project(
    nimcp_gpu_context_t* ctx,
    const float* relation,
    float* projection,
    uint32_t rows,
    uint32_t cols,
    bool project_first)
{
    (void)ctx; (void)relation; (void)projection;
    (void)rows; (void)cols; (void)project_first;
    return false;
}

const char* nimcp_gpu_fuzzy_relation_get_last_error(void) {
    return "GPU support not compiled";
}

} // extern "C"

#endif // NIMCP_ENABLE_CUDA
