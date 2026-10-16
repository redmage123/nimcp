//=============================================================================
// nimcp_lnn_ode_gpu.h - Advanced GPU ODE Integration for Liquid Neural Networks
//=============================================================================
/**
 * @file nimcp_lnn_ode_gpu.h
 * @brief GPU-accelerated ODE integration kernels for LNN continuous-time dynamics
 *
 * WHAT: Advanced CUDA kernels for ODE solving with parallelization across:
 *       - All neurons in a layer (independent ODE instances)
 *       - Multiple time steps (where applicable)
 *       - Batch dimension (multiple samples)
 *
 * WHY:  LNN neurons are governed by ODEs: dx/dt = f(x, u, t)
 *       GPU parallelization provides massive speedup for neural dynamics
 *
 * HOW:  Specialized kernels for:
 *       - Euler method with batch support
 *       - RK4 (Runge-Kutta 4th order) with fused operations
 *       - Adaptive step size control (embedded error estimation)
 *       - Continuous-time neuron state updates
 *       - Reservoir computing state propagation
 *       - Wiring/connectivity matrix operations
 *
 * INTEGRATION:
 * - Uses existing lnn_ode_method_t enum from nimcp_lnn_types.h
 * - Extends nimcp_lnn_gpu.h with batch and multi-step operations
 * - Integrates with GPU tensor library for efficient computation
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_LNN_ODE_GPU_H
#define NIMCP_LNN_ODE_GPU_H

// Include GPU context BEFORE extern "C" block
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/lnn/nimcp_lnn_gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "lnn/nimcp_lnn_types.h"
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// ODE Integration Configuration
//=============================================================================

/**
 * @brief Extended ODE solver configuration for batch processing
 */
typedef struct {
    lnn_ode_method_t method;           /**< ODE solver method */
    float dt;                           /**< Base time step */
    float dt_min;                       /**< Minimum adaptive dt */
    float dt_max;                       /**< Maximum adaptive dt */
    float error_tolerance;              /**< Error tolerance for adaptive methods */
    float relative_tolerance;           /**< Relative error tolerance */
    uint32_t max_steps;                 /**< Maximum ODE steps per call */
    uint32_t num_substeps;              /**< Number of substeps per dt */
    bool adaptive_stepping;             /**< Enable adaptive time stepping */
    bool use_checkpoint;                /**< Enable state checkpointing */
    bool enable_stability_check;        /**< Check for numerical stability */
    float stability_threshold;          /**< Max state norm before warning */
} nimcp_lnn_ode_batch_config_t;

/**
 * @brief Batch ODE state container
 *
 * WHAT: GPU-resident state tensors for batched LNN forward pass
 * WHY:  Batch processing improves GPU utilization
 * HOW:  3D tensors with batch dimension
 */
typedef struct {
    nimcp_gpu_tensor_t* x;              /**< State [batch, n_neurons] */
    nimcp_gpu_tensor_t* dx_dt;          /**< Derivative [batch, n_neurons] */
    nimcp_gpu_tensor_t* tau;            /**< Time constants [batch, n_neurons] */
    nimcp_gpu_tensor_t* error;          /**< Error estimate [batch, n_neurons] */
    nimcp_gpu_tensor_t* dt_per_sample;  /**< Adaptive dt per sample [batch] */
    uint32_t batch_size;                /**< Current batch size */
    uint32_t n_neurons;                 /**< Number of neurons */
    float current_time;                 /**< Current simulation time */
} nimcp_lnn_ode_batch_state_t;

/**
 * @brief Reservoir computing state for echo state networks
 */
typedef struct {
    nimcp_gpu_tensor_t* reservoir_state; /**< Reservoir state [batch, reservoir_size] */
    nimcp_gpu_tensor_t* W_reservoir;     /**< Reservoir weights [reservoir_size, reservoir_size] */
    nimcp_gpu_tensor_t* W_input;         /**< Input weights [reservoir_size, n_inputs] */
    nimcp_gpu_tensor_t* W_output;        /**< Output weights [n_outputs, reservoir_size] */
    nimcp_gpu_tensor_t* leaking_rate;    /**< Leaking rate per neuron [reservoir_size] */
    uint32_t reservoir_size;             /**< Size of reservoir */
    uint32_t n_inputs;                   /**< Number of inputs */
    uint32_t n_outputs;                  /**< Number of outputs */
    float spectral_radius;               /**< Target spectral radius */
    float sparsity;                      /**< Reservoir sparsity */
} nimcp_lnn_reservoir_state_t;

