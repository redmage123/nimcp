/**
 * @file nimcp_loss_functions.h
 * @brief Loss Functions Module for NIMCP Training Pipeline
 *
 * Implements standard loss functions for neural network training:
 * - Mean Squared Error (MSE)
 * - Cross-Entropy Loss (binary and categorical)
 * - KL Divergence
 * - Huber Loss (smooth L1)
 * - Custom loss function support
 *
 * All loss functions support:
 * - Forward computation (loss value)
 * - Backward computation (gradients)
 * - Reduction modes (mean, sum, none)
 * - Security integration via nimcp_security module
 * - Memory pool integration via unified memory manager
 *
 * @note Part of Phase TM-1: Training Module Infrastructure
 */

#ifndef NIMCP_LOSS_FUNCTIONS_H
#define NIMCP_LOSS_FUNCTIONS_H

#include "utils/validation/nimcp_common.h"
#include "security/nimcp_security_integration.h"
#include "utils/memory/nimcp_unified_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Loss Function Types and Enumerations
 * ============================================================================ */

/**
 * @brief Supported loss function types
 */
typedef enum nimcp_loss_type {
    NIMCP_LOSS_MSE = 0,              /**< Mean Squared Error */
    NIMCP_LOSS_MAE,                   /**< Mean Absolute Error */
    NIMCP_LOSS_CROSS_ENTROPY,         /**< Cross-Entropy (softmax) */
    NIMCP_LOSS_BINARY_CROSS_ENTROPY,  /**< Binary Cross-Entropy (sigmoid) */
    NIMCP_LOSS_KL_DIVERGENCE,         /**< Kullback-Leibler Divergence */
    NIMCP_LOSS_HUBER,                 /**< Huber Loss (smooth L1) */
    NIMCP_LOSS_HINGE,                 /**< Hinge Loss (SVM) */
    NIMCP_LOSS_LOG_COSH,              /**< Log-Cosh Loss */
    NIMCP_LOSS_FOCAL,                 /**< Focal Loss (class imbalance) */
    NIMCP_LOSS_CONTRASTIVE,           /**< Contrastive Loss (siamese) */
    NIMCP_LOSS_TRIPLET,               /**< Triplet Loss (metric learning) */
    NIMCP_LOSS_CUSTOM,                /**< User-defined loss function */
    NIMCP_LOSS_TYPE_COUNT
} nimcp_loss_type_t;

/**
 * @brief Reduction mode for loss computation
 */
typedef enum nimcp_loss_reduction {
    NIMCP_LOSS_REDUCE_MEAN = 0,  /**< Average loss over batch */
    NIMCP_LOSS_REDUCE_SUM,       /**< Sum loss over batch */
    NIMCP_LOSS_REDUCE_NONE       /**< Return per-sample losses */
} nimcp_loss_reduction_t;

/* ============================================================================
 * Loss Function Structures
 * ============================================================================ */

/**
 * @brief Loss computation result
 */
typedef struct nimcp_loss_result {
    float loss_value;           /**< Computed loss value */
    float* gradients;           /**< Gradient array (if computed) */
    size_t gradient_count;      /**< Number of gradients */
    float* per_sample_loss;     /**< Per-sample losses (REDUCE_NONE) */
    size_t sample_count;        /**< Number of samples */
    uint64_t compute_time_ns;   /**< Computation time in nanoseconds */
} nimcp_loss_result_t;

/**
 * @brief Configuration for MSE loss
 */
typedef struct nimcp_mse_config {
    nimcp_loss_reduction_t reduction;  /**< Reduction mode */
    bool compute_gradient;              /**< Whether to compute gradients */
} nimcp_mse_config_t;

/**
 * @brief Configuration for cross-entropy loss
 */
typedef struct nimcp_cross_entropy_config {
    nimcp_loss_reduction_t reduction;  /**< Reduction mode */
    bool compute_gradient;              /**< Whether to compute gradients */
    float* class_weights;               /**< Per-class weights (optional) */
    size_t num_classes;                 /**< Number of classes */
    float label_smoothing;              /**< Label smoothing factor [0,1) */
    int ignore_index;                   /**< Class index to ignore (-1 = none) */
} nimcp_cross_entropy_config_t;

