/**
 * @file nimcp_regularization.h
 * @brief Regularization Module for NIMCP Training Pipeline
 *
 * Implements standard regularization techniques:
 * - L1 Regularization (Lasso): Promotes sparsity
 * - L2 Regularization (Ridge/Weight Decay): Prevents large weights
 * - Elastic Net: Combination of L1 and L2
 * - Dropout: Random neuron deactivation during training
 * - Gradient Clipping: By value or by norm
 * - Label Smoothing: Softens hard labels
 * - Early Stopping: Training termination based on validation loss
 *
 * All regularizers integrate with:
 * - nimcp_optimizer for weight updates
 * - Security integration via nimcp_security module
 *
 * @note Part of Phase TM-5: Regularization
 * @version 1.0.0
 * @author NIMCP Development Team
 */

#ifndef NIMCP_REGULARIZATION_H
#define NIMCP_REGULARIZATION_H

#include "utils/validation/nimcp_common.h"
#include "security/nimcp_security_integration.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants and Limits
 * ============================================================================ */

#define NIMCP_REG_MAX_LAYERS 256          /**< Maximum layers for per-layer config */
#define NIMCP_REG_MAX_PATIENCE 1000       /**< Maximum early stopping patience */
#define NIMCP_DROPOUT_MIN 0.0f            /**< Minimum dropout rate */
#define NIMCP_DROPOUT_MAX 0.99f           /**< Maximum dropout rate */

/* ============================================================================
 * Regularization Types and Enumerations
 * ============================================================================ */

/**
 * @brief Weight regularization types
 */
typedef enum nimcp_reg_type {
    NIMCP_REG_NONE = 0,           /**< No regularization */
    NIMCP_REG_L1,                 /**< L1 regularization (Lasso) */
    NIMCP_REG_L2,                 /**< L2 regularization (Ridge/Weight Decay) */
    NIMCP_REG_ELASTIC_NET,        /**< Elastic Net (L1 + L2) */
    NIMCP_REG_TYPE_COUNT
} nimcp_reg_type_t;

/**
 * @brief Gradient clipping modes
 */
typedef enum nimcp_clip_mode {
    NIMCP_CLIP_NONE = 0,          /**< No gradient clipping */
    NIMCP_CLIP_BY_VALUE,          /**< Clip each gradient component to [-value, value] */
    NIMCP_CLIP_BY_NORM,           /**< Clip gradient if L2 norm exceeds threshold */
    NIMCP_CLIP_BY_GLOBAL_NORM,    /**< Clip all gradients by global L2 norm */
    NIMCP_CLIP_MODE_COUNT
} nimcp_clip_mode_t;

/**
 * @brief Dropout modes
 */
typedef enum nimcp_dropout_mode {
    NIMCP_DROPOUT_STANDARD = 0,   /**< Standard dropout with scaling */
    NIMCP_DROPOUT_ALPHA,          /**< Alpha dropout for SELU networks */
    NIMCP_DROPOUT_SPATIAL,        /**< Spatial dropout for CNNs (drop entire channels) */
    NIMCP_DROPOUT_MODE_COUNT
} nimcp_dropout_mode_t;

/**
 * @brief Early stopping improvement modes
 */
typedef enum nimcp_early_stop_mode {
    NIMCP_EARLY_STOP_MIN = 0,     /**< Stop when metric stops decreasing */
    NIMCP_EARLY_STOP_MAX          /**< Stop when metric stops increasing */
} nimcp_early_stop_mode_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief L1 regularization configuration
 *
 * Adds lambda * |w| to loss, promotes sparsity
 */
typedef struct nimcp_l1_config {
    float lambda;                 /**< Regularization strength (default: 0.01) */
} nimcp_l1_config_t;

/**
 * @brief L2 regularization configuration
 *
 * Adds 0.5 * lambda * w^2 to loss, prevents large weights
 */