/**
 * @brief Multi-step integration cache for temporal parallelism
 */
typedef struct {
    nimcp_gpu_tensor_t** k_stages;       /**< RK intermediate stages [n_stages][batch, n_neurons] */
    nimcp_gpu_tensor_t* x_temp;          /**< Temporary state [batch, n_neurons] */
    nimcp_gpu_tensor_t* x_checkpoint;    /**< Checkpoint state for adjoint [batch, n_neurons] */
    uint32_t n_stages;                   /**< Number of RK stages */
    uint32_t checkpoint_interval;        /**< Checkpointing interval for adjoint */
} nimcp_lnn_ode_cache_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Get default batch ODE configuration
 *
 * @return Default configuration (RK4, dt=1.0ms, no adaptive stepping)
 */
NIMCP_EXPORT nimcp_lnn_ode_batch_config_t nimcp_lnn_ode_batch_default_config(void);

//=============================================================================
// Batch ODE Solver Kernels
//=============================================================================

/**
 * @brief Batched Euler step: x_new = x + dt * f(t, x) for all samples
 *
 * WHAT: First-order explicit Euler integration over batch
 * WHY:  Fast baseline integration with full batch parallelism
 * HOW:  2D grid: (neurons, batch) with fused operations
 *
 * @param ctx GPU context
 * @param batch_state Batch ODE state (x and dx_dt must be valid)
 * @param dt Time step
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_lnn_euler_step_batched(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_ode_batch_state_t* batch_state,
    float dt
);

/**
 * @brief Batched RK4 step with fused kernel operations
 *
 * WHAT: Fourth-order Runge-Kutta integration for batch
 * WHY:  High accuracy with optimized memory access patterns
 * HOW:  Fused kernels reduce memory bandwidth by computing
 *       multiple RK stages with minimal intermediate storage
 *
 * @param ctx GPU context
 * @param layer LNN layer GPU state
 * @param input Batched input tensor [batch, n_inputs]
 * @param batch_state Batch ODE state
 * @param cache RK stage cache (for memory reuse)
 * @param config ODE configuration
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_lnn_rk4_step_batched(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    nimcp_lnn_ode_batch_state_t* batch_state,
    nimcp_lnn_ode_cache_t* cache,
    const nimcp_lnn_ode_batch_config_t* config
);

/**
 * @brief Adaptive RK45 (Dormand-Prince) step with per-sample step sizing
 *
 * WHAT: Fifth-order adaptive RK with embedded error estimation
 * WHY:  Automatic step size control per sample in batch
 * HOW:  Each sample has independent dt based on local error
 *
 * @param ctx GPU context
 * @param layer LNN layer GPU state
 * @param input Batched input tensor [batch, n_inputs]
 * @param batch_state Batch ODE state (includes dt_per_sample)
 * @param cache RK stage cache
 * @param config ODE configuration
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_lnn_rk45_adaptive_batched(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    nimcp_lnn_ode_batch_state_t* batch_state,
    nimcp_lnn_ode_cache_t* cache,
    const nimcp_lnn_ode_batch_config_t* config
);

/**
 * @brief Multi-step integration: integrate over multiple timesteps
 *
 * WHAT: Integrate from t to t + n_steps * dt in single kernel launch
 * WHY:  Reduces kernel launch overhead for long simulations
 * HOW:  Loop unrolling with intermediate state in registers/shared memory
 *
 * @param ctx GPU context
 * @param layer LNN layer GPU state
 * @param input_sequence Input sequence [n_steps, batch, n_inputs]
 * @param batch_state Batch ODE state
 * @param cache ODE cache
 * @param config ODE configuration
 * @param n_steps Number of timesteps to integrate
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_lnn_integrate_multistep(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input_sequence,
    nimcp_lnn_ode_batch_state_t* batch_state,
    nimcp_lnn_ode_cache_t* cache,
    const nimcp_lnn_ode_batch_config_t* config,
    uint32_t n_steps
);

//=============================================================================
// Continuous-Time Neuron State Updates
//=============================================================================

/**
 * @brief Compute batched LTC derivative with input-dependent tau
 *
 * WHAT: dx/dt = -x/tau(x,I) + f(W_in*I + W_rec*x + b) for batch
 * WHY:  Core LTC dynamics need batched evaluation for training
 * HOW:  Fused kernel with tau computation + activation
 *
 * @param ctx GPU context
 * @param layer LNN layer GPU state
 * @param input Batched input [batch, n_inputs]
 * @param batch_state Batch ODE state (x, dx_dt, tau updated)
 * @param activation Activation function type
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_lnn_compute_ltc_derivative_batched(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    nimcp_lnn_ode_batch_state_t* batch_state,
    lnn_activation_t activation
);

/**
 * @brief Update tau with stability constraints
 *
 * WHAT: tau = tau_base * sigmoid(W_tau * [x; I] + b_tau), clamped to [tau_min, tau_max]
 * WHY:  Ensure tau stays in stable range during training
 * HOW:  Fused sigmoid + clamp kernel with batched processing
 *
 * @param ctx GPU context
 * @param layer LNN layer GPU state
 * @param input Batched input [batch, n_inputs]
 * @param batch_state Batch ODE state (tau updated)
 * @param tau_min Minimum allowed tau
 * @param tau_max Maximum allowed tau
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_lnn_update_tau_batched(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* input,
    nimcp_lnn_ode_batch_state_t* batch_state,
    float tau_min,
    float tau_max
);

//=============================================================================
// Reservoir Computing Operations
//=============================================================================

/**
 * @brief Initialize reservoir state with echo state network properties
 *
 * WHAT: Create reservoir with controlled spectral radius
 * WHY:  Echo state property requires spectral radius < 1
 * HOW:  Random init + spectral radius scaling
 *
 * @param ctx GPU context
 * @param reservoir Reservoir state to initialize
 * @param reservoir_size Size of reservoir
 * @param n_inputs Number of inputs
 * @param n_outputs Number of outputs
 * @param spectral_radius Target spectral radius (typically 0.9)
 * @param sparsity Reservoir sparsity (typically 0.9 = 90% zeros)
 * @param seed Random seed for reproducibility
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_lnn_reservoir_init(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_reservoir_state_t* reservoir,
    uint32_t reservoir_size,
    uint32_t n_inputs,
    uint32_t n_outputs,
    float spectral_radius,
    float sparsity,
    uint64_t seed
);

/**
 * @brief Reservoir state propagation step
 *
 * WHAT: x(t+1) = (1-a)*x(t) + a*tanh(W_in*u(t) + W*x(t))
 * WHY:  Core echo state network dynamics
 * HOW:  Fused kernel with leaky integration
 *
 * @param ctx GPU context
 * @param reservoir Reservoir state
 * @param input Input tensor [batch, n_inputs]
 * @param output Output tensor [batch, n_outputs] (optional, NULL to skip)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_lnn_reservoir_step(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_reservoir_state_t* reservoir,
    const nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output
);

/**
 * @brief Batch reservoir state propagation over sequence
 *
 * WHAT: Process entire input sequence through reservoir
 * WHY:  Efficient training for echo state networks
 * HOW:  Sequence-parallel processing with state caching
 *
 * @param ctx GPU context
 * @param reservoir Reservoir state
 * @param input_sequence Input sequence [seq_len, batch, n_inputs]
 * @param output_sequence Output sequence [seq_len, batch, n_outputs] (optional)
 * @param state_history State history [seq_len, batch, reservoir_size] (optional)
 * @param seq_len Sequence length
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_lnn_reservoir_propagate_sequence(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_reservoir_state_t* reservoir,
    const nimcp_gpu_tensor_t* input_sequence,
    nimcp_gpu_tensor_t* output_sequence,
    nimcp_gpu_tensor_t* state_history,
    uint32_t seq_len
);

/**
 * @brief Destroy reservoir state and free GPU memory
 *
 * @param reservoir Reservoir state to destroy
 */