/**
 * @brief Configuration for KL divergence
 */
typedef struct nimcp_kl_config {
    nimcp_loss_reduction_t reduction;  /**< Reduction mode */
    bool compute_gradient;              /**< Whether to compute gradients */
    bool log_target;                    /**< Is target already log probabilities */
} nimcp_kl_config_t;

/**
 * @brief Configuration for Huber loss
 */
typedef struct nimcp_huber_config {
    nimcp_loss_reduction_t reduction;  /**< Reduction mode */
    bool compute_gradient;              /**< Whether to compute gradients */
    float delta;                        /**< Threshold for L1/L2 transition */
} nimcp_huber_config_t;

/**
 * @brief Configuration for focal loss
 */
typedef struct nimcp_focal_config {
    nimcp_loss_reduction_t reduction;  /**< Reduction mode */
    bool compute_gradient;              /**< Whether to compute gradients */
    float gamma;                        /**< Focusing parameter (default 2.0) */
    float alpha;                        /**< Class balance weight */
} nimcp_focal_config_t;

/**
 * @brief Configuration for contrastive loss
 */
typedef struct nimcp_contrastive_config {
    nimcp_loss_reduction_t reduction;  /**< Reduction mode */
    bool compute_gradient;              /**< Whether to compute gradients */
    float margin;                       /**< Margin for negative pairs */
} nimcp_contrastive_config_t;

/**
 * @brief Configuration for triplet loss
 */
typedef struct nimcp_triplet_config {
    nimcp_loss_reduction_t reduction;  /**< Reduction mode */
    bool compute_gradient;              /**< Whether to compute gradients */
    float margin;                       /**< Margin between pos/neg distances */
    bool swap;                          /**< Use distance swap heuristic */
} nimcp_triplet_config_t;

/**
 * @brief Custom loss function callback types
 */
typedef float (*nimcp_loss_forward_fn)(
    const float* predictions,
    const float* targets,
    size_t count,
    void* user_data
);

typedef void (*nimcp_loss_backward_fn)(
    const float* predictions,
    const float* targets,
    float* gradients,
    size_t count,
    void* user_data
);

/**
 * @brief Configuration for custom loss function
 */
typedef struct nimcp_custom_loss_config {
    nimcp_loss_reduction_t reduction;   /**< Reduction mode */
    nimcp_loss_forward_fn forward_fn;   /**< Forward computation callback */
    nimcp_loss_backward_fn backward_fn; /**< Backward computation callback */
    void* user_data;                    /**< User data for callbacks */
    const char* name;                   /**< Custom loss name */
} nimcp_custom_loss_config_t;

/**
 * @brief Unified loss function configuration
 */
typedef struct nimcp_loss_config {
    nimcp_loss_type_t type;            /**< Loss function type */
    union {
        nimcp_mse_config_t mse;
        nimcp_cross_entropy_config_t cross_entropy;
        nimcp_kl_config_t kl;
        nimcp_huber_config_t huber;
        nimcp_focal_config_t focal;
        nimcp_contrastive_config_t contrastive;
        nimcp_triplet_config_t triplet;
        nimcp_custom_loss_config_t custom;
    } params;

    /* Memory management */
    bool use_memory_pool;               /**< Use unified memory manager */
    unified_mem_strategy_t cow_strategy; /**< CoW strategy */

    /* Numerical stability */
    float epsilon;                      /**< Small value for numerical stability */
    bool clip_gradients;                /**< Clip gradients to prevent explosion */
    float gradient_clip_value;          /**< Maximum gradient magnitude */
} nimcp_loss_config_t;

/**
 * @brief Loss function context (opaque)
 */
/* Forward declarations for opaque pointer types */
typedef struct nimcp_loss_context nimcp_loss_context_t;

/**
 * @brief Loss function statistics
 */
