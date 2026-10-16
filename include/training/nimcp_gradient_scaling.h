/**
 * @file nimcp_gradient_scaling.h
 * @brief Activation Function Gradient Scaling for NIMCP
 *
 * WHAT: Per-layer and per-activation gradient scaling for training stability
 * WHY:  Address vanishing/exploding gradients in deep networks
 * HOW:  Learnable scales, normalized gradients, activation-specific handling
 *
 * FRAMEWORK COMPARISON:
 * - PyTorch: Manual gradient hooks, torch.nn.utils.clip_grad_norm_
 * - JAX: jax.lax.stop_gradient, custom_vjp
 * - TensorFlow: tf.GradientTape, tf.clip_by_norm
 *
 * NIMCP APPROACH:
 * - Integrates with gradient_manager for unified handling
 * - Per-neuron type scaling (biological variation)
 * - Supports SNN surrogate gradient scaling
 *
 * BIOLOGICAL GROUNDING:
 * - Neuromodulation: Dopamine/ACh modulate synaptic plasticity
 * - Homeostatic scaling: Maintain activity in healthy range
 * - Gain modulation: Context-dependent response scaling
 * - Metaplasticity: History-dependent plasticity thresholds
 *
 * INTEGRATION POINTS:
 * - gradient_manager: Gradient transformation pipeline
 * - snn_backprop: Surrogate gradient scaling
 * - brain_factory: Per-region scaling
 * - lnn: Time-constant aware scaling
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#ifndef NIMCP_GRADIENT_SCALING_H
#define NIMCP_GRADIENT_SCALING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/tensor/nimcp_tensor.h"
#include "utils/error/nimcp_error_codes.h"
#include "middleware/training/nimcp_gradient_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define GS_MAX_LAYERS                 256      /**< Maximum layers */
#define GS_DEFAULT_SCALE              1.0f     /**< Default gradient scale */
#define GS_DEFAULT_CLIP_VALUE         1.0f     /**< Default gradient clip value */
#define GS_MIN_SCALE                  1e-6f    /**< Minimum allowed scale */
#define GS_MAX_SCALE                  1e6f     /**< Maximum allowed scale */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Gradient scaling method
 *
 * BIOLOGICAL BASIS:
 * - Normalized: Similar to homeostatic scaling
 * - Adaptive: Similar to metaplasticity
 * - Layer-wise: Models regional specialization
 */
typedef enum {
    GS_METHOD_NONE = 0,              /**< No scaling */
    GS_METHOD_FIXED,                 /**< Fixed scale per layer */
    GS_METHOD_NORMALIZED,            /**< Normalize to unit variance */
    GS_METHOD_ADAPTIVE,              /**< Adaptive based on gradient stats */
    GS_METHOD_LAYER_WISE_LR,         /**< Layer-wise learning rate */
    GS_METHOD_LSUV,                  /**< Layer-Sequential Unit-Variance */
    GS_METHOD_SPECTRAL,              /**< Spectral normalization */
    GS_METHOD_CENTRALIZED,           /**< Centralized gradients */
    GS_METHOD_COUNT
} gs_method_t;

/**
 * @brief Surrogate gradient function for SNN
 *
 * BIOLOGICAL BASIS:
 * - Spike is discrete; need smooth approximation for backprop
 * - Different surrogates model different neuron dynamics
 */
typedef enum {
    GS_SURROGATE_NONE = 0,           /**< No surrogate (non-spiking) */
    GS_SURROGATE_SIGMOID,            /**< Sigmoid surrogate */
    GS_SURROGATE_FAST_SIGMOID,       /**< Fast sigmoid (steeper) */
    GS_SURROGATE_ARCTAN,             /**< Arctangent surrogate */
    GS_SURROGATE_TRIANGLE,           /**< Triangular (piecewise linear) */
    GS_SURROGATE_SUPERSPIKE,         /**< SuperSpike surrogate */
    GS_SURROGATE_MULTI_GAUSSIAN,     /**< Multi-Gaussian (SLAYER) */
    GS_SURROGATE_COUNT
} gs_surrogate_t;

/**
 * @brief Activation function type
 */