NIMCP_EXPORT void nimcp_gpu_lnn_reservoir_destroy(nimcp_lnn_reservoir_state_t* reservoir);

//=============================================================================
// Wiring/Connectivity Matrix Operations
//=============================================================================

/**
 * @brief Apply sparse wiring pattern to state propagation
 *
 * WHAT: Compute W_rec * x using sparse CSR wiring
 * WHY:  NCP wiring is sparse, saving compute and memory
 * HOW:  CSR SpMV kernel optimized for neural connectivity patterns
 *
 * @param ctx GPU context
 * @param layer LNN layer with sparse wiring
 * @param x_batch Batched state [batch, n_neurons]
 * @param output Batched output [batch, n_neurons]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_lnn_apply_wiring_batched(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    const nimcp_gpu_tensor_t* x_batch,
    nimcp_gpu_tensor_t* output
);

/**
 * @brief Generate random wiring pattern on GPU
 *
 * WHAT: Create sparse adjacency matrix with specified density
 * WHY:  Enable on-GPU wiring generation for large networks
 * HOW:  cuRAND + parallel CSR construction
 *
 * @param ctx GPU context
 * @param row_ptr Output: CSR row pointers [n_neurons + 1]
 * @param col_idx Output: CSR column indices [n_edges]
 * @param values Output: Edge weights [n_edges] (NULL for binary)
 * @param n_neurons Number of neurons
 * @param target_density Target connection density (0.0 to 1.0)
 * @param seed Random seed
 * @return Number of edges created, or 0 on failure
 */