typedef struct nimcp_loss_stats {
    uint64_t forward_count;        /**< Number of forward passes */
    uint64_t backward_count;       /**< Number of backward passes */
    double total_loss;             /**< Cumulative loss value */
    double min_loss;               /**< Minimum loss observed */
    double max_loss;               /**< Maximum loss observed */
    double avg_loss;               /**< Running average loss */
    double loss_variance;          /**< Loss variance (online computation) */
    uint64_t total_compute_time_ns;/**< Total computation time */
    uint64_t gradient_clips;       /**< Number of gradient clips applied */
    size_t peak_memory_bytes;      /**< Peak memory usage */
} nimcp_loss_stats_t;

/* ============================================================================
 * Default Configuration Constructors
 * ============================================================================ */

/**
 * @brief Get default MSE loss configuration
 * @return Default MSE configuration
 */
nimcp_mse_config_t nimcp_loss_mse_default_config(void);

/**
 * @brief Get default cross-entropy loss configuration
 * @param num_classes Number of output classes
 * @return Default cross-entropy configuration
 */
nimcp_cross_entropy_config_t nimcp_loss_cross_entropy_default_config(size_t num_classes);

/**
 * @brief Get default KL divergence configuration
 * @return Default KL configuration
 */
nimcp_kl_config_t nimcp_loss_kl_default_config(void);

/**
 * @brief Get default Huber loss configuration
 * @param delta Threshold for L1/L2 transition (default 1.0)
 * @return Default Huber configuration
 */
nimcp_huber_config_t nimcp_loss_huber_default_config(float delta);

/**
 * @brief Get default focal loss configuration
 * @return Default focal loss configuration
 */
nimcp_focal_config_t nimcp_loss_focal_default_config(void);

/**
 * @brief Get default loss configuration for any type
 * @param type Loss function type
 * @return Default configuration for the specified type
 */
nimcp_loss_config_t nimcp_loss_default_config(nimcp_loss_type_t type);

/* ============================================================================
 * Loss Context Lifecycle
 * ============================================================================ */

/**
 * @brief Create a loss function context
 * @param config Loss configuration
 * @param security_ctx Security context (optional, can be NULL)
 * @param memory_mgr Memory manager (optional, can be NULL)
 * @return New loss context or NULL on failure
 */
nimcp_loss_context_t* nimcp_loss_create(
    const nimcp_loss_config_t* config,
    nimcp_sec_integration_t* security_ctx,
    unified_mem_manager_t memory_mgr
);

/**
 * @brief Initialize a loss context (after creation)
 * @param ctx Loss context
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_loss_init(nimcp_loss_context_t* ctx);

/**
 * @brief Destroy a loss context and free resources
 * @param ctx Loss context to destroy
 */
void nimcp_loss_destroy(nimcp_loss_context_t* ctx);

/**
 * @brief Reset loss context state (clear statistics)
 * @param ctx Loss context
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_loss_reset(nimcp_loss_context_t* ctx);

/* ============================================================================
 * Loss Computation Functions
 * ============================================================================ */

/**
 * @brief Compute loss value (forward pass)
 * @param ctx Loss context
 * @param predictions Model predictions (batch_size x output_size)
 * @param targets Ground truth targets (batch_size x output_size)
 * @param batch_size Number of samples in batch
 * @param output_size Size of each prediction/target vector
 * @param result Output result structure
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_loss_forward(
    nimcp_loss_context_t* ctx,
    const float* predictions,
    const float* targets,
    size_t batch_size,
    size_t output_size,
    nimcp_loss_result_t* result
);

/**
 * @brief Compute loss gradients (backward pass)
 * @param ctx Loss context
 * @param predictions Model predictions
 * @param targets Ground truth targets
 * @param batch_size Number of samples in batch
 * @param output_size Size of each vector
 * @param gradients Output gradient buffer (pre-allocated)
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_loss_backward(
    nimcp_loss_context_t* ctx,
    const float* predictions,
    const float* targets,
    size_t batch_size,
    size_t output_size,
    float* gradients
);

/**
 * @brief Compute loss and gradients in single pass
 * @param ctx Loss context
 * @param predictions Model predictions
 * @param targets Ground truth targets
 * @param batch_size Number of samples
 * @param output_size Size of each vector
 * @param result Output result with loss and gradients
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_loss_forward_backward(
    nimcp_loss_context_t* ctx,
    const float* predictions,
    const float* targets,
    size_t batch_size,
    size_t output_size,
    nimcp_loss_result_t* result
);

/* ============================================================================
 * Specific Loss Function Implementations (Direct API)
 * ============================================================================ */

