/**
 * @file nimcp_optimizers.h
 * @brief Optimizers Module for NIMCP Training Pipeline
 *
 * Implements standard neural network optimizers:
 * - SGD (Stochastic Gradient Descent) with momentum
 * - Adam (Adaptive Moment Estimation)
 * - RMSprop (Root Mean Square Propagation)
 * - AdaGrad (Adaptive Gradient Algorithm)
 * - AdamW (Adam with Weight Decay)
 * - Nadam (Nesterov-accelerated Adam)
 *
 * All optimizers support:
 * - Parameter update step
 * - Learning rate scheduling integration
 * - Weight decay / L2 regularization
 * - Gradient clipping
 * - Security integration via nimcp_security module
 * - Memory pool integration via unified memory manager
 *
 * @note Part of Phase TM-2: Training Pipeline Infrastructure
 */

#ifndef NIMCP_OPTIMIZERS_H
#define NIMCP_OPTIMIZERS_H

#include "utils/validation/nimcp_common.h"
#include "security/nimcp_security_integration.h"
#include "utils/memory/nimcp_unified_memory.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Optimizer Types and Enumerations
 * ============================================================================ */

/**
 * @brief Supported optimizer types
 */
typedef enum nimcp_optimizer_type {
    NIMCP_OPTIMIZER_SGD = 0,      /**< Stochastic Gradient Descent */
    NIMCP_OPTIMIZER_SGD_MOMENTUM, /**< SGD with Momentum */
    NIMCP_OPTIMIZER_NESTEROV,     /**< Nesterov Accelerated Gradient */
    NIMCP_OPTIMIZER_ADAGRAD,      /**< Adaptive Gradient Algorithm */
    NIMCP_OPTIMIZER_RMSPROP,      /**< Root Mean Square Propagation */
    NIMCP_OPTIMIZER_ADAM,         /**< Adaptive Moment Estimation */
    NIMCP_OPTIMIZER_ADAMW,        /**< Adam with Weight Decay */
    NIMCP_OPTIMIZER_NADAM,        /**< Nesterov-accelerated Adam */
    NIMCP_OPTIMIZER_CUSTOM,       /**< User-defined optimizer */
    NIMCP_OPTIMIZER_TYPE_COUNT
} nimcp_optimizer_type_t;

/* ============================================================================
 * Optimizer Configuration Structures
 * ============================================================================ */

/**
 * @brief Configuration for SGD optimizer
 */
typedef struct nimcp_sgd_config {
    float learning_rate;          /**< Initial learning rate */
    float momentum;               /**< Momentum coefficient (0 for vanilla SGD) */
    bool nesterov;                /**< Use Nesterov momentum */
    float dampening;              /**< Dampening for momentum */
    float weight_decay;           /**< L2 regularization coefficient */
} nimcp_sgd_config_t;

/**
 * @brief Configuration for Adam optimizer
 */
typedef struct nimcp_adam_config {
    float learning_rate;          /**< Initial learning rate (default: 0.001) */
    float beta1;                  /**< Exponential decay rate for 1st moment (default: 0.9) */
    float beta2;                  /**< Exponential decay rate for 2nd moment (default: 0.999) */
    float epsilon;                /**< Numerical stability constant (default: 1e-8) */
    float weight_decay;           /**< L2 regularization coefficient */
    bool amsgrad;                 /**< Use AMSGrad variant */
} nimcp_adam_config_t;

/**
 * @brief Configuration for AdamW optimizer
 */
typedef struct nimcp_adamw_config {
    float learning_rate;          /**< Initial learning rate */
    float beta1;                  /**< Exponential decay rate for 1st moment */
    float beta2;                  /**< Exponential decay rate for 2nd moment */
    float epsilon;                /**< Numerical stability constant */
    float weight_decay;           /**< Decoupled weight decay coefficient */
    bool amsgrad;                 /**< Use AMSGrad variant */
} nimcp_adamw_config_t;

/**
 * @brief Configuration for RMSprop optimizer
 */
