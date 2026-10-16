//=============================================================================
// nimcp_backprop_kernel.h - Shared Backprop Kernel with Parallel + SIMD
//=============================================================================
/**
 * @file nimcp_backprop_kernel.h
 * @brief Extracted sparse backpropagation kernel used by all learning modes
 *
 * WHAT: Full backprop through all layers with sparse active-set tracking
 * WHY:  Deduplicate 3x identical backprop blocks (GPU/SUPERVISED/HYBRID)
 * PERF: Within-layer parallel dispatch + SIMD activation derivatives
 */

#ifndef NIMCP_BACKPROP_KERNEL_H
#define NIMCP_BACKPROP_KERNEL_H

#include <stddef.h>
#include <stdint.h>

// Forward declare — avoid pulling in full neuralnet header
typedef struct neural_network_struct* neural_network_t;

#ifdef __cplusplus
extern "C" {
#endif

/** Output layer learning rate multiplier for classification tasks.
 *  Defined here (single source of truth) so both nimcp_adaptive.c and
 *  nimcp_backprop_kernel.c share the same constant. */
#define OUTPUT_LR_BOOST 10.0f

/** Maximum number of layers for per-layer gradient norm tracking.
 *  Covers networks up to 8 weight layers (9 neuron layers). */
#define BP_MAX_GRAD_LAYERS 8

/**
 * @brief Per-layer gradient norm results from a backprop pass
 *
 * WHY: Enables layer-wise learning rate diagnostics and per-layer gradient
 *      monitoring without changing the scalar out_grad_norm API.
 *
 * USAGE: Allocate on the stack and pass to backprop_sparse_full_ex().
 *        After return, norms[0..num_layers-1] contain the L2 norm of all
 *        weight deltas applied at each weight layer (layer 0 = weights
 *        connecting input->hidden, layer 1 = hidden->output, etc.).
 *        Pass NULL to skip per-layer tracking (zero overhead).
 */
typedef struct {
    float  norms[BP_MAX_GRAD_LAYERS];  /**< L2 norm per weight layer */
    uint32_t num_layers;               /**< Number of valid entries in norms[] */
} backprop_layer_grads_t;

/**
 * @brief Run sparse backprop through all layers
 *
 * @param net           Base neural network handle
 * @param num_layers    Number of layers (must be >= 2)
 * @param layer_sizes   Array of per-layer neuron counts [num_layers]
 * @param learning_rate Base learning rate (output layer gets 10x boost)
 * @param min_weight    Weight clamp lower bound
 * @param max_weight    Weight clamp upper bound
 * @param target        Target vector [target_size]
 * @param output        Network output vector [target_size]
 * @param target_size   Length of target/output vectors
 * @param max_grad_norm  Maximum gradient norm for clipping (0.0 = no clipping)
 * @param out_grad_norm Output: L2 gradient norm (sqrt of sum of squared weight deltas)
 * @return 0 on success, -1 on allocation failure
 */
int backprop_sparse_full(
    neural_network_t net,
    uint32_t num_layers,
    const uint32_t* layer_sizes,
    float learning_rate,
    float min_weight, float max_weight,
    const float* target, const float* output,
    uint32_t target_size,
    float max_grad_norm,
    float* out_grad_norm);

/**
 * @brief Run sparse backprop with optional per-layer gradient norm tracking
 *
 * Extended version of backprop_sparse_full() that also computes per-layer
 * gradient L2 norms. The scalar out_grad_norm still receives the global norm.
 *
 * @param net           Base neural network handle
 * @param num_layers    Number of layers (must be >= 2)
 * @param layer_sizes   Array of per-layer neuron counts [num_layers]
 * @param learning_rate Base learning rate (output layer gets 10x boost)
 * @param min_weight    Weight clamp lower bound
 * @param max_weight    Weight clamp upper bound
 * @param target        Target vector [target_size]
 * @param output        Network output vector [target_size]
 * @param target_size   Length of target/output vectors
 * @param max_grad_norm Maximum gradient norm for clipping (0.0 = no clipping)
 * @param out_grad_norm Output: global L2 gradient norm
 * @param out_layer_grads Output: per-layer gradient norms (NULL to skip tracking)
 * @return 0 on success, -1 on allocation failure
 */
int backprop_sparse_full_ex(
    neural_network_t net,
    uint32_t num_layers,
    const uint32_t* layer_sizes,
    float learning_rate,
    float min_weight, float max_weight,
    const float* target, const float* output,
    uint32_t target_size,
    float max_grad_norm,
    float* out_grad_norm,
    backprop_layer_grads_t* out_layer_grads);

/** @brief Regression-mode backprop: MSE gradients on ALL outputs.
 *  No threshold filtering, no negative sampling. For continuous targets. */
int backprop_sparse_full_regression(
    neural_network_t net,
    uint32_t num_layers,
    const uint32_t* layer_sizes,
    float learning_rate,
    float min_weight, float max_weight,
    const float* target, const float* output,
    uint32_t target_size,
    float max_grad_norm,
    float* out_grad_norm,
    backprop_layer_grads_t* out_layer_grads);

/**
 * @brief Run sparse backprop with gradient accumulation via LR scaling
 *
 * Mathematically equivalent to accumulating N gradients and averaging:
 * scales the learning rate by 1/accumulation_steps so each micro-batch
 * contributes 1/N of its gradient to the weight update.
 *
 * @param net           Base neural network handle
 * @param num_layers    Number of layers (must be >= 2)
 * @param layer_sizes   Array of per-layer neuron counts [num_layers]
 * @param learning_rate Base learning rate (will be divided by accumulation_steps)
 * @param min_weight    Weight clamp lower bound
 * @param max_weight    Weight clamp upper bound
 * @param target        Target vector [target_size]
 * @param output        Network output vector [target_size]
 * @param target_size   Length of target/output vectors
 * @param max_grad_norm Maximum gradient norm for clipping (0.0 = no clipping)
 * @param accumulation_steps Number of micro-batches to simulate (LR divided by this)
 * @param out_grad_norm Output: L2 gradient norm (sqrt of sum of squared weight deltas)
 * @return 0 on success, -1 on error
 */
int backprop_with_accumulation(
    neural_network_t net,
    uint32_t num_layers,
    const uint32_t* layer_sizes,
    float learning_rate,
    float min_weight, float max_weight,
    const float* target, const float* output,
    uint32_t target_size,
    float max_grad_norm,
    uint32_t accumulation_steps,
    float* out_grad_norm);

/**
 * @brief Run sparse backprop with weight decay and diversity gradient injection
 *
 * Extended version that supports:
 * - Decoupled weight decay (AdamW-style): w *= (1 - lr * wd) before gradient step
 * - Diversity gradient: added to output layer deltas to push outputs apart
 *
 * @param net           Base neural network handle
 * @param num_layers    Number of layers (must be >= 2)
 * @param layer_sizes   Array of per-layer neuron counts [num_layers]
 * @param learning_rate Base learning rate (output layer gets 10x boost)
 * @param min_weight    Weight clamp lower bound
 * @param max_weight    Weight clamp upper bound
 * @param target        Target vector [target_size]
 * @param output        Network output vector [target_size]
 * @param target_size   Length of target/output vectors
 * @param max_grad_norm Maximum gradient norm for clipping (0.0 = no clipping)
 * @param weight_decay  Decoupled weight decay coefficient (0.0 = disabled)
 * @param diversity_grad Diversity gradient to add to output deltas (NULL = disabled)
 * @param out_grad_norm Output: global L2 gradient norm
 * @param out_layer_grads Output: per-layer gradient norms (NULL to skip tracking)
 * @return 0 on success, -1 on allocation failure
 */
int backprop_sparse_full_ex2(
    neural_network_t net,
    uint32_t num_layers,
    const uint32_t* layer_sizes,
    float learning_rate,
    float min_weight, float max_weight,
    const float* target, const float* output,
    uint32_t target_size,
    float max_grad_norm,
    float weight_decay,
    const float* diversity_grad,
    float* out_grad_norm,
    backprop_layer_grads_t* out_layer_grads);

/** @brief Regression-mode backprop with weight decay. MSE on ALL outputs. */
int backprop_sparse_full_regression_wd(
    neural_network_t net,
    uint32_t num_layers,
    const uint32_t* layer_sizes,
    float learning_rate,
    float min_weight, float max_weight,
    const float* target, const float* output,
    uint32_t target_size,
    float max_grad_norm,
    float weight_decay,
    const float* diversity_grad,
    float* out_grad_norm,
    backprop_layer_grads_t* out_layer_grads);

/**
 * @brief Destroy the static backprop thread pool
 *
 * Call during shutdown to release worker threads.
 * Safe to call multiple times or if pool was never created.
 */
void backprop_kernel_cleanup(void);

// Buffer allocation helpers (shared with nimcp_adaptive.c)
void* bp_alloc_hot_buffer(size_t size);
void  bp_free_hot_buffer(void* buf);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BACKPROP_KERNEL_H