/**
 * @brief Compute Mean Squared Error loss
 * @param predictions Predicted values
 * @param targets Target values
 * @param count Number of elements
 * @param reduction Reduction mode
 * @return Loss value
 */
float nimcp_loss_mse(
    const float* predictions,
    const float* targets,
    size_t count,
    nimcp_loss_reduction_t reduction
);

/**
 * @brief Compute MSE gradients
 * @param predictions Predicted values
 * @param targets Target values
 * @param gradients Output gradients
 * @param count Number of elements
 */
void nimcp_loss_mse_grad(
    const float* predictions,
    const float* targets,
    float* gradients,
    size_t count
);

/**
 * @brief Compute Mean Absolute Error loss
 * @param predictions Predicted values
 * @param targets Target values
 * @param count Number of elements
 * @param reduction Reduction mode
 * @return Loss value
 */
float nimcp_loss_mae(
    const float* predictions,
    const float* targets,
    size_t count,
    nimcp_loss_reduction_t reduction
);

/**
 * @brief Compute MAE gradients
 * @param predictions Predicted values
 * @param targets Target values
 * @param gradients Output gradients
 * @param count Number of elements
 */
void nimcp_loss_mae_grad(
    const float* predictions,
    const float* targets,
    float* gradients,
    size_t count
);

/**
 * @brief Compute binary cross-entropy loss
 * @param predictions Predicted probabilities [0,1]
 * @param targets Binary targets {0,1}
 * @param count Number of elements
 * @param reduction Reduction mode
 * @param epsilon Small value for numerical stability
 * @return Loss value
 */
float nimcp_loss_binary_cross_entropy(
    const float* predictions,
    const float* targets,
    size_t count,
    nimcp_loss_reduction_t reduction,
    float epsilon
);

/**
 * @brief Compute binary cross-entropy gradients
 * @param predictions Predicted probabilities
 * @param targets Binary targets
 * @param gradients Output gradients
 * @param count Number of elements
 * @param epsilon Numerical stability constant
 */
void nimcp_loss_binary_cross_entropy_grad(
    const float* predictions,
    const float* targets,
    float* gradients,
    size_t count,
    float epsilon
);

/**
 * @brief Compute categorical cross-entropy loss
 * @param predictions Predicted class probabilities (batch x classes)
 * @param targets One-hot encoded targets (batch x classes)
 * @param batch_size Number of samples
 * @param num_classes Number of classes
 * @param reduction Reduction mode
 * @param epsilon Numerical stability constant
 * @return Loss value
 */
float nimcp_loss_cross_entropy(
    const float* predictions,
    const float* targets,
    size_t batch_size,
    size_t num_classes,
    nimcp_loss_reduction_t reduction,
    float epsilon
);

/**
 * @brief Compute cross-entropy gradients
 * @param predictions Predicted probabilities
 * @param targets One-hot targets
 * @param gradients Output gradients
 * @param batch_size Number of samples
 * @param num_classes Number of classes
 */
void nimcp_loss_cross_entropy_grad(
    const float* predictions,
    const float* targets,
    float* gradients,
    size_t batch_size,
    size_t num_classes
);

/**
 * @brief Compute KL divergence
 * @param p First distribution (predictions)
 * @param q Second distribution (targets)
 * @param count Number of elements
 * @param reduction Reduction mode
 * @param epsilon Numerical stability constant
 * @return KL(p||q) divergence
 */
float nimcp_loss_kl_divergence(
    const float* p,
    const float* q,
    size_t count,
    nimcp_loss_reduction_t reduction,
    float epsilon
);

/**
 * @brief Compute KL divergence gradients
 * @param p First distribution
 * @param q Second distribution
 * @param gradients Output gradients
 * @param count Number of elements
 * @param epsilon Numerical stability constant
 */
void nimcp_loss_kl_divergence_grad(
    const float* p,
    const float* q,
    float* gradients,
    size_t count,
    float epsilon
);