typedef struct nimcp_rmsprop_config {
    float learning_rate;          /**< Initial learning rate */
    float alpha;                  /**< Smoothing constant (default: 0.99) */
    float epsilon;                /**< Numerical stability constant */
    float weight_decay;           /**< L2 regularization coefficient */
    float momentum;               /**< Momentum coefficient */
    bool centered;                /**< Use centered RMSprop (normalize by variance) */
} nimcp_rmsprop_config_t;

/**
 * @brief Configuration for AdaGrad optimizer
 */
typedef struct nimcp_adagrad_config {
    float learning_rate;          /**< Initial learning rate */
    float lr_decay;               /**< Learning rate decay */
    float weight_decay;           /**< L2 regularization coefficient */
    float initial_accumulator;    /**< Initial value for accumulator */
    float epsilon;                /**< Numerical stability constant */
} nimcp_adagrad_config_t;

/**
 * @brief Custom optimizer step function signature
 */
typedef void (*nimcp_optimizer_step_fn)(
    float* params,
    const float* gradients,
    void* state,
    size_t count,
    void* user_data
);

/**
 * @brief Configuration for custom optimizer
 */
typedef struct nimcp_custom_optimizer_config {
    float learning_rate;           /**< Base learning rate */
    nimcp_optimizer_step_fn step_fn; /**< Custom step function */
    void* user_data;               /**< User context */
    const char* name;              /**< Optimizer name */
    size_t state_size_per_param;   /**< Bytes of state per parameter */
} nimcp_custom_optimizer_config_t;

/**
 * @brief Main optimizer configuration structure
 */
typedef struct nimcp_optimizer_config {
    nimcp_optimizer_type_t type;   /**< Optimizer type */

    union {
        nimcp_sgd_config_t sgd;
        nimcp_adam_config_t adam;
        nimcp_adamw_config_t adamw;
        nimcp_rmsprop_config_t rmsprop;
        nimcp_adagrad_config_t adagrad;
        nimcp_custom_optimizer_config_t custom;
    } params;

    /* Gradient clipping */
    bool clip_gradients;           /**< Enable gradient clipping */
    float gradient_clip_value;     /**< Max gradient magnitude */
    float gradient_clip_norm;      /**< Max gradient norm (0 = disabled) */

    /* Memory management */
    bool use_memory_pool;          /**< Use unified memory manager */
    unified_mem_strategy_t cow_strategy; /**< CoW strategy */

} nimcp_optimizer_config_t;

/**
 * @brief Optimizer context (opaque)
 */
typedef struct nimcp_optimizer_context nimcp_optimizer_context_t;

/**
 * @brief Parameter group for optimizer
 */
typedef struct nimcp_param_group {
    float* params;                 /**< Parameter array */
    float* gradients;              /**< Gradient array */
    size_t count;                  /**< Number of parameters */
    float learning_rate;           /**< Group-specific learning rate (0 = use global) */
    float weight_decay;            /**< Group-specific weight decay (0 = use global) */
} nimcp_param_group_t;

/**
 * @brief Optimizer statistics
 */
typedef struct nimcp_optimizer_stats {
    uint64_t step_count;           /**< Total optimization steps */
    double total_gradient_norm;    /**< Sum of gradient norms */
    double min_gradient_norm;      /**< Minimum gradient norm */
    double max_gradient_norm;      /**< Maximum gradient norm */
    double avg_gradient_norm;      /**< Average gradient norm */
    uint64_t gradient_clips;       /**< Number of gradient clips */
    double total_param_update;     /**< Sum of parameter updates */
    double current_lr;             /**< Current learning rate */
    uint64_t total_compute_time_ns;/**< Total computation time */
    size_t peak_memory_bytes;      /**< Peak memory usage */
    uint64_t gradient_explosions;  /**< Number of skipped updates due to NaN/Inf gradients */
} nimcp_optimizer_stats_t;

/* ============================================================================
 * Default Configuration Constructors
 * ============================================================================ */

/**
 * @brief Get default SGD configuration
 * @param learning_rate Initial learning rate
 * @return Default SGD configuration
 */
nimcp_sgd_config_t nimcp_optimizer_sgd_default(float learning_rate);

/**
 * @brief Get default Adam configuration
 * @param learning_rate Initial learning rate
 * @return Default Adam configuration
 */