typedef enum {
    GS_ACTIVATION_LINEAR = 0,        /**< Linear (identity) */
    GS_ACTIVATION_RELU,              /**< ReLU */
    GS_ACTIVATION_LEAKY_RELU,        /**< Leaky ReLU */
    GS_ACTIVATION_GELU,              /**< GELU */
    GS_ACTIVATION_SWISH,             /**< Swish */
    GS_ACTIVATION_TANH,              /**< Tanh */
    GS_ACTIVATION_SIGMOID,           /**< Sigmoid */
    GS_ACTIVATION_SOFTMAX,           /**< Softmax */
    GS_ACTIVATION_SPIKE,             /**< Spiking (Heaviside) */
    GS_ACTIVATION_LTC,               /**< Liquid Time-Constant */
    GS_ACTIVATION_COUNT
} gs_activation_t;

/**
 * @brief Gradient clipping strategy
 */
typedef enum {
    GS_CLIP_NONE = 0,                /**< No clipping */
    GS_CLIP_VALUE,                   /**< Clip by value */
    GS_CLIP_NORM,                    /**< Clip by L2 norm */
    GS_CLIP_GLOBAL_NORM,             /**< Clip by global norm */
    GS_CLIP_ADAPTIVE,                /**< Adaptive clipping */
    GS_CLIP_COUNT
} gs_clip_strategy_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Per-layer scaling configuration
 */
typedef struct {
    uint32_t layer_id;               /**< Layer identifier */
    const char* layer_name;          /**< Layer name */
    gs_activation_t activation;      /**< Activation function */

    /* Scaling parameters */
    float scale;                     /**< Fixed scale factor */
    bool learn_scale;                /**< Learn scale parameter */
    float scale_lr;                  /**< Learning rate for scale */
    float min_scale;                 /**< Minimum scale */
    float max_scale;                 /**< Maximum scale */

    /* Clipping */
    gs_clip_strategy_t clip_strategy;/**< Clipping strategy */
    float clip_value;                /**< Clip threshold */

    /* Activation-specific */
    float surrogate_beta;            /**< Surrogate gradient sharpness */
    gs_surrogate_t surrogate;        /**< Surrogate type (for spiking) */
} gs_layer_config_t;

/**
 * @brief Adaptive scaling configuration
 *
 * BIOLOGICAL BASIS:
 * - Homeostatic target range ≈ healthy firing rate range
 * - Adaptation rate ≈ plasticity time constants
 */
typedef struct {
    float target_norm;               /**< Target gradient norm */
    float adaptation_rate;           /**< Adaptation speed */
    float smoothing_factor;          /**< EMA smoothing (0.99) */
    bool use_running_stats;          /**< Use running statistics */
    uint32_t warmup_steps;           /**< Steps before adaptation */
} gs_adaptive_config_t;

/**
 * @brief Surrogate gradient configuration (for SNN)
 */
typedef struct {
    gs_surrogate_t default_surrogate;/**< Default surrogate type */
    float default_beta;              /**< Default sharpness parameter */
    bool scale_by_threshold;         /**< Scale by spike threshold */
    bool temporal_scaling;           /**< Scale by time since last spike */
    float temporal_decay;            /**< Decay rate for temporal scaling */
} gs_surrogate_config_t;

/**
 * @brief Layer-wise learning rate configuration
 *
 * REFERENCE: Luo et al. 2019 "Layer-wise Adaptive Learning Rate"
 */
typedef struct {
    float base_lr;                   /**< Base learning rate */
    float* layer_lr_multipliers;     /**< Per-layer LR multipliers */
    uint32_t num_layers;             /**< Number of layers */
    bool decay_with_depth;           /**< LR decays with depth */
    float decay_rate;                /**< Per-layer decay rate */
} gs_layer_lr_config_t;

/**
 * @brief Complete gradient scaling configuration
 */