/**
 * @brief Compute Huber loss
 * @param predictions Predicted values
 * @param targets Target values
 * @param count Number of elements
 * @param delta Threshold for L1/L2 transition
 * @param reduction Reduction mode
 * @return Loss value
 */
float nimcp_loss_huber(
    const float* predictions,
    const float* targets,
    size_t count,
    float delta,
    nimcp_loss_reduction_t reduction
);

/**
 * @brief Compute Huber loss gradients
 * @param predictions Predicted values
 * @param targets Target values
 * @param gradients Output gradients
 * @param count Number of elements
 * @param delta Threshold for L1/L2 transition
 */
void nimcp_loss_huber_grad(
    const float* predictions,
    const float* targets,
    float* gradients,
    size_t count,
    float delta
);

/**
 * @brief Compute hinge loss (for SVM-style classification)
 * @param predictions Raw scores (not probabilities)
 * @param targets Labels {-1, +1}
 * @param count Number of elements
 * @param reduction Reduction mode
 * @return Loss value
 */
float nimcp_loss_hinge(
    const float* predictions,
    const float* targets,
    size_t count,
    nimcp_loss_reduction_t reduction
);

/**
 * @brief Compute hinge loss gradients
 * @param predictions Raw scores
 * @param targets Labels {-1, +1}
 * @param gradients Output gradients
 * @param count Number of elements
 */
void nimcp_loss_hinge_grad(
    const float* predictions,
    const float* targets,
    float* gradients,
    size_t count
);

/**
 * @brief Compute focal loss (for class imbalance)
 * @param predictions Predicted probabilities
 * @param targets Binary targets
 * @param count Number of elements
 * @param gamma Focusing parameter
 * @param alpha Class balance weight
 * @param reduction Reduction mode
 * @param epsilon Numerical stability constant
 * @return Loss value
 */
float nimcp_loss_focal(
    const float* predictions,
    const float* targets,
    size_t count,
    float gamma,
    float alpha,
    nimcp_loss_reduction_t reduction,
    float epsilon
);

/**
 * @brief Compute focal loss gradients
 * @param predictions Predicted probabilities
 * @param targets Binary targets
 * @param gradients Output gradients
 * @param count Number of elements
 * @param gamma Focusing parameter
 * @param alpha Class balance weight
 * @param epsilon Numerical stability constant
 */
void nimcp_loss_focal_grad(
    const float* predictions,
    const float* targets,
    float* gradients,
    size_t count,
    float gamma,
    float alpha,
    float epsilon
);

/**
 * @brief Compute contrastive loss for siamese networks
 * @param embeddings1 First set of embeddings
 * @param embeddings2 Second set of embeddings
 * @param labels Similarity labels (1=similar, 0=dissimilar)
 * @param batch_size Number of pairs
 * @param embed_dim Embedding dimension
 * @param margin Margin for dissimilar pairs
 * @param reduction Reduction mode
 * @return Loss value
 */
float nimcp_loss_contrastive(
    const float* embeddings1,
    const float* embeddings2,
    const float* labels,
    size_t batch_size,
    size_t embed_dim,
    float margin,
    nimcp_loss_reduction_t reduction
);

/**
 * @brief Compute triplet loss for metric learning
 * @param anchors Anchor embeddings
 * @param positives Positive embeddings
 * @param negatives Negative embeddings
 * @param batch_size Number of triplets
 * @param embed_dim Embedding dimension
 * @param margin Margin between positive and negative
 * @param reduction Reduction mode
 * @return Loss value
 */
