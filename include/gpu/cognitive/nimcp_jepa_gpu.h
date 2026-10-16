/**
 * @file nimcp_jepa_gpu.h
 * @brief GPU-accelerated JEPA Predictor operations using CUDA
 *
 * WHAT: CUDA kernels for JEPA predictor forward/inverse models and masking
 * WHY:  GPU acceleration for latent space prediction in JEPA architecture
 * HOW:  Custom kernels for MLP forward pass, inverse model, and mask operations
 *
 * HOT PATHS ACCELERATED:
 * - Forward model: Latent space prediction (matrix-vector ops)
 * - Inverse model: Action inference from state transitions
 * - Masking operations: Contrastive learning mask application
 *
 * ARCHITECTURE:
 * - MLP forward: z_pred = W2 @ GELU(W1 @ z_ctx + b1) + b2
 * - Inverse model: action = InverseNet(z_t, z_t+1)
 * - Masking: Apply binary/soft masks to latent representations
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_JEPA_GPU_H
#define NIMCP_JEPA_GPU_H

#include "gpu/context/nimcp_gpu_context.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief Maximum latent dimension for GPU operations */
#define NIMCP_JEPA_GPU_MAX_DIM              2048

/** @brief Default block size for JEPA kernels */
#define NIMCP_JEPA_GPU_BLOCK_SIZE           256

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Activation function types for GPU kernels
 */
typedef enum {
    NIMCP_JEPA_ACT_NONE = 0,        /**< No activation (linear) */
    NIMCP_JEPA_ACT_RELU = 1,        /**< ReLU: max(0, x) */
    NIMCP_JEPA_ACT_GELU = 2,        /**< GELU: x * Phi(x) */
    NIMCP_JEPA_ACT_TANH = 3,        /**< Tanh */
    NIMCP_JEPA_ACT_SIGMOID = 4      /**< Sigmoid */
} nimcp_jepa_gpu_activation_t;

/**
 * @brief Mask types for contrastive learning
 */
typedef enum {
    NIMCP_JEPA_MASK_BINARY = 0,     /**< Binary mask (0 or 1) */
    NIMCP_JEPA_MASK_SOFT = 1,       /**< Soft mask (continuous [0, 1]) */
    NIMCP_JEPA_MASK_GAUSSIAN = 2    /**< Gaussian-weighted mask */
} nimcp_jepa_mask_type_t;

/* ============================================================================
 * GPU State Structures
 * ============================================================================ */

/**
 * @brief GPU-side MLP layer weights and buffers
 */
typedef struct {
    nimcp_gpu_tensor_t* weights;    /**< Weight matrix [out_dim x in_dim] */
    nimcp_gpu_tensor_t* bias;       /**< Bias vector [out_dim] */
    nimcp_gpu_tensor_t* grad_w;     /**< Weight gradients (training) */
    nimcp_gpu_tensor_t* grad_b;     /**< Bias gradients (training) */
    uint32_t in_dim;                /**< Input dimension */
    uint32_t out_dim;               /**< Output dimension */
    nimcp_jepa_gpu_activation_t activation; /**< Activation function */
} nimcp_jepa_gpu_layer_t;

/**
 * @brief GPU-side JEPA predictor state
 */
typedef struct {
    nimcp_jepa_gpu_layer_t* layers; /**< Array of MLP layers */
    uint32_t num_layers;            /**< Number of layers */

    /* Intermediate buffers */
    nimcp_gpu_tensor_t** activations;    /**< Per-layer activations */
    nimcp_gpu_tensor_t** pre_activations; /**< Pre-activation values */

    /* Configuration */
    uint32_t input_dim;             /**< Input latent dimension */
    uint32_t hidden_dim;            /**< Hidden layer dimension */
    uint32_t output_dim;            /**< Output latent dimension */

    /* GPU context */
    nimcp_gpu_context_t* ctx;       /**< GPU context reference */
} nimcp_jepa_gpu_predictor_t;

/**
 * @brief GPU-side inverse model state
 */
typedef struct {
    nimcp_jepa_gpu_layer_t* layers; /**< MLP layers for inverse model */
    uint32_t num_layers;            /**< Number of layers */
    uint32_t state_dim;             /**< State/latent dimension */
    uint32_t action_dim;            /**< Action embedding dimension */
    nimcp_gpu_context_t* ctx;       /**< GPU context reference */
} nimcp_jepa_gpu_inverse_t;

/* ============================================================================
 * Predictor Lifecycle API
 * ============================================================================ */

