/**
 * @file nimcp_fep_learning_gpu.h
 * @brief GPU-accelerated FEP Learning API
 * @version 1.1.0
 * @date 2026-01-01
 *
 * WHAT: GPU acceleration for FEP learning operations
 * WHY:  Matrix operations benefit greatly from GPU parallelism
 * HOW:  CUDA kernels for matvec, gradient, and batch updates
 *
 * REFACTORED (2026-01-01):
 * ========================
 * - Internal storage now uses nimcp_gpu_tensor_t
 * - Uses nimcp_gpu_gemv for matrix-vector operations
 * - Added tensor-based API for zero-copy GPU integration
 * - Context accessors for direct tensor manipulation
 */

#ifndef NIMCP_FEP_LEARNING_GPU_H
#define NIMCP_FEP_LEARNING_GPU_H

// Include GPU headers before extern "C" (they may contain C++ code)
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque GPU context for FEP learning
 *
 * Internal storage now uses nimcp_gpu_tensor_t for all matrices and vectors.
 */
typedef struct fep_learning_gpu_context_s fep_learning_gpu_context_t;

//=============================================================================
// Context Lifecycle
//=============================================================================

/**
 * @brief Create GPU context for FEP learning
 *
 * Allocates GPU tensors for all matrices and vectors needed for learning:
 * - Transition matrix A [state_dim x state_dim]
 * - Likelihood matrix B [obs_dim x state_dim] (if obs_dim > 0)
 * - Gradient and velocity buffers for momentum
 * - Intermediate state and prediction vectors
 *
 * @param gpu_ctx Main GPU context
 * @param state_dim State dimension
 * @param obs_dim Observation dimension (0 if not using likelihood)
 * @param max_batch_size Maximum batch size for learning
 * @return FEP learning GPU context, NULL on failure
 */
fep_learning_gpu_context_t* fep_learning_gpu_create(
    nimcp_gpu_context_t* gpu_ctx,
    uint32_t state_dim,
    uint32_t obs_dim,
    uint32_t max_batch_size);

/**
 * @brief Destroy FEP learning GPU context
 *
 * Frees all GPU tensors and host memory.
 *
 * @param ctx Context to destroy (NULL safe)
 */
void fep_learning_gpu_destroy(fep_learning_gpu_context_t* ctx);

//=============================================================================
// Transition Matrix Operations
//=============================================================================

/**
 * @brief Upload transition matrix to GPU
 *
 * Copies the transition matrix from host to the internal GPU tensor.
 *
 * @param ctx FEP learning GPU context
 * @param A Transition matrix (dim x dim, row-major)
 * @param dim State dimension
 * @return 0 on success, negative on error
 */
int fep_learning_gpu_upload_transition(
    fep_learning_gpu_context_t* ctx,
    const float* A,
    uint32_t dim);

/**
 * @brief Download transition matrix from GPU
 *
 * Copies the transition matrix from internal GPU tensor to host.
 *
 * @param ctx FEP learning GPU context
 * @param A Output buffer for matrix
 * @param dim State dimension
 * @return 0 on success, negative on error
 */
int fep_learning_gpu_download_transition(
    fep_learning_gpu_context_t* ctx,
    float* A,
    uint32_t dim);

/**
 * @brief GPU-accelerated transition learning step
 *
 * Performs one step of gradient descent on the transition matrix A:
 *   prediction = A * state_t        (uses nimcp_gpu_gemv)
 *   error = state_t1 - prediction
 *   grad = -error x state_t^T + lambda * A
 *   A = A - lr * grad  (with optional momentum)
 *
 * @param ctx FEP learning GPU context
 * @param state_t Current state (host memory)
 * @param state_t1 Next state (host memory)
 * @param lr Learning rate
 * @param lambda L2 regularization coefficient
 * @param momentum Momentum coefficient (0 to disable)
 * @param loss_out Output loss value (can be NULL)
 * @return 0 on success, negative on error
 */
int fep_learn_transition_gpu(
    fep_learning_gpu_context_t* ctx,
    const float* state_t,
    const float* state_t1,
    float lr,
    float lambda,
    float momentum,
    float* loss_out);

/**
 * @brief GPU-accelerated batch transition learning
 *
 * Accumulates gradients over multiple transitions, then applies averaged update.
 *
 * @param ctx FEP learning GPU context
 * @param states Flattened state sequence [(n_transitions+1) * dim]
 * @param n_transitions Number of transitions
 * @param lr Learning rate
 * @param lambda L2 regularization
 * @param momentum Momentum coefficient
 * @param avg_loss_out Average loss output (can be NULL)
 * @return 0 on success, negative on error
 */
