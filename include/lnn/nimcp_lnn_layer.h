/**
 * @file nimcp_lnn_layer.h
 * @brief LNN Layer API - Collection of LTC Neurons with Wiring
 * @version 1.0.0
 * @date 2025-12-20
 *
 * WHAT: Layer of Liquid Time-Constant neurons with recurrent connectivity
 * WHY:  Provides continuous-time dynamics with learnable time constants for NIMCP modules
 * HOW:  Vectorized computation using tensors, sparse recurrent connections via wiring patterns
 *
 * ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                          LNN LAYER STRUCTURE                             │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │                                                                          │
 * │   INPUT [n_inputs]                                                       │
 * │      │                                                                   │
 * │      ▼                                                                   │
 * │   ┌────────────────────────────────────────────────────────────────┐    │
 * │   │           INPUT TRANSFORMATION (W_in)                           │    │
 * │   │   h_in = W_in @ input + b_in       [n_neurons]                 │    │
 * │   └──────────────────────────┬─────────────────────────────────────┘    │
 * │                              │                                           │
 * │                              ▼                                           │
 * │   ┌────────────────────────────────────────────────────────────────┐    │
 * │   │           RECURRENT TRANSFORMATION (W_rec)                      │    │
 * │   │   h_rec = W_rec @ x        [n_neurons]                         │    │
 * │   │   (Sparse if wiring != FULL)                                   │    │
 * │   └──────────────────────────┬─────────────────────────────────────┘    │
 * │                              │                                           │
 * │                              ▼                                           │
 * │   ┌────────────────────────────────────────────────────────────────┐    │
 * │   │      TIME CONSTANT MODULATION (τ computation)                  │    │
 * │   │   tau_input = concat([h_in, h_rec])                            │    │
 * │   │   tau = tau_base * sigmoid(W_tau @ tau_input + b_tau)          │    │
 * │   │   (bounds τ to [tau_min, tau_max])                             │    │
 * │   └──────────────────────────┬─────────────────────────────────────┘    │
 * │                              │                                           │
 * │                              ▼                                           │
 * │   ┌────────────────────────────────────────────────────────────────┐    │
 * │   │                  ODE DYNAMICS                                   │    │
 * │   │   dx/dt = -x/τ + activation(h_in + h_rec)                      │    │
 * │   │   x_new = ODE_solver(x, dx/dt, dt, method)                     │    │
 * │   │   (RK4, Euler, Heun, etc.)                                     │    │
 * │   └──────────────────────────┬─────────────────────────────────────┘    │
 * │                              │                                           │
 * │                              ▼                                           │
 * │   OUTPUT [n_neurons] = x_new                                             │
 * │                                                                          │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * MATHEMATICAL MODEL:
 * ===================
 *
 * LTC Neuron Dynamics (per neuron i):
 *   dx_i/dt = -(1/τ_i(t)) * x_i + f(W_in[i] @ input + W_rec[i] @ x)
 *
 * where:
 *   x_i       = neuron state (activation)
 *   τ_i(t)    = input-dependent time constant
 *   f         = activation function (tanh, sigmoid, etc.)
 *   W_in[i]   = input weights (row i of W_in)
 *   W_rec[i]  = recurrent weights (row i of W_rec, sparse)
 *
 * Time Constant Modulation:
 *   τ_i(t) = τ_base[i] * σ(W_tau[i] @ [input; x] + b_tau[i])
 *   where σ = sigmoid bounds τ to (0, 2*τ_base)
 *
 * VECTORIZED FORM:
 * ----------------
 * For SIMD efficiency, layer computes all neurons in parallel:
 *
 *   h_in   = W_in @ input + b_in              [n_neurons]
 *   h_rec  = W_rec @ x                        [n_neurons]
 *   tau    = tau_base ⊙ σ(W_tau @ [h_in; h_rec] + b_tau)
 *   dx/dt  = -x ⊘ tau + f(h_in + h_rec)
 *   x_next = x + ODE_step(dx/dt, dt)
 *
 * GRADIENT COMPUTATION:
 * =====================
 *
 * Adjoint Method (memory-efficient for long sequences):
 *   dL/dθ = ∫ λ(t)^T ∂f/∂θ dt
 *   where λ(t) solves: dλ/dt = -∂f/∂x^T λ - ∂L/∂x
 *
 * Gradients w.r.t. parameters:
 *   ∂L/∂W_in, ∂L/∂W_rec, ∂L/∂W_tau, ∂L/∂b_in, ∂L/∂b_tau, ∂L/∂tau_base
 *
 * SPARSE WIRING:
 * ==============
 *
 * Recurrent matrix W_rec can be sparse for efficiency:
 *   - FULL: Dense n×n matrix
 *   - RANDOM: Random k connections per neuron (Erdos-Renyi)
 *   - SMALL_WORLD: Watts-Strogatz (local + random long-range)
 *   - SCALE_FREE: Barabasi-Albert (hub neurons)
 *   - NCP: Neural Circuit Policy (sensory→inter→command→motor)
 *
 * Sparse matmul: y = W_rec @ x uses CSR format for efficiency
 *
 * NIMCP STANDARDS:
 * ================
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 * - nimcp_tensor_t for all vectorized operations
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_LNN_LAYER_H
#define NIMCP_LNN_LAYER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "lnn/nimcp_lnn_types.h"
#include "lnn/nimcp_lnn_config.h"
#include "utils/tensor/nimcp_tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Type lnn_layer_config_t is defined in nimcp_lnn_config.h */