NIMCP_EXPORT uint32_t nimcp_gpu_lnn_generate_wiring(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* row_ptr,
    nimcp_gpu_tensor_t* col_idx,
    nimcp_gpu_tensor_t* values,
    uint32_t n_neurons,
    float target_density,
    uint64_t seed
);

/**
 * @brief Compute spectral radius of recurrent weight matrix
 *
 * WHAT: Estimate largest eigenvalue magnitude of W_rec
 * WHY:  Spectral radius controls echo state property
 * HOW:  Power iteration on GPU
 *
 * @param ctx GPU context
 * @param layer LNN layer with W_rec
 * @param spectral_radius Output: estimated spectral radius
 * @param max_iterations Maximum power iteration steps
 * @param tolerance Convergence tolerance
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_lnn_compute_spectral_radius(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    float* spectral_radius,
    uint32_t max_iterations,
    float tolerance
);

/**
 * @brief Rescale weights to target spectral radius
 *
 * WHAT: Scale W_rec so spectral_radius(W_rec) = target
 * WHY:  Initialize reservoir with controlled dynamics
 * HOW:  W_rec = W_rec * (target / current_spectral_radius)
 *
 * @param ctx GPU context
 * @param layer LNN layer with W_rec to rescale
 * @param target_spectral_radius Target spectral radius
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_lnn_rescale_spectral_radius(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_layer_gpu_t* layer,
    float target_spectral_radius
);

//=============================================================================
// State and Cache Management
//=============================================================================

/**
 * @brief Create batch ODE state container
 *
 * @param ctx GPU context
 * @param batch_size Number of samples in batch
 * @param n_neurons Number of neurons
 * @return Batch state or NULL on failure
 */
NIMCP_EXPORT nimcp_lnn_ode_batch_state_t* nimcp_lnn_ode_batch_state_create(
    nimcp_gpu_context_t* ctx,
    uint32_t batch_size,
    uint32_t n_neurons
);

/**
 * @brief Destroy batch ODE state
 *
 * @param state Batch state to destroy
 */
NIMCP_EXPORT void nimcp_lnn_ode_batch_state_destroy(nimcp_lnn_ode_batch_state_t* state);