int fep_learn_transition_batch_gpu(
    fep_learning_gpu_context_t* ctx,
    const float* states,
    uint32_t n_transitions,
    float lr,
    float lambda,
    float momentum,
    float* avg_loss_out);

//=============================================================================
// Likelihood Matrix Operations
//=============================================================================

/**
 * @brief Upload likelihood matrix to GPU
 *
 * @param ctx FEP learning GPU context
 * @param B Likelihood matrix (obs_dim x state_dim)
 * @param obs_dim Observation dimension
 * @param state_dim State dimension
 * @return 0 on success, negative on error
 */
int fep_learning_gpu_upload_likelihood(
    fep_learning_gpu_context_t* ctx,
    const float* B,
    uint32_t obs_dim,
    uint32_t state_dim);

/**
 * @brief Download likelihood matrix from GPU
 *
 * @param ctx FEP learning GPU context
 * @param B Output buffer for matrix
 * @param obs_dim Observation dimension
 * @param state_dim State dimension
 * @return 0 on success, negative on error
 */
int fep_learning_gpu_download_likelihood(
    fep_learning_gpu_context_t* ctx,
    float* B,
    uint32_t obs_dim,
    uint32_t state_dim);

/**
 * @brief GPU-accelerated likelihood learning step
 *
 * @param ctx FEP learning GPU context
 * @param observation Current observation (host memory)
 * @param state Current state (host memory)
 * @param lr Learning rate
 * @param lambda L2 regularization coefficient
 * @param momentum Momentum coefficient (0 to disable)
 * @param loss_out Output loss value (can be NULL)
 * @return 0 on success, negative on error
 */
int fep_learn_likelihood_gpu(
    fep_learning_gpu_context_t* ctx,
    const float* observation,
    const float* state,
    float lr,
    float lambda,
    float momentum,
    float* loss_out);

//=============================================================================
// Tensor-Based API (New in v1.1)
//=============================================================================

/**
 * @brief Get the transition matrix tensor from context
 *
 * Returns a pointer to the internal GPU tensor storing the transition matrix.
 * This allows direct manipulation when the matrix is already on GPU.
 *
 * @param ctx FEP learning GPU context
 * @return Transition matrix tensor [state_dim x state_dim], or NULL if not initialized
 *
 * @note The returned tensor is owned by the context; do not destroy it.
 */
nimcp_gpu_tensor_t* fep_learning_gpu_get_transition_tensor(
    fep_learning_gpu_context_t* ctx);

/**
 * @brief Get the likelihood matrix tensor from context
 *
 * @param ctx FEP learning GPU context
 * @return Likelihood matrix tensor [obs_dim x state_dim], or NULL if not available
 *
 * @note The returned tensor is owned by the context; do not destroy it.
 */
nimcp_gpu_tensor_t* fep_learning_gpu_get_likelihood_tensor(
    fep_learning_gpu_context_t* ctx);

/**
 * @brief Perform transition learning with tensors already on GPU
 *
 * Use this when state vectors are already on GPU to avoid host-device transfers.
 * This is the most efficient path when integrating with other GPU operations.
 *
 * @param ctx FEP learning GPU context
 * @param state_t Current state tensor (already on GPU, FP32, [state_dim])
 * @param state_t1 Next state tensor (already on GPU, FP32, [state_dim])
 * @param lr Learning rate
 * @param lambda L2 regularization
 * @param momentum Momentum coefficient
 * @return true on success, false on failure
 *
 * EXAMPLE:
 * @code
 * // State vectors already on GPU from attention processing
 * nimcp_gpu_tensor_t* state_t = attention_get_output_tensor(attn_ctx);
 * nimcp_gpu_tensor_t* state_t1 = attention_get_next_output_tensor(attn_ctx);
 *
 * bool ok = fep_learn_transition_tensor(fep_ctx, state_t, state_t1, 0.01f, 0.001f, 0.9f);
 * @endcode
 */
bool fep_learn_transition_tensor(
    fep_learning_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* state_t,
    const nimcp_gpu_tensor_t* state_t1,
    float lr,
    float lambda,
    float momentum);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Check if GPU should be used for FEP learning
 *
 * Returns true if the matrix size is large enough to benefit from GPU.
 * For small matrices (< 64 elements), CPU may be faster due to
 * kernel launch overhead.
 *
 * @param dim State dimension
 * @return true if GPU acceleration is beneficial
 */
bool fep_learning_should_use_gpu(uint32_t dim);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FEP_LEARNING_GPU_H */