nimcp_adam_config_t nimcp_optimizer_adam_default(float learning_rate);

/**
 * @brief Get default RMSprop configuration
 * @param learning_rate Initial learning rate
 * @return Default RMSprop configuration
 */
nimcp_rmsprop_config_t nimcp_optimizer_rmsprop_default(float learning_rate);

/**
 * @brief Get default AdaGrad configuration
 * @param learning_rate Initial learning rate
 * @return Default AdaGrad configuration
 */
nimcp_adagrad_config_t nimcp_optimizer_adagrad_default(float learning_rate);

/**
 * @brief Get default optimizer configuration for specified type
 * @param type Optimizer type
 * @return Default configuration
 */
nimcp_optimizer_config_t nimcp_optimizer_default_config(nimcp_optimizer_type_t type);

/* ============================================================================
 * Core Optimizer Operations
 * ============================================================================ */

/**
 * @brief Create optimizer context
 * @param config Optimizer configuration
 * @param security_ctx Security integration context (optional)
 * @param memory_mgr Memory manager (optional)
 * @return Optimizer context or NULL on error
 */
nimcp_optimizer_context_t* nimcp_optimizer_create(
    const nimcp_optimizer_config_t* config,
    nimcp_sec_integration_t* security_ctx,
    unified_mem_manager_t memory_mgr
);

/**
 * @brief Destroy optimizer context
 * @param ctx Optimizer context to destroy
 */
void nimcp_optimizer_destroy(nimcp_optimizer_context_t* ctx);

/**
 * @brief Initialize optimizer state for parameters
 * @param ctx Optimizer context
 * @param num_params Number of parameters
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_optimizer_init_params(
    nimcp_optimizer_context_t* ctx,
    size_t num_params
);

/**
 * @brief Perform single optimization step
 * @param ctx Optimizer context
 * @param params Parameter array (modified in place)
 * @param gradients Gradient array
 * @param count Number of parameters
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_optimizer_step(
    nimcp_optimizer_context_t* ctx,
    float* params,
    const float* gradients,
    size_t count
);

/**
 * @brief Perform optimization step on parameter group
 * @param ctx Optimizer context
 * @param group Parameter group
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_optimizer_step_group(
    nimcp_optimizer_context_t* ctx,
    nimcp_param_group_t* group
);

/**
 * @brief Zero all gradients in parameter group
 * @param group Parameter group
 */
void nimcp_optimizer_zero_grad(nimcp_param_group_t* group);

/**
 * @brief Get current learning rate
 * @param ctx Optimizer context
 * @return Current learning rate
 */
float nimcp_optimizer_get_lr(const nimcp_optimizer_context_t* ctx);

/**
 * @brief Set learning rate
 * @param ctx Optimizer context
 * @param lr New learning rate
 */
void nimcp_optimizer_set_lr(nimcp_optimizer_context_t* ctx, float lr);

/**
 * @brief Get current step count
 * @param ctx Optimizer context
 * @return Current step count
 */
uint64_t nimcp_optimizer_get_step(const nimcp_optimizer_context_t* ctx);

/**
 * @brief Reset optimizer state (momentum, etc.)
 * @param ctx Optimizer context
 */
void nimcp_optimizer_reset_state(nimcp_optimizer_context_t* ctx);

/* ============================================================================
 * Statistics and Diagnostics
 * ============================================================================ */

/**
 * @brief Get optimizer statistics
 * @param ctx Optimizer context
 * @param stats Output statistics structure
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_optimizer_get_stats(
    const nimcp_optimizer_context_t* ctx,
    nimcp_optimizer_stats_t* stats
);

/**
 * @brief Reset optimizer statistics
 * @param ctx Optimizer context
 */
void nimcp_optimizer_reset_stats(nimcp_optimizer_context_t* ctx);

/**
 * @brief Get optimizer type name
 * @param type Optimizer type
 * @return String name of the optimizer
 */
const char* nimcp_optimizer_type_name(nimcp_optimizer_type_t type);

/**
 * @brief Validate optimizer configuration
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS if valid
 */
nimcp_result_t nimcp_optimizer_validate_config(const nimcp_optimizer_config_t* config);