typedef struct {
    gs_method_t method;              /**< Scaling method */

    /* Layer configurations */
    gs_layer_config_t* layer_configs;/**< Per-layer configs */
    uint32_t num_layers;             /**< Number of configured layers */

    /* Method-specific configs */
    gs_adaptive_config_t adaptive;
    gs_surrogate_config_t surrogate;
    gs_layer_lr_config_t layer_lr;

    /* Global settings */
    gs_clip_strategy_t global_clip;  /**< Global clipping strategy */
    float global_clip_value;         /**< Global clip threshold */
    bool normalize_per_layer;        /**< Per-layer normalization */

    /* Integration */
    bool integrate_gradient_manager; /**< Use gradient_manager */
    bool integrate_snn_backprop;     /**< Integrate with SNN backprop */

    /* Debugging */
    bool verbose;
    bool track_statistics;
} gs_config_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Per-layer gradient statistics
 */
typedef struct {
    uint32_t layer_id;               /**< Layer identifier */

    /* Gradient statistics */
    float grad_norm;                 /**< Current gradient norm */
    float grad_mean;                 /**< Gradient mean */
    float grad_std;                  /**< Gradient std dev */
    float grad_min;                  /**< Minimum gradient */
    float grad_max;                  /**< Maximum gradient */

    /* Scaling statistics */
    float current_scale;             /**< Current scale factor */
    float effective_lr;              /**< Effective learning rate */

    /* Clipping statistics */
    uint64_t clip_count;             /**< Times clipped */
    float avg_clip_ratio;            /**< Average clip ratio */

    /* Running statistics */
    float running_norm_avg;          /**< Running average norm */
    float running_norm_var;          /**< Running norm variance */
} gs_layer_stats_t;

/**
 * @brief Global gradient scaling statistics
 */
typedef struct {
    uint64_t total_steps;            /**< Total training steps */

    /* Global statistics */
    float global_grad_norm;          /**< Global gradient norm */
    float avg_layer_norm;            /**< Average layer norm */
    float norm_variance;             /**< Variance across layers */

    /* Per-layer stats */
    gs_layer_stats_t* layer_stats;   /**< Per-layer statistics */
    uint32_t num_layers;             /**< Number of layers */

    /* Health indicators */
    float vanishing_ratio;           /**< Fraction of vanishing grads */
    float exploding_ratio;           /**< Fraction of exploding grads */
    bool gradient_healthy;           /**< Overall gradient health */

    /* Scaling dynamics */
    float avg_scale;                 /**< Average scale factor */
    float scale_variance;            /**< Scale variance */
} gs_stats_t;

//=============================================================================
// Opaque Types
//=============================================================================

/**
 * @brief Gradient scaling context (opaque)
 */
typedef struct gs_ctx_s gs_ctx_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default gradient scaling configuration
 *
 * DEFAULTS:
 * - Normalized scaling method
 * - Global norm clipping at 1.0
 * - Sigmoid surrogate for SNN
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
int gs_default_config(gs_config_t* config);

/**
 * @brief Get SNN-optimized configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
int gs_snn_config(gs_config_t* config);

/**
 * @brief Create gradient scaling context
 *
 * @param config Gradient scaling configuration
 * @return Context or NULL on failure
 */
gs_ctx_t* gs_create(const gs_config_t* config);

/**
 * @brief Destroy gradient scaling context
 *
 * @param ctx Context to destroy (NULL-safe)
 */
void gs_destroy(gs_ctx_t* ctx);

/**
 * @brief Register layer for gradient scaling
 *
 * @param ctx Gradient scaling context
 * @param layer_config Layer configuration
 * @return Layer index or negative on error
 */
int gs_register_layer(gs_ctx_t* ctx, const gs_layer_config_t* layer_config);

//=============================================================================
// Scaling API
//=============================================================================

/**
 * @brief Scale gradients for layer
 *
 * WHAT: Apply scaling to layer gradients
 * WHY:  Normalize/adapt gradient magnitudes
 * HOW:  Apply layer-specific transformation
 *
 * @param ctx Gradient scaling context
 * @param layer_id Layer identifier
 * @param gradients Gradient array (modified in place)
 * @param count Number of gradients
 * @return Scale factor applied
 */
float gs_scale_layer(
    gs_ctx_t* ctx,
    uint32_t layer_id,
    float* gradients,
    size_t count
);