/**
 * @brief Create GPU JEPA predictor
 *
 * @param ctx GPU context
 * @param input_dim Input latent dimension
 * @param hidden_dim Hidden layer dimension
 * @param output_dim Output latent dimension
 * @param num_layers Number of MLP layers
 * @param activation Activation function for hidden layers
 * @return GPU predictor state or NULL on failure
 */
NIMCP_EXPORT nimcp_jepa_gpu_predictor_t* nimcp_jepa_gpu_predictor_create(
    nimcp_gpu_context_t* ctx,
    uint32_t input_dim,
    uint32_t hidden_dim,
    uint32_t output_dim,
    uint32_t num_layers,
    nimcp_jepa_gpu_activation_t activation
);

/**
 * @brief Destroy GPU JEPA predictor
 *
 * @param predictor GPU predictor to destroy
 */
NIMCP_EXPORT void nimcp_jepa_gpu_predictor_destroy(nimcp_jepa_gpu_predictor_t* predictor);

/**
 * @brief Upload CPU weights to GPU predictor
 *
 * @param predictor GPU predictor
 * @param layer_idx Layer index
 * @param weights Weight data [out_dim x in_dim]
 * @param bias Bias data [out_dim]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_jepa_gpu_predictor_upload_weights(
    nimcp_jepa_gpu_predictor_t* predictor,
    uint32_t layer_idx,
    const float* weights,
    const float* bias
);

/**
 * @brief Download GPU weights to CPU
 *
 * @param predictor GPU predictor
 * @param layer_idx Layer index
 * @param weights Output weight data [out_dim x in_dim]
 * @param bias Output bias data [out_dim]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_jepa_gpu_predictor_download_weights(
    const nimcp_jepa_gpu_predictor_t* predictor,
    uint32_t layer_idx,
    float* weights,
    float* bias
);

/* ============================================================================
 * Forward Model API (Latent Prediction)
 * ============================================================================ */

/**
 * @brief GPU forward prediction: z_pred = MLP(z_context)
 *
 * WHAT: Predict target latent from context latent
 * WHY:  Core JEPA operation - predict in embedding space
 * HOW:  Multi-layer MLP with configurable activation
 *
 * @param predictor GPU predictor state
 * @param context Input context latent [batch_size x input_dim]
 * @param prediction Output predicted latent [batch_size x output_dim]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_jepa_gpu_forward_predict(
    nimcp_jepa_gpu_predictor_t* predictor,
    const nimcp_gpu_tensor_t* context,
    nimcp_gpu_tensor_t* prediction
);

/**
 * @brief GPU forward with action conditioning
 *
 * WHAT: Predict next latent given current state and action
 * WHY:  World model prediction: z_{t+1} = f(z_t, a_t)
 * HOW:  Concatenate state and action, then forward through MLP
 *
 * @param predictor GPU predictor (must be configured for state+action input)
 * @param state Current state latent [batch_size x state_dim]
 * @param action Action embedding [batch_size x action_dim]
 * @param next_state Output predicted next state [batch_size x state_dim]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_jepa_gpu_forward_conditioned(
    nimcp_jepa_gpu_predictor_t* predictor,
    const nimcp_gpu_tensor_t* state,
    const nimcp_gpu_tensor_t* action,
    nimcp_gpu_tensor_t* next_state
);

/* ============================================================================
 * Inverse Model API (Action Inference)
 * ============================================================================ */

/**
 * @brief Create GPU inverse model
 *
 * @param ctx GPU context
 * @param state_dim State/latent dimension
 * @param action_dim Action embedding dimension
 * @param hidden_dim Hidden layer dimension
 * @param num_layers Number of MLP layers
 * @return GPU inverse model or NULL
 */
NIMCP_EXPORT nimcp_jepa_gpu_inverse_t* nimcp_jepa_gpu_inverse_create(
    nimcp_gpu_context_t* ctx,
    uint32_t state_dim,
    uint32_t action_dim,
    uint32_t hidden_dim,
    uint32_t num_layers
);

/**
 * @brief Destroy GPU inverse model
 */
NIMCP_EXPORT void nimcp_jepa_gpu_inverse_destroy(nimcp_jepa_gpu_inverse_t* inverse);

/**
 * @brief GPU inverse model: Infer action from state transition
 *
 * WHAT: Infer what action caused state transition
 * WHY:  Inverse dynamics for learning action representations
 * HOW:  action = InverseNet(z_t, z_{t+1})
 *
 * @param inverse GPU inverse model
 * @param state_t Current state [batch_size x state_dim]
 * @param state_next Next state [batch_size x state_dim]
 * @param action Output inferred action [batch_size x action_dim]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_jepa_gpu_inverse_infer(
    nimcp_jepa_gpu_inverse_t* inverse,
    const nimcp_gpu_tensor_t* state_t,
    const nimcp_gpu_tensor_t* state_next,
    nimcp_gpu_tensor_t* action
);

/* ============================================================================
 * Masking Operations API
 * ============================================================================ */

