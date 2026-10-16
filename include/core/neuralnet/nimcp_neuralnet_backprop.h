/**
 * @file nimcp_neuralnet_backprop.h
 * @brief Backpropagation for weight gradient computation in neural networks
 *
 * WHAT: Compute gradients for all synaptic weights using backpropagation
 * WHY:  Required for gradient-based learning (SGD, Adam, etc.)
 * HOW:  Chain rule through network layers
 *
 * ARCHITECTURE:
 * 1. Forward pass: Record activations for each layer
 * 2. Backward pass: Propagate output gradients to compute weight gradients
 *
 * INTEGRATION:
 * - Used by nimcp_brain_train_step() for weight updates
 * - Integrates with existing loss functions and optimizers
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#ifndef NIMCP_NEURALNET_BACKPROP_H
#define NIMCP_NEURALNET_BACKPROP_H

#include "core/neuralnet/nimcp_neuralnet.h"
#include "utils/validation/nimcp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Activation record for a single layer
 *
 * Stores pre-activation (z) and post-activation (a) values
 * needed for gradient computation during backpropagation.
 */
typedef struct layer_activation {
    float* pre_activation;    /**< z = W*a_prev + b (before activation fn) */
    float* post_activation;   /**< a = f(z) (after activation fn) */
    uint32_t size;            /**< Number of neurons in this layer */
} layer_activation_t;

/**
 * @brief Backpropagation context
 *
 * Stores activations from forward pass and gradients from backward pass.
 */
typedef struct backprop_ctx {
    neural_network_t network;        /**< Reference to network */
    layer_activation_t* activations; /**< Activations per layer */
    uint32_t num_layers;             /**< Number of layers */
    float* weight_gradients;         /**< Computed weight gradients [total_weights] */
    float* bias_gradients;           /**< Computed bias gradients [total_neurons] */
    size_t total_weights;            /**< Total number of weights */
    size_t total_neurons;            /**< Total number of neurons (exc. input) */
    bool gradients_valid;            /**< Whether gradients have been computed */
    float** layer_deltas;            /**< Persistent per-layer delta buffers */
    uint32_t layer_deltas_count;     /**< Number of layer delta buffers */
} backprop_ctx_t;

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Create backpropagation context for a neural network
 *
 * Allocates buffers for activations and gradients based on network topology.
 *
 * @param network Neural network to create context for
 * @return Context pointer, or NULL on failure
 */
backprop_ctx_t* backprop_create(neural_network_t network);

/**
 * @brief Destroy backpropagation context and free resources
 *
 * @param ctx Context to destroy
 */
void backprop_destroy(backprop_ctx_t* ctx);

//=============================================================================
// Forward Pass
//=============================================================================

/**
 * @brief Perform forward pass with activation recording
 *
 * Computes network output while storing intermediate activations
 * for subsequent backpropagation.
 *
 * @param ctx Backprop context
 * @param inputs Input features [input_size]
 * @param input_size Number of input features
 * @param outputs Output buffer [output_size]
 * @param output_size Number of outputs
 * @return true on success, false on failure
 */
bool backprop_forward(backprop_ctx_t* ctx,
                      const float* inputs, uint32_t input_size,
                      float* outputs, uint32_t output_size);

//=============================================================================
// Backward Pass
//=============================================================================

/**
 * @brief Perform backward pass to compute weight gradients
 *
 * Given output gradients (dL/dOutput from loss function), computes
 * gradients for all weights (dL/dWeight) using the chain rule.
 *
 * ALGORITHM:
 * For each layer L from output to input:
 *   1. delta_L = output_grad * activation_derivative(z_L)  [for output layer]
 *   2. delta_L = (W_{L+1}^T * delta_{L+1}) * activation_derivative(z_L)  [for hidden]
 *   3. dW_L = delta_L * a_{L-1}^T  [weight gradient]
 *   4. db_L = delta_L  [bias gradient]
 *
 * @param ctx Backprop context (must have valid activations from forward pass)
 * @param output_gradients dL/dOutput from loss function [output_size]
 * @param output_size Number of outputs
 * @return true on success, false on failure
 */
bool backprop_backward(backprop_ctx_t* ctx,
                       const float* output_gradients, uint32_t output_size);

//=============================================================================
// Gradient Access
//=============================================================================

/**
 * @brief Get weight gradients after backward pass
 *
 * @param ctx Backprop context
 * @param gradients Output buffer [total_weights]
 * @param count Size of output buffer
 * @return Number of gradients copied, 0 on failure
 */
size_t backprop_get_weight_gradients(const backprop_ctx_t* ctx,
                                     float* gradients, size_t count);

/**
 * @brief Get total weight count
 *
 * @param ctx Backprop context
 * @return Total number of weights in network
 */
size_t backprop_get_weight_count(const backprop_ctx_t* ctx);

/**
 * @brief Get bias gradients after backward pass
 *
 * @param ctx Backprop context
 * @param gradients Output buffer [total_neurons]
 * @param count Size of output buffer
 * @return Number of gradients copied, 0 on failure
 */
size_t backprop_get_bias_gradients(const backprop_ctx_t* ctx,
                                   float* gradients, size_t count);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Clear recorded activations and gradients
 *
 * Call before starting a new forward pass.
 *
 * @param ctx Backprop context
 */
void backprop_clear(backprop_ctx_t* ctx);

/**
 * @brief Check if gradients are valid (backward pass completed)
 *
 * @param ctx Backprop context
 * @return true if gradients are valid, false otherwise
 */
bool backprop_has_valid_gradients(const backprop_ctx_t* ctx);

/**
 * @brief Store activations from network neuron states
 *
 * After running the network's forward pass, this function extracts
 * activation values from neuron states and stores them in the
 * backprop context for gradient computation.
 *
 * @param ctx Backprop context
 * @param inputs Input features (stored as input layer activations)
 * @param input_size Number of input features
 */
void backprop_store_activations_from_network(backprop_ctx_t* ctx,
                                             const float* inputs,
                                             uint32_t input_size);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEURALNET_BACKPROP_H */
