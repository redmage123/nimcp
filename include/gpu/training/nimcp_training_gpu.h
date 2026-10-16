//=============================================================================
// nimcp_training_gpu.h - GPU Training Kernels for Neural Networks
//=============================================================================
/**
 * @file nimcp_training_gpu.h
 * @brief GPU-accelerated training operations using CUDA
 *
 * WHAT: GPU kernels for neural network training (loss, gradients, optimizers)
 * WHY:  Enables massive parallel acceleration for training loops
 * HOW:  Custom CUDA kernels for backpropagation, gradient computation, optimization
 *
 * ARCHITECTURE:
 * - Loss functions: MSE, CrossEntropy, Focal, Huber
 * - Gradient operations: accumulation, clipping, scaling
 * - Optimizers: SGD, Adam, AdamW, RMSprop, AdaGrad
 * - Backpropagation: linear, conv, activation gradients
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_TRAINING_GPU_H
#define NIMCP_TRAINING_GPU_H

// Include headers with CUDA dependencies BEFORE extern "C" block
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/sparse/nimcp_sparse_gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// Loss Function Types
//=============================================================================

typedef enum {
    NIMCP_LOSS_MSE = 0,           /**< Mean Squared Error */
    NIMCP_LOSS_MAE = 1,           /**< Mean Absolute Error */
    NIMCP_LOSS_CROSS_ENTROPY = 2, /**< Cross Entropy (classification) */
    NIMCP_LOSS_BCE = 3,           /**< Binary Cross Entropy */
    NIMCP_LOSS_FOCAL = 4,         /**< Focal Loss (imbalanced data) */
    NIMCP_LOSS_HUBER = 5,         /**< Huber Loss (robust regression) */
    NIMCP_LOSS_COSINE = 6,        /**< Cosine Similarity Loss */
    NIMCP_LOSS_HINGE = 7,         /**< Hinge Loss (SVM-style) */
    NIMCP_LOSS_KL_DIV = 8         /**< KL Divergence */
} nimcp_loss_type_t;

//=============================================================================
// Optimizer Types
//=============================================================================

typedef enum {
    NIMCP_OPTIM_SGD = 0,          /**< Stochastic Gradient Descent */
    NIMCP_OPTIM_SGD_MOMENTUM = 1, /**< SGD with Momentum */
    NIMCP_OPTIM_ADAM = 2,         /**< Adam optimizer */
    NIMCP_OPTIM_ADAMW = 3,        /**< AdamW (decoupled weight decay) */
    NIMCP_OPTIM_RMSPROP = 4,      /**< RMSprop */
    NIMCP_OPTIM_ADAGRAD = 5,      /**< AdaGrad */
    NIMCP_OPTIM_ADADELTA = 6,     /**< AdaDelta */
    NIMCP_OPTIM_NADAM = 7         /**< Nesterov Adam */
} nimcp_optim_type_t;

//=============================================================================
// Optimizer State Structure
//=============================================================================

/**
 * @brief Optimizer state for storing momentum and adaptive learning rates
 */
typedef struct nimcp_optim_state_s {
    nimcp_optim_type_t type;      /**< Optimizer type */
    nimcp_gpu_tensor_t* m;        /**< First moment (momentum/mean) */
    nimcp_gpu_tensor_t* v;        /**< Second moment (variance) */
    nimcp_gpu_tensor_t* v_hat;    /**< Bias-corrected variance (AdaDelta) */
    uint64_t t;                   /**< Timestep counter */
    float lr;                     /**< Learning rate */
    float beta1;                  /**< First moment decay (default: 0.9) */
    float beta2;                  /**< Second moment decay (default: 0.999) */
    float eps;                    /**< Epsilon for numerical stability */
    float weight_decay;           /**< Weight decay coefficient */
    float momentum;               /**< Momentum coefficient (SGD) */
    bool nesterov;                /**< Use Nesterov momentum */
} nimcp_optim_state_t;

//=============================================================================
// Loss Functions
//=============================================================================

/**
 * @brief Compute MSE loss and gradient
 *
 * Loss = mean((pred - target)^2)
 * Gradient = 2 * (pred - target) / n
 *
 * @param ctx GPU context
 * @param pred Predictions tensor
 * @param target Target tensor
 * @param loss Output scalar loss value
 * @param grad Output gradient tensor (same shape as pred)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_loss_mse(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target,
    float* loss,
    nimcp_gpu_tensor_t* grad
);

/**
 * @brief Compute MAE loss and gradient
 *
 * Loss = mean(|pred - target|)
 * Gradient = sign(pred - target) / n
 */