/**
 * @brief Check if optimizer is registered with security
 * @param ctx Optimizer context
 * @return true if registered
 */
bool nimcp_optimizer_is_registered(const nimcp_optimizer_context_t* ctx);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Compute gradient norm
 * @param gradients Gradient array
 * @param count Number of gradients
 * @return L2 norm of gradients
 */
float nimcp_optimizer_gradient_norm(const float* gradients, size_t count);

/**
 * @brief Clip gradients by value
 * @param gradients Gradient array (modified in place)
 * @param count Number of gradients
 * @param max_value Maximum absolute value
 * @return Number of gradients clipped
 */
size_t nimcp_optimizer_clip_by_value(
    float* gradients,
    size_t count,
    float max_value
);

/**
 * @brief Clip gradients by norm
 * @param gradients Gradient array (modified in place)
 * @param count Number of gradients
 * @param max_norm Maximum gradient norm
 * @return Original norm before clipping
 */
float nimcp_optimizer_clip_by_norm(
    float* gradients,
    size_t count,
    float max_norm
);

/* ============================================================================
 * Tensor-Based Operations (Phase TENSOR-2)
 * ============================================================================ */

/* Forward declaration for tensor type */
#ifndef NIMCP_TENSOR_T_DEFINED
#define NIMCP_TENSOR_T_DEFINED
struct nimcp_tensor_s;
typedef struct nimcp_tensor_s nimcp_tensor_t;
#endif

/**
 * @brief Perform optimization step on tensor parameters
 *
 * WHAT: Update parameters using gradients with tensor operations
 * WHY:  Efficient vectorized parameter updates
 * HOW:  Extract data from tensors and apply optimizer step
 *
 * @param ctx Optimizer context
 * @param params Parameter tensor (modified in place)
 * @param gradients Gradient tensor
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t nimcp_optimizer_step_tensor(
    nimcp_optimizer_context_t* ctx,
    nimcp_tensor_t* params,
    const nimcp_tensor_t* gradients
);

/**
 * @brief Compute gradient norm from tensor
 *
 * WHAT: Calculate L2 norm of gradient tensor
 * WHY:  Efficient norm computation for gradient monitoring
 * HOW:  Use tensor library's vectorized norm function
 *
 * @param gradients Gradient tensor
 * @return L2 norm of gradients
 */
float nimcp_optimizer_gradient_norm_tensor(const nimcp_tensor_t* gradients);

/**
 * @brief Clip gradient tensor by norm
 *
 * WHAT: Scale gradient tensor if norm exceeds threshold
 * WHY:  Prevent exploding gradients with tensor efficiency
 * HOW:  Compute norm and scale in-place if needed
 *
 * @param gradients Gradient tensor (modified in place)
 * @param max_norm Maximum gradient norm
 * @return Original norm before clipping
 */
float nimcp_optimizer_clip_by_norm_tensor(
    nimcp_tensor_t* gradients,
    float max_norm
);

/* ============================================================================
 * Optimizer State Persistence
 * ============================================================================ */

/**
 * @brief Save optimizer state to file
 *
 * WHAT: Serialize optimizer momentum/velocity buffers and step count
 * WHY:  Resume training without loss of optimizer state (Adam m/v, etc.)
 * HOW:  Write type, step count, buffer sizes, and raw float arrays
 *
 * @param ctx Optimizer context
 * @param file Open file handle (binary write mode)
 * @return 0 on success, -1 on error
 */
int nimcp_optimizer_save(const nimcp_optimizer_context_t* ctx, FILE* file);

/**
 * @brief Load optimizer state from file
 *
 * WHAT: Restore optimizer momentum/velocity buffers and step count
 * WHY:  Continue training from checkpoint with warm optimizer state
 * HOW:  Read type, step count, buffer sizes, and raw float arrays
 *
 * @param ctx Optimizer context (must already be created with matching type)
 * @param file Open file handle (binary read mode)
 * @return 0 on success, -1 on error or type mismatch
 */
int nimcp_optimizer_load(nimcp_optimizer_context_t* ctx, FILE* file);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OPTIMIZERS_H */