/**
 * @brief Apply binary mask to latent
 *
 * WHAT: Zero out masked positions: z_masked = z * mask
 * WHY:  JEPA uses masking for self-supervised learning
 *
 * @param ctx GPU context
 * @param latent Input latent [batch_size x dim]
 * @param mask Binary mask [batch_size x dim] or [dim]
 * @param masked Output masked latent [batch_size x dim]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_jepa_gpu_apply_mask(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* latent,
    const nimcp_gpu_tensor_t* mask,
    nimcp_gpu_tensor_t* masked
);

/**
 * @brief Generate random block mask
 *
 * WHAT: Create random contiguous block masks
 * WHY:  Block masking is more effective than random masking
 *
 * @param ctx GPU context
 * @param mask Output mask tensor [batch_size x dim]
 * @param block_size Size of each mask block
 * @param mask_ratio Ratio of positions to mask (0.0 to 1.0)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_jepa_gpu_generate_block_mask(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* mask,
    uint32_t block_size,
    float mask_ratio
);

/**
 * @brief Apply soft/attention-weighted mask
 *
 * WHAT: Apply continuous weights to latent positions
 * WHY:  Soft masking allows gradient flow during training
 *
 * @param ctx GPU context
 * @param latent Input latent
 * @param weights Soft mask weights [0, 1]
 * @param masked Output weighted latent
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_jepa_gpu_apply_soft_mask(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* latent,
    const nimcp_gpu_tensor_t* weights,
    nimcp_gpu_tensor_t* masked
);

/* ============================================================================
 * Loss Computation API
 * ============================================================================ */

/**
 * @brief Compute MSE loss between prediction and target on GPU
 *
 * @param ctx GPU context
 * @param prediction Predicted latent
 * @param target Target latent
 * @param mask Optional mask (NULL for no masking)
 * @param loss Output scalar loss value
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_jepa_gpu_compute_loss(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* prediction,
    const nimcp_gpu_tensor_t* target,
    const nimcp_gpu_tensor_t* mask,
    float* loss
);

/**
 * @brief Compute precision-weighted loss
 *
 * WHAT: Loss weighted by precision (inverse variance)
 * WHY:  FEP integration - uncertain dimensions contribute less
 * HOW:  L = mean(precision * (pred - target)^2)
 *
 * @param ctx GPU context
 * @param prediction Predicted latent
 * @param target Target latent
 * @param precision Per-dimension precision weights
 * @param loss Output scalar loss
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_jepa_gpu_compute_precision_loss(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* prediction,
    const nimcp_gpu_tensor_t* target,
    const nimcp_gpu_tensor_t* precision,
    float* loss
);

/* ============================================================================
 * Backward Pass API
 * ============================================================================ */

/**
 * @brief GPU backward pass for predictor
 *
 * @param predictor GPU predictor
 * @param grad_output Gradient from loss [batch_size x output_dim]
 * @param grad_input Output gradient w.r.t. input [batch_size x input_dim]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_jepa_gpu_backward(
    nimcp_jepa_gpu_predictor_t* predictor,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input
);

/**
 * @brief Update predictor weights on GPU
 *
 * @param predictor GPU predictor
 * @param learning_rate Learning rate
 * @param weight_decay L2 regularization coefficient
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_jepa_gpu_update_weights(
    nimcp_jepa_gpu_predictor_t* predictor,
    float learning_rate,
    float weight_decay
);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Copy latent tensor from GPU to CPU
 *
 * @param ctx GPU context
 * @param gpu_latent GPU tensor
 * @param cpu_data Output CPU array
 * @param max_elements Maximum elements to copy
 * @return Number of elements copied, -1 on error
 */
NIMCP_EXPORT int nimcp_jepa_gpu_download_latent(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* gpu_latent,
    float* cpu_data,
    size_t max_elements
);

/**
 * @brief Upload latent tensor from CPU to GPU
 *
 * @param ctx GPU context
 * @param cpu_data CPU source array
 * @param num_elements Number of elements
 * @param gpu_latent Output GPU tensor
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_jepa_gpu_upload_latent(
    nimcp_gpu_context_t* ctx,
    const float* cpu_data,
    size_t num_elements,
    nimcp_gpu_tensor_t* gpu_latent
);

/**
 * @brief Synchronize GPU operations
 *
 * @param ctx GPU context
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_jepa_gpu_synchronize(nimcp_gpu_context_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_JEPA_GPU_H */