typedef struct nimcp_l2_config {
    float lambda;                 /**< Regularization strength (default: 0.01) */
} nimcp_l2_config_t;

/**
 * @brief Elastic Net configuration
 *
 * Combines L1 and L2: alpha * L1 + (1-alpha) * L2
 */
typedef struct nimcp_elastic_net_config {
    float lambda;                 /**< Total regularization strength */
    float alpha;                  /**< L1 ratio [0,1] (0=pure L2, 1=pure L1) */
} nimcp_elastic_net_config_t;

/**
 * @brief Dropout configuration
 */
typedef struct nimcp_dropout_config {
    float rate;                   /**< Dropout probability [0,1) */
    nimcp_dropout_mode_t mode;    /**< Dropout mode */
    bool training;                /**< Whether in training mode */
    uint64_t seed;                /**< Random seed (0 for random) */
} nimcp_dropout_config_t;

/**
 * @brief Gradient clipping configuration
 */
typedef struct nimcp_clip_config {
    nimcp_clip_mode_t mode;       /**< Clipping mode */
    float threshold;              /**< Clipping threshold */
} nimcp_clip_config_t;

/**
 * @brief Label smoothing configuration
 */
typedef struct nimcp_label_smooth_config {
    float smoothing;              /**< Smoothing factor [0,1] (default: 0.1) */
    uint32_t num_classes;         /**< Number of classes */
} nimcp_label_smooth_config_t;

/**
 * @brief Early stopping configuration
 */
typedef struct nimcp_early_stop_config {
    uint32_t patience;            /**< Epochs to wait for improvement */
    float min_delta;              /**< Minimum change to qualify as improvement */
    nimcp_early_stop_mode_t mode; /**< Improvement mode (min/max) */
    bool restore_best;            /**< Restore best weights when stopped */
} nimcp_early_stop_config_t;

/**
 * @brief Main regularization configuration
 */
typedef struct nimcp_regularization_config {
    /* Weight regularization */
    nimcp_reg_type_t weight_reg_type;
    union {
        nimcp_l1_config_t l1;
        nimcp_l2_config_t l2;
        nimcp_elastic_net_config_t elastic_net;
    } weight_reg;

    /* Gradient clipping */
    nimcp_clip_config_t gradient_clip;

    /* Dropout */
    nimcp_dropout_config_t dropout;

    /* Label smoothing */
    nimcp_label_smooth_config_t label_smooth;
    bool use_label_smoothing;

    /* Early stopping */
    nimcp_early_stop_config_t early_stop;
    bool use_early_stopping;

    /* Common options */
    bool verbose;

    /* Security integration */
    nimcp_sec_integration_t* security_ctx; /**< Security context (optional) */
} nimcp_regularization_config_t;

/* ============================================================================
 * Opaque Context Types
 * ============================================================================ */

/**
 * @brief Opaque regularization context
 */
typedef struct nimcp_regularization_ctx nimcp_regularization_ctx_t;

/**
 * @brief Opaque dropout context (for per-layer dropout)
 */
typedef struct nimcp_dropout_ctx nimcp_dropout_ctx_t;

/**
 * @brief Opaque early stopping context
 */
typedef struct nimcp_early_stop_ctx nimcp_early_stop_ctx_t;

/* ============================================================================
 * Statistics Structure
 * ============================================================================ */

/**
 * @brief Regularization statistics
 */
typedef struct nimcp_regularization_stats {
    /* Weight regularization stats */
    uint64_t weight_reg_count;    /**< Number of weight regularization calls */
    float total_reg_loss;         /**< Cumulative regularization loss */
    float last_reg_loss;          /**< Last regularization loss value */

    /* Gradient clipping stats */
    uint64_t clip_count;          /**< Number of gradient clipping events */
    float total_clip_ratio;       /**< Sum of clip ratios (for averaging) */
    float max_grad_norm;          /**< Maximum gradient norm seen */

    /* Dropout stats */
    uint64_t dropout_count;       /**< Number of dropout applications */
    float avg_dropout_rate;       /**< Average effective dropout rate */

    /* Early stopping stats */
    uint32_t epochs_since_improve; /**< Epochs since last improvement */
    float best_metric;            /**< Best metric value */
    uint64_t best_epoch;          /**< Epoch of best metric */
} nimcp_regularization_stats_t;

