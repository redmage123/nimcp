/**
 * @file nimcp_lnn_gradient.h
 * @brief Adjoint method for gradient computation in LNN
 *
 * WHAT: Memory-efficient gradient computation for continuous-time networks
 * WHY:  BPTT requires O(T) memory, adjoint requires O(1)
 * HOW:  Solve adjoint ODE backwards: dλ/dt = -∂f/∂x^T λ - ∂L/∂x
 *
 * MATHEMATICAL FOUNDATION:
 *
 * Forward pass:
 *   dx/dt = f(x, θ, t)    from t0 to T
 *   Record: x(T) only
 *
 * Backward pass (adjoint):
 *   dλ/dt = -∂f/∂x^T λ    from T to t0
 *   λ(T) = ∂L/∂x(T)
 *
 * Parameter gradients:
 *   ∂L/∂θ = ∫[t0,T] λ(t)^T ∂f/∂θ dt
 *
 * Memory efficiency:
 *   BPTT: O(T) states
 *   Adjoint: O(1) states (only current)
 *   Adjoint+Checkpointing: O(√T) tradeoff
 *
 * BIOLOGICAL GROUNDING:
 * - Adjoint method models error signal propagation
 * - Checkpointing models episodic memory retrieval
 * - Continuous gradients match analog neural computation
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#ifndef NIMCP_LNN_GRADIENT_H
#define NIMCP_LNN_GRADIENT_H

#include "lnn/nimcp_lnn_types.h"
#include "utils/tensor/nimcp_tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Types lnn_network_t, lnn_layer_t, lnn_gradient_ctx_t are forward declared
 * in nimcp_lnn_types.h. The complete definition of lnn_gradient_ctx_s is below. */

/*=============================================================================
 * Gradient Context Structure
 *===========================================================================*/

/**
 * @brief Gradient computation context for adjoint method
 *
 * WHAT: Holds adjoint state and accumulated gradients
 * WHY:  Encapsulates gradient computation state
 * HOW:  Maintains adjoint variables λ and parameter gradients
 */
struct lnn_gradient_ctx_s {
    /* Network reference */
    lnn_network_t* network;         /**< Associated LNN network */

    /* Adjoint state */
    nimcp_tensor_t* adjoint;        /**< Current λ(t) adjoint variables */
    nimcp_tensor_t* adjoint_prev;   /**< Previous adjoint state */
    nimcp_tensor_t* adjoint_temp;   /**< Temporary for ODE steps */

    /* Accumulated parameter gradients */
    nimcp_tensor_t* grad_params;    /**< Total ∂L/∂θ */
    nimcp_tensor_t* grad_temp;      /**< Temporary gradient accumulator */

    /* Time integration */
    float t_start;                  /**< Start time */
    float t_end;                    /**< End time */
    float dt;                       /**< Time step (negative for backward) */
    uint32_t n_steps;               /**< Number of time steps */
    uint32_t current_step;          /**< Current step index */

    /* Loss gradient */
    nimcp_tensor_t* dL_dx_final;    /**< ∂L/∂x at final time */

    /* Checkpointing for memory efficiency */
    bool use_checkpointing;         /**< Enable checkpointing */
    uint32_t checkpoint_interval;   /**< Steps between checkpoints */
    nimcp_tensor_t** checkpoints;   /**< Saved states */
    uint32_t n_checkpoints;         /**< Number of checkpoints */
    uint32_t checkpoint_capacity;   /**< Allocated checkpoint slots */

    /* Jacobian storage (for adjoint ODE) */
    nimcp_tensor_t** jacobians;     /**< ∂f/∂x per layer */
    uint32_t n_jacobians;

    /* Statistics */
    uint64_t adjoint_steps;         /**< Number of adjoint ODE steps */
    uint64_t jacobian_evals;        /**< Jacobian evaluations */
    uint64_t checkpoints_used;      /**< Checkpoint accesses */
    double compute_time_ms;         /**< Total computation time */

    /* Health monitoring */
    float gradient_norm;            /**< Current gradient norm */
    float max_gradient;             /**< Maximum gradient seen */
    bool has_nan;                   /**< NaN detected */
    bool has_inf;                   /**< Inf detected */