/*=============================================================================
 * Layer Lifecycle
 *===========================================================================*/

/**
 * @brief Create LNN layer
 *
 * WHAT: Allocate and initialize layer with given configuration
 * WHY:  Core layer creation operation
 * HOW:  Allocate neurons, weight tensors, initialize wiring pattern
 *
 * @param config Layer configuration
 * @param n_inputs Number of input features
 * @return Layer handle or NULL on failure
 */
lnn_layer_t* lnn_layer_create(const lnn_layer_config_t* config, uint32_t n_inputs);

/**
 * @brief Destroy layer and free resources
 *
 * WHAT: Free all layer memory (neurons, weights, tensors)
 * WHY:  Clean shutdown
 * HOW:  Free wiring, destroy tensors, free neuron array
 *
 * @param layer Layer to destroy
 */
void lnn_layer_destroy(lnn_layer_t* layer);

/**
 * @brief Initialize layer weights
 *
 * WHAT: Initialize W_in, W_rec, W_tau with random values
 * WHY:  Proper initialization critical for training
 * HOW:  Xavier/He initialization scaled by std, apply sparsity mask
 *
 * @param layer Layer to initialize
 * @param std Standard deviation for weight initialization
 * @param seed Random seed (0 = use time)
 * @return 0 on success, negative on error
 */
int lnn_layer_init_weights(lnn_layer_t* layer, float std, uint64_t seed);

/*=============================================================================
 * Forward Computation (Vectorized)
 *===========================================================================*/

/**
 * @brief Forward pass through layer
 *
 * WHAT: Compute one time step: input → state update → output
 * WHY:  Core inference operation
 * HOW:  1. Compute τ(x,I)  2. Compute dx/dt  3. ODE step  4. Update x
 *
 * @param layer LNN layer
 * @param input Input tensor [n_inputs]
 * @param output Output tensor [n_neurons] (pre-allocated)
 * @param dt Time step (0 = use layer default)
 * @return 0 on success, negative on error
 */
int lnn_layer_forward(
    lnn_layer_t* layer,
    const nimcp_tensor_t* input,
    nimcp_tensor_t* output,
    float dt
);

/**
 * @brief Compute time constants for current input/state
 *
 * WHAT: τ = τ_base * sigmoid(W_tau @ [h_in; h_rec] + b_tau)
 * WHY:  Time constants adapt to input
 * HOW:  Concatenate input and recurrent signals, apply W_tau, sigmoid
 *
 * @param layer LNN layer
 * @param input Input tensor [n_inputs]
 * @return 0 on success, updates layer->tau
 */
int lnn_layer_compute_tau(lnn_layer_t* layer, const nimcp_tensor_t* input);

/**
 * @brief Compute state derivatives dx/dt
 *
 * WHAT: dx/dt = -x/τ + activation(W_in @ input + W_rec @ x + b_in)
 * WHY:  Needed for ODE integration
 * HOW:  Compute input/recurrent terms, apply activation, subtract decay
 *
 * @param layer LNN layer
 * @param input Input tensor [n_inputs]
 * @return 0 on success, updates layer->dx_dt
 */
int lnn_layer_compute_derivatives(lnn_layer_t* layer, const nimcp_tensor_t* input);

/**
 * @brief Take one ODE integration step
 *
 * WHAT: x_next = x + ODE_step(dx/dt, dt, method)
 * WHY:  Advance continuous-time dynamics
 * HOW:  Use specified ODE solver (Euler, RK4, etc.)
 *
 * @param layer LNN layer
 * @param dt Time step
 * @param method ODE solver method (overrides layer default if specified)
 * @return 0 on success, updates layer->x
 */
int lnn_layer_step(lnn_layer_t* layer, float dt, lnn_ode_method_t method);

/*=============================================================================
 * State Management
 *===========================================================================*/