/* ============================================================================
 * Default Configurations
 * ============================================================================ */

/**
 * @brief Get default L1 regularization configuration
 * @param lambda Regularization strength
 * @return Default configuration
 */
nimcp_l1_config_t nimcp_l1_default_config(float lambda);

/**
 * @brief Get default L2 regularization configuration
 * @param lambda Regularization strength
 * @return Default configuration
 */
nimcp_l2_config_t nimcp_l2_default_config(float lambda);

/**
 * @brief Get default Elastic Net configuration
 * @param lambda Total regularization strength
 * @param alpha L1 ratio [0,1]
 * @return Default configuration
 */
nimcp_elastic_net_config_t nimcp_elastic_net_default_config(float lambda, float alpha);

/**
 * @brief Get default dropout configuration
 * @param rate Dropout rate [0,1)
 * @return Default configuration
 */
nimcp_dropout_config_t nimcp_dropout_default_config(float rate);

/**
 * @brief Get default gradient clipping configuration
 * @param mode Clipping mode
 * @param threshold Clipping threshold
 * @return Default configuration
 */
nimcp_clip_config_t nimcp_clip_default_config(nimcp_clip_mode_t mode, float threshold);

/**
 * @brief Get default label smoothing configuration
 * @param smoothing Smoothing factor
 * @param num_classes Number of classes
 * @return Default configuration
 */
nimcp_label_smooth_config_t nimcp_label_smooth_default_config(float smoothing, uint32_t num_classes);

/**
 * @brief Get default early stopping configuration
 * @param patience Epochs to wait for improvement
 * @return Default configuration
 */
nimcp_early_stop_config_t nimcp_early_stop_default_config(uint32_t patience);

/**
 * @brief Get default regularization configuration (all disabled)
 * @return Default configuration with no regularization
 */
nimcp_regularization_config_t nimcp_regularization_default_config(void);

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Create a regularization context
 * @param config Regularization configuration
 * @return Regularization context or NULL on failure
 */
nimcp_regularization_ctx_t* nimcp_regularization_create(
    const nimcp_regularization_config_t* config
);

/**
 * @brief Destroy a regularization context
 * @param ctx Regularization context
 */
void nimcp_regularization_destroy(nimcp_regularization_ctx_t* ctx);

/* ============================================================================
 * Weight Regularization Operations
 * ============================================================================ */

/**
 * @brief Compute regularization loss for weights
 * @param ctx Regularization context
 * @param weights Weight array
 * @param num_weights Number of weights
 * @return Regularization loss value
 */
float nimcp_regularization_loss(
    nimcp_regularization_ctx_t* ctx,
    const float* weights,
    size_t num_weights
);

/**
 * @brief Compute L1 regularization loss
 * @param weights Weight array
 * @param num_weights Number of weights
 * @param lambda Regularization strength
 * @return L1 loss value
 */
float nimcp_l1_loss(const float* weights, size_t num_weights, float lambda);

/**
 * @brief Compute L2 regularization loss
 * @param weights Weight array
 * @param num_weights Number of weights
 * @param lambda Regularization strength
 * @return L2 loss value
 */
float nimcp_l2_loss(const float* weights, size_t num_weights, float lambda);

/**
 * @brief Compute Elastic Net regularization loss
 * @param weights Weight array
 * @param num_weights Number of weights
 * @param lambda Total regularization strength
 * @param alpha L1 ratio [0,1]
 * @return Elastic net loss value
 */
float nimcp_elastic_net_loss(
    const float* weights,
    size_t num_weights,
    float lambda,
    float alpha
);