    /* Adjoint truncation */
    uint32_t max_adjoint_steps;     /**< Max backward steps (0 = unlimited) */

    /* Input gradient (final adjoint state = dL/d_input) */
    nimcp_tensor_t* last_input_grad; /**< λ(t=0) from last adjoint computation */

    /* Thread safety */
    void* mutex;                    /**< Mutex for thread safety */
};

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

/**
 * @brief Create gradient context for adjoint method
 *
 * WHAT: Allocate gradient computation context
 * WHY:  Required for memory-efficient gradient computation
 * HOW:  Allocate adjoint variables and gradient storage
 *
 * @param network LNN network
 * @param max_steps Maximum sequence length
 * @param use_checkpointing Enable memory-computation tradeoff
 * @param checkpoint_interval Steps between checkpoints (0 = auto)
 * @return Gradient context or NULL on failure
 */
lnn_gradient_ctx_t* lnn_gradient_ctx_create(
    lnn_network_t* network,
    uint32_t max_steps,
    bool use_checkpointing,
    uint32_t checkpoint_interval
);

/**
 * @brief Destroy gradient context
 *
 * WHAT: Free all gradient computation resources
 * WHY:  Clean memory management
 * HOW:  Destroy tensors, free checkpoints, release mutex
 *
 * @param ctx Gradient context
 */
void lnn_gradient_ctx_destroy(lnn_gradient_ctx_t* ctx);

/*=============================================================================
 * Gradient Computation Functions
 *===========================================================================*/

/**
 * @brief Compute gradients using adjoint method
 *
 * WHAT: Solve adjoint ODE backward to compute ∂L/∂θ
 * WHY:  O(1) memory vs O(T) for BPTT
 * HOW:  Integrate dλ/dt = -∂f/∂x^T λ from T to t0, accumulate ∂L/∂θ
 *
 * Algorithm:
 * 1. Initialize λ(T) = ∂L/∂x(T) from loss
 * 2. For t = T to t0:
 *    - Compute Jacobian ∂f/∂x at current x(t)
 *    - Solve dλ/dt = -∂f/∂x^T λ (backward step)
 *    - Accumulate ∂L/∂θ += λ^T ∂f/∂θ * dt
 * 3. Return accumulated gradients
 *
 * @param ctx Gradient context (pre-allocated)
 * @param network LNN network (with recorded forward pass)
 * @param dL_dx_final Gradient of loss w.r.t. final state [n_neurons]
 * @return 0 on success, negative on error
 */
int lnn_gradient_compute_adjoint(
    lnn_gradient_ctx_t* ctx,
    lnn_network_t* network,
    const nimcp_tensor_t* dL_dx_final
);

/**
 * @brief Compute gradients using BPTT (Backprop Through Time)
 *
 * WHAT: Standard BPTT gradient computation
 * WHY:  Alternative to adjoint, useful for short sequences
 * HOW:  Unroll network in time, backprop through each step
 *
 * @param ctx Gradient context
 * @param network LNN network
 * @param dL_dx_sequence Loss gradients over sequence [T, n_neurons]
 * @return 0 on success
 */
int lnn_gradient_compute_bptt(
    lnn_gradient_ctx_t* ctx,
    lnn_network_t* network,
    const nimcp_tensor_t* dL_dx_sequence
);

/**
 * @brief Parallel gradient computation across batch
 *
 * WHAT: Compute gradients for batch of sequences in parallel
 * WHY:  Exploit parallelism for faster training
 * HOW:  Thread pool distributes batch items across workers
 *
 * @param ctx Gradient context
 * @param network LNN network
 * @param dL_dx_batch Loss gradients [batch, seq_len, n_outputs]
 * @param batch_size Batch size
 * @param thread_pool Thread pool handle (nimcp_thread_pool_t*)
 * @return 0 on success
 */
int lnn_gradient_compute_batch_parallel(
    lnn_gradient_ctx_t* ctx,
    lnn_network_t* network,
    const nimcp_tensor_t* dL_dx_batch,
    uint32_t batch_size,
    void* thread_pool
);