/**
 * @brief Get current layer state
 *
 * WHAT: Copy current x values to output tensor
 * WHY:  State introspection, checkpointing
 * HOW:  Clone layer->x tensor
 *
 * @param layer LNN layer
 * @param state Output state tensor [n_neurons] (allocated by function)
 * @return 0 on success
 */
int lnn_layer_get_state(const lnn_layer_t* layer, nimcp_tensor_t** state);

/**
 * @brief Set layer state
 *
 * WHAT: Set x to given values
 * WHY:  State restoration, initialization
 * HOW:  Copy state tensor to layer->x
 *
 * @param layer LNN layer
 * @param state State tensor [n_neurons]
 * @return 0 on success
 */
int lnn_layer_set_state(lnn_layer_t* layer, const nimcp_tensor_t* state);

/**
 * @brief Get current time constants
 *
 * WHAT: Copy current τ values to output tensor
 * WHY:  Introspection, debugging
 * HOW:  Clone layer->tau tensor
 *
 * @param layer LNN layer
 * @param tau Output tau tensor [n_neurons] (allocated by function)
 * @return 0 on success
 */
int lnn_layer_get_tau(const lnn_layer_t* layer, nimcp_tensor_t** tau);

/**
 * @brief Reset layer state to zero
 *
 * WHAT: Set x, dx_dt to zero
 * WHY:  Between sequences, reset dynamics
 * HOW:  Zero out state tensors
 *
 * @param layer LNN layer
 */
void lnn_layer_reset(lnn_layer_t* layer);

/*=============================================================================
 * Gradient Computation
 *===========================================================================*/

/**
 * @brief Backward pass (compute gradients)
 *
 * WHAT: Compute ∂L/∂W_in, ∂L/∂W_rec, ∂L/∂W_tau given upstream gradient
 * WHY:  Training via backpropagation through time or adjoint method
 * HOW:  Chain rule: dL/dθ = dL/dx * dx/dθ
 *
 * @param layer LNN layer
 * @param upstream_grad Gradient flowing from next layer [n_neurons]
 * @return 0 on success, updates layer gradient tensors
 */
int lnn_layer_backward(lnn_layer_t* layer, const nimcp_tensor_t* upstream_grad);

/**
 * @brief Reset accumulated gradients to zero
 *
 * WHAT: Zero out all gradient tensors
 * WHY:  Between training batches
 * HOW:  Set grad_W_in, grad_W_rec, etc. to zero
 *
 * @param layer LNN layer
 */
void lnn_layer_reset_gradients(lnn_layer_t* layer);

/**
 * @brief Get gradient tensors
 *
 * WHAT: Return pointers to all gradient tensors
 * WHY:  Access for optimizer
 * HOW:  Fill grads array with pointers
 *
 * @param layer LNN layer
 * @param grads Output array of gradient tensors (allocated by caller)
 * @param n_grads Output number of gradient tensors
 * @return 0 on success
 */
int lnn_layer_get_gradients(
    const lnn_layer_t* layer,
    nimcp_tensor_t** grads,
    uint32_t* n_grads
);

/**
 * @brief Apply gradients with learning rate
 *
 * WHAT: θ = θ - lr * ∇θ
 * WHY:  Simple SGD update
 * HOW:  Subtract scaled gradients from parameters
 *
 * @param layer LNN layer
 * @param learning_rate Learning rate
 * @return 0 on success
 */
int lnn_layer_apply_gradients(lnn_layer_t* layer, float learning_rate);

/*=============================================================================
 * Statistics and Introspection
 *===========================================================================*/

/**
 * @brief Get layer statistics
 *
 * WHAT: Compute average/min/max τ, state norm
 * WHY:  Monitoring, debugging
 * HOW:  Reduce over tau and x tensors
 *
 * @param layer LNN layer
 * @param avg_tau Output average time constant
 * @param min_tau Output minimum time constant
 * @param max_tau Output maximum time constant
 * @param state_norm Output L2 norm of state
 * @return 0 on success
 */
int lnn_layer_get_stats(
    const lnn_layer_t* layer,
    float* avg_tau,
    float* min_tau,
    float* max_tau,
    float* state_norm
);

/**
 * @brief Get total number of trainable parameters
 *
 * WHAT: Count all learnable weights
 * WHY:  Model capacity reporting
 * HOW:  Sum numel of W_in, W_rec, W_tau, biases, tau_base
 *
 * @param layer LNN layer
 * @return Total parameter count
 */
size_t lnn_layer_param_count(const lnn_layer_t* layer);

/**
 * @brief Get default layer configuration
 *
 * WHAT: Fill config with sensible defaults
 * WHY:  Convenience for common use cases
 * HOW:  Set standard values (tanh activation, RK4 solver, etc.)
 *
 * @param config Output configuration
 */
void lnn_layer_config_default(lnn_layer_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_LAYER_H */