/**
 * @brief Apply regularization gradient to weight gradients
 * @param ctx Regularization context
 * @param weights Weight array
 * @param gradients Gradient array (modified in place)
 * @param num_weights Number of weights
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_regularization_apply_gradient(
    nimcp_regularization_ctx_t* ctx,
    const float* weights,
    float* gradients,
    size_t num_weights
);

/**
 * @brief Apply L1 gradient contribution
 * @param weights Weight array
 * @param gradients Gradient array (modified in place)
 * @param num_weights Number of weights
 * @param lambda Regularization strength
 */
void nimcp_l1_gradient(
    const float* weights,
    float* gradients,
    size_t num_weights,
    float lambda
);

/**
 * @brief Apply L2 gradient contribution
 * @param weights Weight array
 * @param gradients Gradient array (modified in place)
 * @param num_weights Number of weights
 * @param lambda Regularization strength
 */
void nimcp_l2_gradient(
    const float* weights,
    float* gradients,
    size_t num_weights,
    float lambda
);

/* ============================================================================
 * Gradient Clipping Operations
 * ============================================================================ */

/**
 * @brief Clip gradients using context configuration
 * @param ctx Regularization context
 * @param gradients Gradient array (modified in place)
 * @param num_gradients Number of gradients
 * @return Clip ratio (1.0 if no clipping, <1.0 if clipped)
 */
float nimcp_gradient_clip(
    nimcp_regularization_ctx_t* ctx,
    float* gradients,
    size_t num_gradients
);

/**
 * @brief Clip gradients by value
 * @param gradients Gradient array (modified in place)
 * @param num_gradients Number of gradients
 * @param threshold Clipping threshold [-threshold, threshold]
 * @return Number of clipped values
 */
uint64_t nimcp_gradient_clip_by_value(
    float* gradients,
    size_t num_gradients,
    float threshold
);

/**
 * @brief Clip gradients by L2 norm
 * @param gradients Gradient array (modified in place)
 * @param num_gradients Number of gradients
 * @param max_norm Maximum L2 norm
 * @return Clip ratio (actual_norm / max_norm if clipped, 1.0 otherwise)
 */
float nimcp_gradient_clip_by_norm(
    float* gradients,
    size_t num_gradients,
    float max_norm
);

/**
 * @brief Compute gradient L2 norm
 * @param gradients Gradient array
 * @param num_gradients Number of gradients
 * @return L2 norm
 */
float nimcp_gradient_norm(const float* gradients, size_t num_gradients);

/* ============================================================================
 * Dropout Operations
 * ============================================================================ */

/**
 * @brief Create a dropout context
 * @param config Dropout configuration
 * @return Dropout context or NULL on failure
 */
nimcp_dropout_ctx_t* nimcp_dropout_create(const nimcp_dropout_config_t* config);

/**
 * @brief Destroy a dropout context
 * @param ctx Dropout context
 */
void nimcp_dropout_destroy(nimcp_dropout_ctx_t* ctx);

/**
 * @brief Apply dropout to activations
 * @param ctx Dropout context
 * @param activations Activation array (modified in place)
 * @param num_activations Number of activations
 * @param mask Output mask array (optional, NULL to skip)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_dropout_forward(
    nimcp_dropout_ctx_t* ctx,
    float* activations,
    size_t num_activations,
    uint8_t* mask
);

/**
 * @brief Apply dropout backward pass (using saved mask)
 * @param ctx Dropout context
 * @param gradients Gradient array (modified in place)
 * @param num_gradients Number of gradients
 * @param mask Mask from forward pass
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_dropout_backward(
    nimcp_dropout_ctx_t* ctx,
    float* gradients,
    size_t num_gradients,
    const uint8_t* mask
);

/**
 * @brief Set dropout training mode
 * @param ctx Dropout context
 * @param training True for training mode, false for inference
 */
void nimcp_dropout_set_training(nimcp_dropout_ctx_t* ctx, bool training);