/*=============================================================================
 * Adjoint ODE Solving
 *===========================================================================*/

/**
 * @brief Single adjoint ODE step
 *
 * WHAT: Integrate dλ/dt = -∂f/∂x^T λ for one time step
 * WHY:  Core adjoint computation
 * HOW:  Use same ODE solver as forward pass (RK4, Euler, etc.)
 *
 * Math:
 *   dλ/dt = -J^T λ  where J = ∂f/∂x is Jacobian
 *
 * @param ctx Gradient context
 * @param x Current state x(t)
 * @param adjoint Current adjoint λ(t)
 * @param t Current time
 * @param dt Time step (negative for backward)
 * @param adjoint_next Output: λ(t-dt)
 * @return 0 on success
 */
int lnn_gradient_adjoint_step(
    lnn_gradient_ctx_t* ctx,
    const nimcp_tensor_t* x,
    const nimcp_tensor_t* adjoint,
    float t,
    float dt,
    nimcp_tensor_t* adjoint_next
);

/**
 * @brief Compute Jacobian ∂f/∂x for layer
 *
 * WHAT: Compute Jacobian matrix of layer dynamics
 * WHY:  Required for adjoint ODE dλ/dt = -J^T λ
 * HOW:  Analytical derivatives or finite differences
 *
 * For LTC neuron: dx/dt = -x/τ(x,I) + σ(W*I + W_rec*x + b)
 * Jacobian includes:
 *   - ∂(-x/τ)/∂x = -1/τ + x/τ² * ∂τ/∂x
 *   - ∂σ(...)/∂x = σ'(...) * W_rec
 *
 * @param layer LNN layer
 * @param x Current state
 * @param jacobian Output Jacobian [n_neurons, n_neurons]
 * @return 0 on success
 */
int lnn_gradient_compute_jacobian(
    const lnn_layer_t* layer,
    const nimcp_tensor_t* x,
    nimcp_tensor_t* jacobian
);

/*=============================================================================
 * Gradient Access and Application
 *===========================================================================*/

/**
 * @brief Get accumulated parameter gradients
 *
 * WHAT: Extract final ∂L/∂θ after adjoint computation
 * WHY:  Transfer gradients to optimizer
 * HOW:  Copy from internal storage
 *
 * @param ctx Gradient context
 * @param grad_params Output tensor (allocated by caller)
 * @return 0 on success
 */
int lnn_gradient_get_params(
    const lnn_gradient_ctx_t* ctx,
    nimcp_tensor_t* grad_params
);

/**
 * @brief Apply gradients to network using optimizer
 *
 * WHAT: Update network parameters: θ ← θ - lr * ∂L/∂θ
 * WHY:  Convenience function for training loop
 * HOW:  Call optimizer update with accumulated gradients
 *
 * @param ctx Gradient context
 * @param network LNN network to update
 * @param optimizer Optimizer handle (void* to nimcp_optimizer_context_t*)
 * @return 0 on success
 */
int lnn_gradient_apply(
    lnn_gradient_ctx_t* ctx,
    lnn_network_t* network,
    void* optimizer
);

/**
 * @brief Reset gradient accumulator
 *
 * WHAT: Zero out accumulated gradients
 * WHY:  Prepare for next batch
 * HOW:  Set grad_params to zero
 *
 * @param ctx Gradient context
 */
void lnn_gradient_reset(lnn_gradient_ctx_t* ctx);

/*=============================================================================
 * Checkpointing Functions
 *===========================================================================*/

/**
 * @brief Save state checkpoint
 *
 * WHAT: Save network state at time step for later recomputation
 * WHY:  Tradeoff memory for computation in adjoint method
 * HOW:  Store copy of state in checkpoint array
 *
 * Memory tradeoff:
 *   No checkpoints: O(1) memory, recompute full forward
 *   Full history: O(T) memory, no recomputation (= BPTT)
 *   Checkpoints every N: O(T/N) memory, recompute N steps
 *
 * @param ctx Gradient context
 * @param step Time step index
 * @param state Network state to save
 * @return 0 on success
 */
int lnn_gradient_save_checkpoint(
    lnn_gradient_ctx_t* ctx,
    uint32_t step,
    const nimcp_tensor_t* state
);