/**
 * @brief Reset batch state to initial conditions
 *
 * @param ctx GPU context
 * @param state Batch state to reset
 * @param initial_x Initial state value (uniform)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_lnn_ode_batch_state_reset(
    nimcp_gpu_context_t* ctx,
    nimcp_lnn_ode_batch_state_t* state,
    float initial_x
);

/**
 * @brief Create ODE cache for RK intermediate stages
 *
 * @param ctx GPU context
 * @param batch_size Number of samples
 * @param n_neurons Number of neurons
 * @param n_stages Number of RK stages (4 for RK4, 7 for DOPRI5)
 * @return ODE cache or NULL on failure
 */
NIMCP_EXPORT nimcp_lnn_ode_cache_t* nimcp_lnn_ode_cache_create(
    nimcp_gpu_context_t* ctx,
    uint32_t batch_size,
    uint32_t n_neurons,
    uint32_t n_stages
);

/**
 * @brief Destroy ODE cache
 *
 * @param cache Cache to destroy
 */
NIMCP_EXPORT void nimcp_lnn_ode_cache_destroy(nimcp_lnn_ode_cache_t* cache);

//=============================================================================
// Stability and Diagnostics
//=============================================================================

/**
 * @brief Check batch state for numerical issues
 *
 * WHAT: Detect NaN, Inf, and explosion in batch state
 * WHY:  Early detection prevents wasted computation
 * HOW:  Parallel reduction for min/max/isnan checks
 *
 * @param ctx GPU context
 * @param batch_state Batch state to check
 * @param has_nan Output: true if NaN detected
 * @param has_inf Output: true if Inf detected
 * @param max_norm Output: maximum L2 norm across batch
 * @return true on success (check outputs for issues)
 */
NIMCP_EXPORT bool nimcp_gpu_lnn_check_stability(
    nimcp_gpu_context_t* ctx,
    const nimcp_lnn_ode_batch_state_t* batch_state,
    bool* has_nan,
    bool* has_inf,
    float* max_norm
);

/**
 * @brief Compute state statistics for monitoring
 *
 * WHAT: Mean, std, min, max of state across batch
 * WHY:  Monitor training dynamics
 * HOW:  Efficient parallel reductions
 *
 * @param ctx GPU context
 * @param batch_state Batch state
 * @param mean Output: mean state value
 * @param std Output: standard deviation
 * @param min_val Output: minimum value
 * @param max_val Output: maximum value
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_lnn_compute_state_stats(
    nimcp_gpu_context_t* ctx,
    const nimcp_lnn_ode_batch_state_t* batch_state,
    float* mean,
    float* std,
    float* min_val,
    float* max_val
);

//=============================================================================
// CPU Fallbacks
//=============================================================================

/**
 * @brief CPU fallback for batched Euler step
 *
 * WHAT: CPU implementation when GPU not available
 * WHY:  Ensure code works on non-GPU systems
 * HOW:  Serial loop over batch and neurons
 *
 * @param x Current state [batch, n_neurons]
 * @param dx_dt Derivative [batch, n_neurons]
 * @param x_new Output state [batch, n_neurons]
 * @param batch_size Number of samples
 * @param n_neurons Number of neurons
 * @param dt Time step
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_cpu_lnn_euler_step_batched(
    const float* x,
    const float* dx_dt,
    float* x_new,
    uint32_t batch_size,
    uint32_t n_neurons,
    float dt
);

/**
 * @brief CPU fallback for batched RK4 step
 *
 * @param x Current state [batch, n_neurons]
 * @param tau Time constants [batch, n_neurons]
 * @param input Input [batch, n_inputs]
 * @param W_in Input weights [n_neurons, n_inputs]
 * @param W_rec Recurrent weights [n_neurons, n_neurons]
 * @param b_in Bias [n_neurons]
 * @param x_new Output state [batch, n_neurons]
 * @param batch_size Number of samples
 * @param n_neurons Number of neurons
 * @param n_inputs Number of inputs
 * @param dt Time step
 * @param activation Activation function
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_cpu_lnn_rk4_step_batched(
    const float* x,
    const float* tau,
    const float* input,
    const float* W_in,
    const float* W_rec,
    const float* b_in,
    float* x_new,
    uint32_t batch_size,
    uint32_t n_neurons,
    uint32_t n_inputs,
    float dt,
    lnn_activation_t activation
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_ODE_GPU_H */