/**
 * @brief Get current dropout rate
 * @param ctx Dropout context
 * @return Dropout rate
 */
float nimcp_dropout_get_rate(const nimcp_dropout_ctx_t* ctx);

/**
 * @brief Set dropout rate
 * @param ctx Dropout context
 * @param rate New dropout rate [0,1)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_dropout_set_rate(nimcp_dropout_ctx_t* ctx, float rate);

/* ============================================================================
 * Label Smoothing Operations
 * ============================================================================ */

/**
 * @brief Apply label smoothing to one-hot labels
 * @param labels One-hot label array (modified in place)
 * @param num_samples Number of samples
 * @param num_classes Number of classes
 * @param smoothing Smoothing factor [0,1]
 */
void nimcp_label_smooth(
    float* labels,
    size_t num_samples,
    uint32_t num_classes,
    float smoothing
);

/**
 * @brief Smooth a single class label
 * @param true_class True class index
 * @param num_classes Total number of classes
 * @param smoothing Smoothing factor
 * @param output Output probability array (size num_classes)
 */
void nimcp_label_smooth_single(
    uint32_t true_class,
    uint32_t num_classes,
    float smoothing,
    float* output
);

/* ============================================================================
 * Early Stopping Operations
 * ============================================================================ */

/**
 * @brief Create an early stopping context
 * @param config Early stopping configuration
 * @return Early stopping context or NULL on failure
 */
nimcp_early_stop_ctx_t* nimcp_early_stop_create(
    const nimcp_early_stop_config_t* config
);

/**
 * @brief Destroy an early stopping context
 * @param ctx Early stopping context
 */
void nimcp_early_stop_destroy(nimcp_early_stop_ctx_t* ctx);

/**
 * @brief Update early stopping with current metric
 * @param ctx Early stopping context
 * @param metric Current validation metric
 * @return True if training should stop, false otherwise
 */
bool nimcp_early_stop_check(nimcp_early_stop_ctx_t* ctx, float metric);

/**
 * @brief Reset early stopping state
 * @param ctx Early stopping context
 */
void nimcp_early_stop_reset(nimcp_early_stop_ctx_t* ctx);

/**
 * @brief Get best metric value
 * @param ctx Early stopping context
 * @return Best metric value
 */
float nimcp_early_stop_get_best(const nimcp_early_stop_ctx_t* ctx);

/**
 * @brief Get epoch of best metric
 * @param ctx Early stopping context
 * @return Best epoch number
 */
uint64_t nimcp_early_stop_get_best_epoch(const nimcp_early_stop_ctx_t* ctx);

/**
 * @brief Check if metric improved
 * @param ctx Early stopping context
 * @return True if last check showed improvement
 */
bool nimcp_early_stop_improved(const nimcp_early_stop_ctx_t* ctx);

/* ============================================================================
 * Statistics and Monitoring
 * ============================================================================ */

/**
 * @brief Get regularization statistics
 * @param ctx Regularization context
 * @param stats Output statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t nimcp_regularization_get_stats(
    const nimcp_regularization_ctx_t* ctx,
    nimcp_regularization_stats_t* stats
);

/**
 * @brief Reset regularization statistics
 * @param ctx Regularization context
 */
void nimcp_regularization_reset_stats(nimcp_regularization_ctx_t* ctx);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get regularization type name
 * @param type Regularization type
 * @return String name
 */
const char* nimcp_reg_type_name(nimcp_reg_type_t type);

/**
 * @brief Get clip mode name
 * @param mode Clip mode
 * @return String name
 */
const char* nimcp_clip_mode_name(nimcp_clip_mode_t mode);

/**
 * @brief Validate regularization configuration
 * @param config Configuration to validate
 * @return NIMCP_SUCCESS if valid
 */
nimcp_result_t nimcp_regularization_validate_config(
    const nimcp_regularization_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_REGULARIZATION_H */