/**
 * @brief Load state checkpoint
 *
 * WHAT: Restore network state from checkpoint
 * WHY:  Resume forward pass for gradient computation
 * HOW:  Copy checkpoint to provided tensor
 *
 * @param ctx Gradient context
 * @param step Time step to load
 * @param state Output tensor (allocated by caller)
 * @return 0 on success
 */
int lnn_gradient_load_checkpoint(
    lnn_gradient_ctx_t* ctx,
    uint32_t step,
    nimcp_tensor_t* state
);

/**
 * @brief Recompute forward pass from checkpoint
 *
 * WHAT: Re-run forward pass between two checkpoints
 * WHY:  Recover intermediate states for adjoint computation
 * HOW:  Load checkpoint, simulate forward from_step to to_step
 *
 * Algorithm:
 * 1. Load checkpoint at from_step
 * 2. Run forward ODE from_step → to_step
 * 3. Return final state
 *
 * @param ctx Gradient context
 * @param network LNN network
 * @param from_step Starting checkpoint
 * @param to_step Target step
 * @return 0 on success
 */
int lnn_gradient_recompute_from_checkpoint(
    lnn_gradient_ctx_t* ctx,
    lnn_network_t* network,
    uint32_t from_step,
    uint32_t to_step
);

/*=============================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Compute gradient norm
 *
 * WHAT: ||∂L/∂θ||₂ L2 norm of parameter gradients
 * WHY:  Monitor gradient health, gradient clipping
 * HOW:  Frobenius norm of gradient tensor
 *
 * @param ctx Gradient context
 * @return Gradient norm
 */
float lnn_gradient_norm(const lnn_gradient_ctx_t* ctx);

/**
 * @brief Get the input gradient from the last adjoint computation
 * @param ctx Gradient context
 * @return Pointer to tensor (owned by ctx, do NOT destroy), or NULL
 */
const nimcp_tensor_t* lnn_gradient_get_input_grad(const lnn_gradient_ctx_t* ctx);

/**
 * @brief Clip gradients by norm
 *
 * WHAT: Scale gradients if ||∂L/∂θ|| > max_norm
 * WHY:  Prevent gradient explosion during training
 * HOW:  If norm > max: grad ← grad * (max_norm / norm)
 *
 * @param ctx Gradient context
 * @param max_norm Maximum allowed gradient norm
 * @return 0 on success
 */
int lnn_gradient_clip(lnn_gradient_ctx_t* ctx, float max_norm);

/**
 * @brief Scale all gradients by a constant factor
 *
 * WHAT: Multiply all parameter gradients by scale factor
 * WHY:  Used for gradient normalization (rescale to target norm)
 * HOW:  grad ← grad * scale for all parameters
 *
 * @param ctx Gradient context
 * @param scale Scale factor to apply
 * @return 0 on success
 */
int lnn_gradient_scale(lnn_gradient_ctx_t* ctx, float scale);

/**
 * @brief Check gradient health
 *
 * WHAT: Detect NaN, Inf, or explosion in gradients
 * WHY:  Early detection prevents training divergence
 * HOW:  Scan gradient tensor for invalid values
 *
 * @param ctx Gradient context
 * @return true if gradients are healthy, false if NaN/Inf/explosion
 */
bool lnn_gradient_check_health(const lnn_gradient_ctx_t* ctx);

/**
 * @brief Get gradient statistics
 *
 * WHAT: Retrieve gradient computation metrics
 * WHY:  Performance profiling and debugging
 * HOW:  Return accumulated statistics
 *
 * @param ctx Gradient context
 * @param adjoint_steps Output: number of adjoint steps
 * @param jacobian_evals Output: Jacobian evaluations
 * @param checkpoints_used Output: checkpoint accesses
 * @param compute_time_ms Output: total computation time
 * @return 0 on success
 */
int lnn_gradient_get_stats(
    const lnn_gradient_ctx_t* ctx,
    uint64_t* adjoint_steps,
    uint64_t* jacobian_evals,
    uint64_t* checkpoints_used,
    double* compute_time_ms
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_GRADIENT_H */