NIMCP_EXPORT bool nimcp_gpu_loss_mae(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target,
    float* loss,
    nimcp_gpu_tensor_t* grad
);

/**
 * @brief Compute Cross Entropy loss and gradient
 *
 * Loss = -sum(target * log(softmax(pred)))
 * Includes softmax computation for numerical stability
 *
 * @param ctx GPU context
 * @param logits Raw logits (before softmax)
 * @param target Target class indices (int64) or one-hot
 * @param loss Output loss value
 * @param grad Output gradient tensor
 * @param reduction 0=none, 1=mean, 2=sum
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_loss_cross_entropy(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* logits,
    const nimcp_gpu_tensor_t* target,
    float* loss,
    nimcp_gpu_tensor_t* grad,
    int reduction
);

/**
 * @brief Compute Binary Cross Entropy loss and gradient
 *
 * Loss = -[target * log(pred) + (1-target) * log(1-pred)]
 */
NIMCP_EXPORT bool nimcp_gpu_loss_bce(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target,
    float* loss,
    nimcp_gpu_tensor_t* grad
);

/**
 * @brief Compute Focal loss and gradient
 *
 * Focal loss for handling class imbalance:
 * FL = -alpha * (1 - p_t)^gamma * log(p_t)
 *
 * @param alpha Weighting factor (default: 0.25)
 * @param gamma Focusing parameter (default: 2.0)
 */
NIMCP_EXPORT bool nimcp_gpu_loss_focal(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target,
    float* loss,
    nimcp_gpu_tensor_t* grad,
    float alpha,
    float gamma
);

/**
 * @brief Compute Huber loss and gradient
 *
 * Huber loss: L2 for small errors, L1 for large
 * Loss = 0.5 * x^2 if |x| <= delta, else delta * (|x| - 0.5 * delta)
 */
NIMCP_EXPORT bool nimcp_gpu_loss_huber(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target,
    float* loss,
    nimcp_gpu_tensor_t* grad,
    float delta
);

//=============================================================================
// Gradient Operations
//=============================================================================

/**
 * @brief Accumulate gradients: accum += grad
 */
NIMCP_EXPORT bool nimcp_gpu_gradient_accumulate(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* grad,
    nimcp_gpu_tensor_t* accum
);

/**
 * @brief Scale gradients by factor
 *
 * Used for gradient averaging in distributed training or mixed precision
 */
NIMCP_EXPORT bool nimcp_gpu_gradient_scale(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* grad,
    float scale
);

/**
 * @brief Clip gradients by global norm
 *
 * If ||grad|| > max_norm, scale grad by max_norm / ||grad||
 *
 * @param ctx GPU context
 * @param grads Array of gradient tensors
 * @param n_grads Number of gradient tensors
 * @param max_norm Maximum allowed gradient norm
 * @param total_norm Output: actual gradient norm before clipping
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_gradient_clip_norm(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t** grads,
    size_t n_grads,
    float max_norm,
    float* total_norm
);

/**
 * @brief Clip gradients by value
 *
 * Clip each element to [-clip_value, clip_value]
 */
NIMCP_EXPORT bool nimcp_gpu_gradient_clip_value(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* grad,
    float clip_value
);

/**
 * @brief Zero all gradients
 */
NIMCP_EXPORT bool nimcp_gpu_gradient_zero(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* grad
);

//=============================================================================
// Optimizer Functions
//=============================================================================

/**
 * @brief Create optimizer state for a parameter
 */
NIMCP_EXPORT nimcp_optim_state_t* nimcp_optim_state_create(
    nimcp_gpu_context_t* ctx,
    nimcp_optim_type_t type,
    const nimcp_gpu_tensor_t* param,
    float lr
);

/**
 * @brief Destroy optimizer state
 */
NIMCP_EXPORT void nimcp_optim_state_destroy(nimcp_optim_state_t* state);

/**
 * @brief SGD optimizer step
 *
 * param = param - lr * grad
 * With momentum: v = momentum * v + grad; param = param - lr * v
 */
NIMCP_EXPORT bool nimcp_gpu_optim_sgd(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* param,
    const nimcp_gpu_tensor_t* grad,
    nimcp_optim_state_t* state
);

/**
 * @brief Adam optimizer step
 *
 * m = beta1 * m + (1 - beta1) * grad
 * v = beta2 * v + (1 - beta2) * grad^2
 * m_hat = m / (1 - beta1^t)
 * v_hat = v / (1 - beta2^t)
 * param = param - lr * m_hat / (sqrt(v_hat) + eps)
 */
