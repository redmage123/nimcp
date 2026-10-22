/**
 * @file nimcp_gpu_stubs_sparse.c
 * @brief CPU fallback stubs for GPU sparse operations
 *
 * WHAT: Provides stub implementations for sparse tensor GPU operations
 * WHY:  Allows code to compile and link without CUDA toolkit
 * HOW:  Returns error codes or NULL, CPU operations fall back automatically
 */

#include "utils/logging/nimcp_logging.h"
#include <stdbool.h>
#include <stddef.h>

static bool s_warned_sparse_gpu = false;

static void warn_once_sparse(const char* func) {
    if (!s_warned_sparse_gpu) {
        NIMCP_LOGGING_WARN("Sparse GPU functions unavailable (CUDA not found). "
                          "Using CPU fallback. Called from: %s", func);
        s_warned_sparse_gpu = true;
    }
}

//=============================================================================
// Sparse Context Operations
//=============================================================================

void* nimcp_sparse_ctx_create(void* gpu_ctx) {
    warn_once_sparse("nimcp_sparse_ctx_create");
    (void)gpu_ctx;
    return NULL;  /* Sparse context creation not available */
}

void nimcp_sparse_ctx_destroy(void* sparse_ctx) {
    (void)sparse_ctx;
    /* No-op: NULL context from stub create */
}

//=============================================================================
// Sparse Tensor Operations
//=============================================================================

void* nimcp_sparse_from_coo(
    void* sparse_ctx,
    const int* row_indices,
    const int* col_indices,
    const float* values,
    size_t nnz,
    size_t rows,
    size_t cols)
{
    warn_once_sparse("nimcp_sparse_from_coo");
    (void)sparse_ctx; (void)row_indices; (void)col_indices;
    (void)values; (void)nnz; (void)rows; (void)cols;
    return NULL;
}

void nimcp_sparse_tensor_destroy(void* sparse_tensor) {
    (void)sparse_tensor;
    /* No-op */
}

size_t nimcp_sparse_nnz(const void* sparse_tensor) {
    (void)sparse_tensor;
    return 0;
}

bool nimcp_sparse_to_host_csr(
    const void* sparse_tensor,
    int** row_ptr_out,
    int** col_ind_out,
    float** values_out,
    size_t* nnz_out,
    size_t* rows_out,
    size_t* cols_out)
{
    warn_once_sparse("nimcp_sparse_to_host_csr");
    (void)sparse_tensor; (void)row_ptr_out; (void)col_ind_out;
    (void)values_out; (void)nnz_out; (void)rows_out; (void)cols_out;
    return false;
}

//=============================================================================
// Sparse Linear Operations
//=============================================================================

bool nimcp_sparse_mv(
    void* sparse_ctx,
    const void* sparse_mat,
    const float* x,
    float* y,
    bool transpose)
{
    warn_once_sparse("nimcp_sparse_mv");
    (void)sparse_ctx; (void)sparse_mat; (void)x; (void)y; (void)transpose;
    return false;
}

bool nimcp_sparse_linear_forward(
    void* sparse_ctx,
    const void* sparse_weight,
    const float* input,
    const float* bias,
    float* output,
    size_t batch_size)
{
    warn_once_sparse("nimcp_sparse_linear_forward");
    (void)sparse_ctx; (void)sparse_weight; (void)input;
    (void)bias; (void)output; (void)batch_size;
    return false;
}

//=============================================================================
// Sparse Backward Pass
//=============================================================================

bool nimcp_gpu_sparse_backward_pass(
    void* gpu_ctx,
    const void* sparse_weight,
    const float* grad_output,
    float* grad_input,
    size_t batch_size)
{
    warn_once_sparse("nimcp_gpu_sparse_backward_pass");
    (void)gpu_ctx; (void)sparse_weight; (void)grad_output;
    (void)grad_input; (void)batch_size;
    return false;
}

bool nimcp_gpu_sparse_backward_accumulate(
    void* gpu_ctx,
    const void* sparse_weight,
    const float* input,
    const float* grad_output,
    void* grad_weight,
    float* grad_bias,
    size_t batch_size)
{
    warn_once_sparse("nimcp_gpu_sparse_backward_accumulate");
    (void)gpu_ctx; (void)sparse_weight; (void)input; (void)grad_output;
    (void)grad_weight; (void)grad_bias; (void)batch_size;
    return false;
}

//=============================================================================
// GPU Gradient Operations
//=============================================================================