/**
 * @brief Scale gradient tensor for layer
 *
 * @param ctx Gradient scaling context
 * @param layer_id Layer identifier
 * @param grad_tensor Gradient tensor (modified in place)
 * @return Scale factor applied
 */
float gs_scale_layer_tensor(
    gs_ctx_t* ctx,
    uint32_t layer_id,
    nimcp_tensor_t* grad_tensor
);

/**
 * @brief Scale all layer gradients
 *
 * @param ctx Gradient scaling context
 * @param gradients Gradients per layer [num_layers]
 * @param counts Element counts per layer
 * @return 0 on success, negative on error
 */
int gs_scale_all_layers(
    gs_ctx_t* ctx,
    float** gradients,
    size_t* counts
);

/**
 * @brief Compute adaptive scale for layer
 *
 * @param ctx Gradient scaling context
 * @param layer_id Layer identifier
 * @param gradients Current gradients
 * @param count Number of gradients
 * @return Computed adaptive scale
 */
float gs_compute_adaptive_scale(
    gs_ctx_t* ctx,
    uint32_t layer_id,
    const float* gradients,
    size_t count
);

/**
 * @brief Update running statistics for adaptive scaling
 *
 * @param ctx Gradient scaling context
 * @param layer_id Layer identifier
 * @param grad_norm Current gradient norm
 * @return 0 on success, negative on error
 */
int gs_update_running_stats(
    gs_ctx_t* ctx,
    uint32_t layer_id,
    float grad_norm
);

//=============================================================================
// Surrogate Gradient API (for SNN)
//=============================================================================

/**
 * @brief Apply surrogate gradient for spike function
 *
 * WHAT: Compute smooth gradient through spike threshold
 * WHY:  Backprop through non-differentiable spike
 * HOW:  Replace Heaviside derivative with smooth surrogate
 *
 * @param ctx Gradient scaling context
 * @param membrane_potential Pre-threshold membrane potential
 * @param threshold Spike threshold
 * @param grad_output Upstream gradient
 * @param grad_input Output gradient (same shape as membrane)
 * @param count Number of neurons
 * @param surrogate Surrogate type
 * @param beta Sharpness parameter
 * @return 0 on success, negative on error
 */
int gs_surrogate_gradient(
    gs_ctx_t* ctx,
    const float* membrane_potential,
    float threshold,
    const float* grad_output,
    float* grad_input,
    size_t count,
    gs_surrogate_t surrogate,
    float beta
);

/**
 * @brief Compute surrogate gradient value
 *
 * @param x Input value (membrane - threshold)
 * @param surrogate Surrogate type
 * @param beta Sharpness parameter
 * @return Surrogate gradient at x
 */
float gs_surrogate_value(float x, gs_surrogate_t surrogate, float beta);

//=============================================================================
// Clipping API
//=============================================================================

/**
 * @brief Clip gradients by value
 *
 * @param gradients Gradient array (modified in place)
 * @param count Number of gradients
 * @param max_value Maximum absolute value
 * @return Number of gradients clipped
 */
uint64_t gs_clip_by_value(
    float* gradients,
    size_t count,
    float max_value
);

/**
 * @brief Clip gradients by norm
 *
 * @param gradients Gradient array (modified in place)
 * @param count Number of gradients
 * @param max_norm Maximum gradient norm
 * @return Original norm before clipping
 */
float gs_clip_by_norm(
    float* gradients,
    size_t count,
    float max_norm
);

/**
 * @brief Clip gradients by global norm
 *
 * WHAT: Clip all gradients together by global norm
 * WHY:  Preserve relative gradient magnitudes
 *
 * @param gradients Gradients per layer [num_layers]
 * @param counts Element counts per layer
 * @param num_layers Number of layers
 * @param max_norm Maximum global norm
 * @return Original global norm
 */
float gs_clip_global_norm(
    float** gradients,
    size_t* counts,
    uint32_t num_layers,
    float max_norm
);

/**
 * @brief Adaptive gradient clipping (AGC)
 *
 * REFERENCE: Brock et al. 2021 "High-Performance Large-Scale Image Recognition"
 *
 * @param ctx Gradient scaling context
 * @param gradients Gradients
 * @param params Parameters (for norm ratio)
 * @param count Number of elements
 * @param clip_factor Clipping factor
 * @return 0 on success, negative on error
 */