NIMCP_EXPORT bool nimcp_gpu_optim_adam(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* param,
    const nimcp_gpu_tensor_t* grad,
    nimcp_optim_state_t* state
);

/**
 * @brief AdamW optimizer step
 *
 * Like Adam but with decoupled weight decay:
 * param = param - lr * weight_decay * param  (before Adam step)
 */
NIMCP_EXPORT bool nimcp_gpu_optim_adamw(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* param,
    const nimcp_gpu_tensor_t* grad,
    nimcp_optim_state_t* state
);

/**
 * @brief RMSprop optimizer step
 *
 * v = alpha * v + (1 - alpha) * grad^2
 * param = param - lr * grad / (sqrt(v) + eps)
 */
NIMCP_EXPORT bool nimcp_gpu_optim_rmsprop(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* param,
    const nimcp_gpu_tensor_t* grad,
    nimcp_optim_state_t* state
);

/**
 * @brief AdaGrad optimizer step
 *
 * v = v + grad^2
 * param = param - lr * grad / (sqrt(v) + eps)
 */
NIMCP_EXPORT bool nimcp_gpu_optim_adagrad(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* param,
    const nimcp_gpu_tensor_t* grad,
    nimcp_optim_state_t* state
);

//=============================================================================
// Backpropagation Kernels
//=============================================================================

/**
 * @brief Backward pass for linear layer
 *
 * Given: y = x @ W^T + b, and dy (gradient of loss w.r.t. y)
 * Compute: dx, dW, db
 *
 * @param ctx GPU context
 * @param x Input (batch x in_features)
 * @param weight Weight matrix (out_features x in_features)
 * @param grad_output Gradient from upstream (batch x out_features)
 * @param grad_input Output: gradient w.r.t. input
 * @param grad_weight Output: gradient w.r.t. weight
 * @param grad_bias Output: gradient w.r.t. bias (can be NULL)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_backward_linear(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* weight,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input,
    nimcp_gpu_tensor_t* grad_weight,
    nimcp_gpu_tensor_t* grad_bias
);

/**
 * @brief Backward pass for ReLU activation
 *
 * dx = dy * (x > 0)
 */
NIMCP_EXPORT bool nimcp_gpu_backward_relu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input
);

/**
 * @brief Backward pass for sigmoid activation
 *
 * dx = dy * sigmoid(x) * (1 - sigmoid(x))
 */
NIMCP_EXPORT bool nimcp_gpu_backward_sigmoid(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* output,  // sigmoid output, not input
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input
);

/**
 * @brief Backward pass for tanh activation
 *
 * dx = dy * (1 - tanh(x)^2)
 */
NIMCP_EXPORT bool nimcp_gpu_backward_tanh(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input
);

/**
 * @brief Backward pass for GELU activation
 */
NIMCP_EXPORT bool nimcp_gpu_backward_gelu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input
);

/**
 * @brief Backward pass for softmax
 *
 * Given softmax output s and gradient dy:
 * dx_i = s_i * (dy_i - sum_j(dy_j * s_j))
 */
NIMCP_EXPORT bool nimcp_gpu_backward_softmax(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input
);

/**
 * @brief Backward pass for batch normalization
 *
 * @param ctx GPU context
 * @param x Input tensor
 * @param gamma Scale parameter
 * @param mean Running mean
 * @param var Running variance
 * @param grad_output Upstream gradient
 * @param grad_input Output: gradient w.r.t. input
 * @param grad_gamma Output: gradient w.r.t. gamma
 * @param grad_beta Output: gradient w.r.t. beta
 * @param eps Epsilon for numerical stability
 */
NIMCP_EXPORT bool nimcp_gpu_backward_batchnorm(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* mean,
    const nimcp_gpu_tensor_t* var,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input,
    nimcp_gpu_tensor_t* grad_gamma,
    nimcp_gpu_tensor_t* grad_beta,
    float eps
);

/**
 * @brief Backward pass for layer normalization
 */
NIMCP_EXPORT bool nimcp_gpu_backward_layernorm(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* mean,
    const nimcp_gpu_tensor_t* var,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input,
    nimcp_gpu_tensor_t* grad_gamma,
    nimcp_gpu_tensor_t* grad_beta,
    float eps
);

/**
 * @brief Backward pass for dropout
 *
 * dx = dy * mask / (1 - p)
 */
NIMCP_EXPORT bool nimcp_gpu_backward_dropout(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* mask,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input,
    float p
);

//=============================================================================
// Learning Rate Schedulers
//=============================================================================

/**
 * @brief Step learning rate scheduler
 *
 * lr = initial_lr * gamma^(step / step_size)
 */