float nimcp_loss_triplet(
    const float* anchors,
    const float* positives,
    const float* negatives,
    size_t batch_size,
    size_t embed_dim,
    float margin,
    nimcp_loss_reduction_t reduction
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get loss function statistics
 * @param ctx Loss context
 * @param stats Output statistics structure
 * @return NIMCP_OK on success
 */
nimcp_result_t nimcp_loss_get_stats(
    const nimcp_loss_context_t* ctx,
    nimcp_loss_stats_t* stats
);

/**
 * @brief Reset loss statistics to zero
 * @param ctx Loss context
 */
void nimcp_loss_reset_stats(nimcp_loss_context_t* ctx);

/**
 * @brief Get loss function type name
 * @param type Loss type
 * @return String name of the loss type
 */
const char* nimcp_loss_type_name(nimcp_loss_type_t type);

/**
 * @brief Validate loss configuration
 * @param config Configuration to validate
 * @return NIMCP_OK if valid, error code otherwise
 */
nimcp_result_t nimcp_loss_validate_config(const nimcp_loss_config_t* config);

/**
 * @brief Apply softmax activation to logits
 * @param logits Input logits
 * @param output Output probabilities
 * @param batch_size Number of samples
 * @param num_classes Number of classes
 */
void nimcp_loss_softmax(
    const float* logits,
    float* output,
    size_t batch_size,
    size_t num_classes
);

/**
 * @brief Apply sigmoid activation
 * @param input Input values
 * @param output Output probabilities
 * @param count Number of elements
 */
void nimcp_loss_sigmoid(
    const float* input,
    float* output,
    size_t count
);

/**
 * @brief Clip gradients by value
 * @param gradients Gradient array (modified in place)
 * @param count Number of gradients
 * @param max_value Maximum absolute value
 * @return Number of gradients clipped
 */
size_t nimcp_loss_clip_gradients(
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
float nimcp_loss_clip_gradients_norm(
    float* gradients,
    size_t count,
    float max_norm
);

/**
 * @brief Free loss result resources
 * @param result Result structure to free
 */
void nimcp_loss_result_free(nimcp_loss_result_t* result);

/**
 * @brief Check if loss context is registered with security module
 * @param ctx Loss context
 * @return true if registered, false otherwise
 */
bool nimcp_loss_is_registered(const nimcp_loss_context_t* ctx);

/* ============================================================================
 * Tensor-Based Loss Functions
 * ============================================================================ */

/* Forward declaration for tensor type */
#ifndef NIMCP_TENSOR_T_DEFINED
#define NIMCP_TENSOR_T_DEFINED
struct nimcp_tensor_s;
typedef struct nimcp_tensor_s nimcp_tensor_t;
#endif

/**
 * @brief Compute MSE loss from tensors
 *
 * More efficient than scalar version for large batches.
 *
 * @param predictions Prediction tensor
 * @param targets Target tensor
 * @param reduction Reduction mode
 * @return Loss value
 */
float nimcp_loss_mse_tensor(
    const nimcp_tensor_t* predictions,
    const nimcp_tensor_t* targets,
    nimcp_loss_reduction_t reduction
);

/**
 * @brief Compute MSE gradient tensor
 *
 * Returns gradient = 2 * (predictions - targets) / n
 *
 * @param predictions Prediction tensor
 * @param targets Target tensor
 * @return Gradient tensor (caller must destroy)
 */
nimcp_tensor_t* nimcp_loss_mse_grad_tensor(
    const nimcp_tensor_t* predictions,
    const nimcp_tensor_t* targets
);

/**
 * @brief Compute softmax from logits tensor
 *
 * @param logits Input logits tensor
 * @param axis Axis to compute softmax over (-1 for last)
 * @return Probability tensor (caller must destroy)
 */
nimcp_tensor_t* nimcp_loss_softmax_tensor(
    const nimcp_tensor_t* logits,
    int axis
);

/**
 * @brief Compute cross-entropy loss from tensors
 *
 * @param predictions Prediction tensor (after softmax)
 * @param targets One-hot encoded target tensor
 * @param epsilon Numerical stability constant
 * @param reduction Reduction mode
 * @return Loss value
 */
float nimcp_loss_cross_entropy_tensor(
    const nimcp_tensor_t* predictions,
    const nimcp_tensor_t* targets,
    float epsilon,
    nimcp_loss_reduction_t reduction
);

/**
 * @brief Clip gradient tensor by norm
 *
 * Scales gradient tensor if L2 norm exceeds max_norm.
 *
 * @param gradients Gradient tensor (modified in place)
 * @param max_norm Maximum allowed norm
 * @return Original norm before clipping
 */
float nimcp_loss_clip_gradients_norm_tensor(
    nimcp_tensor_t* gradients,
    float max_norm
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LOSS_FUNCTIONS_H */