int gs_clip_adaptive(
    gs_ctx_t* ctx,
    float* gradients,
    const float* params,
    size_t count,
    float clip_factor
);

//=============================================================================
// Layer-wise Learning Rate API
//=============================================================================

/**
 * @brief Get effective learning rate for layer
 *
 * @param ctx Gradient scaling context
 * @param layer_id Layer identifier
 * @param base_lr Base learning rate
 * @return Effective learning rate for layer
 */
float gs_get_layer_lr(
    const gs_ctx_t* ctx,
    uint32_t layer_id,
    float base_lr
);

/**
 * @brief Set layer learning rate multiplier
 *
 * @param ctx Gradient scaling context
 * @param layer_id Layer identifier
 * @param multiplier LR multiplier
 * @return 0 on success, negative on error
 */
int gs_set_layer_lr_multiplier(
    gs_ctx_t* ctx,
    uint32_t layer_id,
    float multiplier
);

/**
 * @brief Compute layer-wise LR with depth decay
 *
 * @param ctx Gradient scaling context
 * @param base_lr Base learning rate
 * @param decay_rate Decay rate per layer
 * @return 0 on success, negative on error
 */
int gs_compute_depth_decay_lr(
    gs_ctx_t* ctx,
    float base_lr,
    float decay_rate
);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Connect to gradient manager
 *
 * @param ctx Gradient scaling context
 * @param grad_manager Gradient manager
 * @return 0 on success, negative on error
 */
int gs_connect_gradient_manager(
    gs_ctx_t* ctx,
    nimcp_gradient_manager_ctx_t* grad_manager
);

/**
 * @brief Connect to SNN backprop module
 *
 * @param ctx Gradient scaling context
 * @param snn_backprop SNN backprop module
 * @return 0 on success, negative on error
 */
int gs_connect_snn_backprop(gs_ctx_t* ctx, void* snn_backprop);

/**
 * @brief Connect to brain factory
 *
 * BIOLOGICAL BASIS:
 * - Per-region gradient scaling
 * - Models regional differences in plasticity
 *
 * @param ctx Gradient scaling context
 * @param brain_factory Brain factory
 * @return 0 on success, negative on error
 */
int gs_connect_brain_factory(gs_ctx_t* ctx, void* brain_factory);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get gradient scaling statistics
 *
 * @param ctx Gradient scaling context
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int gs_get_stats(const gs_ctx_t* ctx, gs_stats_t* stats);

/**
 * @brief Get layer statistics
 *
 * @param ctx Gradient scaling context
 * @param layer_id Layer identifier
 * @param stats Output layer statistics
 * @return 0 on success, negative on error
 */
int gs_get_layer_stats(
    const gs_ctx_t* ctx,
    uint32_t layer_id,
    gs_layer_stats_t* stats
);

/**
 * @brief Reset gradient scaling statistics
 *
 * @param ctx Gradient scaling context
 */
void gs_reset_stats(gs_ctx_t* ctx);

/**
 * @brief Check gradient health
 *
 * @param ctx Gradient scaling context
 * @param gradients Gradients per layer
 * @param counts Element counts per layer
 * @param num_layers Number of layers
 * @return true if gradients are healthy
 */
bool gs_check_health(
    gs_ctx_t* ctx,
    float** gradients,
    size_t* counts,
    uint32_t num_layers
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get method name
 */
const char* gs_method_name(gs_method_t method);

/**
 * @brief Get surrogate name
 */
const char* gs_surrogate_name(gs_surrogate_t surrogate);

/**
 * @brief Get activation name
 */
const char* gs_activation_name(gs_activation_t activation);

/**
 * @brief Get clip strategy name
 */
const char* gs_clip_strategy_name(gs_clip_strategy_t strategy);

/**
 * @brief Validate gradient scaling configuration
 */
int gs_validate_config(const gs_config_t* config);

/**
 * @brief Free statistics structure
 */
void gs_free_stats(gs_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_GRADIENT_SCALING_H */