NIMCP_EXPORT float nimcp_lr_step(
    float initial_lr,
    uint64_t step,
    uint64_t step_size,
    float gamma
);

/**
 * @brief Cosine annealing learning rate
 *
 * lr = min_lr + 0.5 * (max_lr - min_lr) * (1 + cos(pi * step / T))
 */
NIMCP_EXPORT float nimcp_lr_cosine(
    float max_lr,
    float min_lr,
    uint64_t step,
    uint64_t total_steps
);

/**
 * @brief Linear warmup then decay
 */
NIMCP_EXPORT float nimcp_lr_warmup_linear(
    float max_lr,
    uint64_t step,
    uint64_t warmup_steps,
    uint64_t total_steps
);

/**
 * @brief Exponential decay learning rate
 *
 * lr = initial_lr * exp(-decay_rate * step)
 */
NIMCP_EXPORT float nimcp_lr_exponential(
    float initial_lr,
    uint64_t step,
    float decay_rate
);

//=============================================================================
// GPU Sparse Backpropagation
//=============================================================================

/**
 * @brief Full GPU sparse backward pass over weight cache
 *
 * WHAT: GPU-accelerated backpropagation for sparse CSR weight matrices
 * WHY:  Replaces CPU backprop_sparse_full for ~3-4x training speedup
 * HOW:  Custom CUDA kernels for CSR weight update + delta propagation
 *
 * @param gpu_ctx GPU context
 * @param sparse_ctx cuSPARSE context
 * @param sparse_weights Per-transition sparse CSR weight tensors
 * @param biases Per-transition bias tensors
 * @param activations Per-layer activation tensors (from forward pass)
 * @param layer_act_types Per-layer activation type (0=relu,1=leaky,2=tanh,3=sigmoid)
 * @param num_layers Number of layers
 * @param layer_sizes Array of layer sizes
 * @param target_host Target vector (host memory)
 * @param output_host Network output (host memory)
 * @param output_size Output vector size
 * @param learning_rate Learning rate
 * @param min_weight Minimum weight clamp
 * @param max_weight Maximum weight clamp
 * @param out_grad_norm Output gradient norm
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_sparse_backward_pass(
    nimcp_gpu_context_t* gpu_ctx,
    nimcp_sparse_ctx_t* sparse_ctx,
    nimcp_sparse_tensor_t** sparse_weights,
    nimcp_gpu_tensor_t** biases,
    nimcp_gpu_tensor_t** activations,
    int* layer_act_types,
    uint32_t num_layers,
    const uint32_t* layer_sizes,
    const float* target_host,
    const float* output_host,
    uint32_t output_size,
    float learning_rate,
    float min_weight, float max_weight,
    float* out_grad_norm
);

/**
 * @brief GPU sparse backward pass — accumulate gradients (no weight update)
 *
 * Same as nimcp_gpu_sparse_backward_pass but accumulates gradients into
 * separate GPU buffers instead of updating weights in-place. Call
 * nimcp_gpu_gradient_flush() after N samples to apply averaged gradients.
 *
 * @param d_weight_grad_accum Per-transition GPU gradient buffers [num_layers-1], same nnz as sparse_weights
 * @param d_bias_grad_accum Per-transition GPU bias gradient buffers [num_layers-1]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_sparse_backward_accumulate(
    nimcp_gpu_context_t* gpu_ctx,
    nimcp_sparse_ctx_t* sparse_ctx,
    nimcp_sparse_tensor_t** sparse_weights,
    nimcp_gpu_tensor_t** biases,
    nimcp_gpu_tensor_t** activations,
    float** d_weight_grad_accum,
    float** d_bias_grad_accum,
    int* layer_act_types,
    uint32_t num_layers,
    const uint32_t* layer_sizes,
    const float* target_host,
    const float* output_host,
    uint32_t output_size,
    float learning_rate
);

/**
 * @brief Apply accumulated gradients / batch_size to weights and biases
 *
 * Divides accumulated gradients by batch_size, applies to weights with clamping,
 * resets gradient buffers to zero.
 *
 * @param batch_size Number of samples accumulated
 * @param out_grad_norm Output: L2 norm of averaged gradients
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_gradient_flush(
    nimcp_gpu_context_t* gpu_ctx,
    nimcp_sparse_tensor_t** sparse_weights,
    nimcp_gpu_tensor_t** biases,
    float** d_weight_grad_accum,
    float** d_bias_grad_accum,
    uint32_t num_layers,
    const uint32_t* layer_sizes,
    uint32_t batch_size,
    float min_weight, float max_weight,
    float* out_grad_norm
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_TRAINING_GPU_H
