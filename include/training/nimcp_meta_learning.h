/**
 * @file nimcp_meta_learning.h
 * @brief Meta-Learning (Learning to Learn) for NIMCP
 *
 * WHAT: Algorithms that learn how to learn efficiently across tasks
 * WHY:  Enable rapid adaptation to new tasks with few examples
 * HOW:  MAML, Reptile, Prototypical Networks, and other meta-learning methods
 *
 * FRAMEWORK COMPARISON:
 * - PyTorch: Higher (learn2learn), torchmeta, pytorch-maml
 * - JAX: jaxopt, haiku MAML implementations
 * - TensorFlow: tf-maml, learn2learn tensorflow
 *
 * NIMCP APPROACH:
 * - Integrates with optimizer layer for higher-order gradients
 * - Bio-inspired via prefrontal executive control
 * - Connects to brain factory for task-switching regions
 *
 * BIOLOGICAL GROUNDING:
 * - Prefrontal cortex: Executive function, rapid rule learning
 * - Hippocampus: Rapid one-shot memory formation
 * - Dopamine: Learning rate modulation based on prediction error
 * - Sleep consolidation: Meta-learning over episodic experiences
 *
 * INTEGRATION POINTS:
 * - optimizers: Higher-order gradient computation
 * - brain_factory: Task-switching regions (PFC, striatum)
 * - gradient_manager: Second-order gradient handling
 * - training_callbacks: Episodic training loop
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#ifndef NIMCP_META_LEARNING_H
#define NIMCP_META_LEARNING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/tensor/nimcp_tensor.h"
#include "utils/error/nimcp_error_codes.h"
#include "middleware/training/nimcp_optimizers.h"
#include "middleware/training/nimcp_gradient_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define META_DEFAULT_INNER_LR         0.01f   /**< Default inner loop LR */
#define META_DEFAULT_OUTER_LR         0.001f  /**< Default outer loop LR */
#define META_DEFAULT_INNER_STEPS      5       /**< Default inner loop steps */
#define META_MAX_TASKS_PER_BATCH      64      /**< Maximum tasks per meta-batch */
#define META_MAX_SHOTS                100     /**< Maximum shots per task */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Meta-learning algorithm
 *
 * COMPARISON:
 * - MAML (Finn 2017): Optimization-based, second-order
 * - Reptile (Nichol 2018): First-order approximation
 * - Prototypical (Snell 2017): Metric-based
 * - MetaSGD (Li 2017): Per-parameter learning rates
 */
typedef enum {
    META_ALG_MAML = 0,               /**< Model-Agnostic Meta-Learning */
    META_ALG_FOMAML,                 /**< First-Order MAML (faster) */
    META_ALG_REPTILE,                /**< Reptile (batch averaging) */
    META_ALG_METASGD,                /**< MetaSGD (per-param LR) */
    META_ALG_ANIL,                   /**< Almost No Inner Loop */
    META_ALG_BOIL,                   /**< Body Only Inner Learning */
    META_ALG_PROTOTYPICAL,           /**< Prototypical Networks */
    META_ALG_MATCHING,               /**< Matching Networks */
    META_ALG_RELATION,               /**< Relation Networks */
    META_ALG_EMAML,                  /**< Efficient MAML (implicit diff) */
    META_ALG_LEAP,                   /**< Learning Embeddings for Adapt */
    META_ALG_COUNT
} meta_algorithm_t;

/**
 * @brief Task sampling strategy
 */
typedef enum {
    META_TASK_SAMPLE_UNIFORM = 0,    /**< Uniform random sampling */
    META_TASK_SAMPLE_CURRICULUM,     /**< Curriculum (easy to hard) */
    META_TASK_SAMPLE_IMPORTANCE,     /**< Importance sampling */
    META_TASK_SAMPLE_STRATIFIED,     /**< Stratified by task type */
    META_TASK_SAMPLE_COUNT
} meta_task_sampling_t;

/**
 * @brief Inner loop optimization strategy
 */
typedef enum {
    META_INNER_SGD = 0,              /**< Standard SGD */
    META_INNER_ADAM,                 /**< Adam optimizer */
    META_INNER_MAHALANOBIS,          /**< Mahalanobis distance learning */
    META_INNER_COUNT
} meta_inner_opt_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief MAML configuration
 *
 * WHAT: Model-Agnostic Meta-Learning settings
 * WHY:  Most general optimization-based meta-learning
 * HOW:  Inner loop adaptation, outer loop meta-update
 */
typedef struct {
    float inner_lr;                  /**< Inner loop learning rate */
    float outer_lr;                  /**< Outer loop learning rate */
    uint32_t inner_steps;            /**< Inner loop gradient steps */
    bool first_order;                /**< Use first-order approximation */
    bool learn_inner_lr;             /**< Learn per-param inner LR (MetaSGD) */
    float inner_lr_init;             /**< Initial inner LR (if learned) */
} meta_maml_config_t;

/**
 * @brief Reptile configuration
 */
typedef struct {
    float inner_lr;                  /**< Inner loop learning rate */
    float outer_lr;                  /**< Outer loop (interpolation) rate */
    uint32_t inner_steps;            /**< Inner loop steps per task */
    meta_inner_opt_t inner_optimizer; /**< Inner optimizer type */
} meta_reptile_config_t;

/**
 * @brief Prototypical networks configuration
 */
typedef struct {
    uint32_t embedding_dim;          /**< Embedding dimension */
    float temperature;               /**< Softmax temperature */
    bool use_attention;              /**< Attention over support set */
    bool euclidean_distance;         /**< Use Euclidean (else cosine) */
} meta_prototypical_config_t;

/**
 * @brief Task configuration
 */
typedef struct {
    uint32_t n_way;                  /**< Number of classes per task */
    uint32_t k_shot;                 /**< Support examples per class */
    uint32_t query_size;             /**< Query examples per class */
} meta_task_config_t;

/**
 * @brief Complete meta-learning configuration
 */
typedef struct {
    meta_algorithm_t algorithm;      /**< Meta-learning algorithm */

    /* Algorithm-specific configs */
    meta_maml_config_t maml;
    meta_reptile_config_t reptile;
    meta_prototypical_config_t prototypical;

    /* Task settings */
    meta_task_config_t task;
    meta_task_sampling_t task_sampling;
    uint32_t tasks_per_batch;        /**< Tasks per meta-batch */

    /* Outer loop settings */
    nimcp_optimizer_type_t outer_optimizer; /**< Outer loop optimizer */
    float outer_lr;                  /**< Outer loop learning rate */

    /* Regularization */
    float outer_weight_decay;        /**< Outer loop weight decay */
    float task_augmentation;         /**< Task augmentation probability */

    /* Integration */
    bool integrate_gradient_manager; /**< Use gradient_manager */
    bool integrate_brain_factory;    /**< Connect to PFC/striatum */

    /* Debugging */
    bool verbose;
    bool track_statistics;
} meta_config_t;

//=============================================================================
// Task Structures
//=============================================================================

/**
 * @brief Meta-learning task
 *
 * Represents one task in the task distribution
 */
typedef struct {
    uint32_t task_id;                /**< Task identifier */
    const char* name;                /**< Task name */

    /* Support set (training examples) */
    nimcp_tensor_t* support_x;       /**< Support inputs [n_way * k_shot, ...] */
    nimcp_tensor_t* support_y;       /**< Support labels [n_way * k_shot] */

    /* Query set (evaluation examples) */
    nimcp_tensor_t* query_x;         /**< Query inputs [n_way * query, ...] */
    nimcp_tensor_t* query_y;         /**< Query labels [n_way * query] */

    /* Task metadata */
    uint32_t n_way;                  /**< Number of classes */
    uint32_t k_shot;                 /**< Support examples per class */
    uint32_t query_size;             /**< Query examples per class */
} meta_task_t;

/**
 * @brief Task batch for meta-training
 */
typedef struct {
    meta_task_t* tasks;              /**< Array of tasks */
    uint32_t num_tasks;              /**< Number of tasks */
    uint32_t batch_id;               /**< Batch identifier */
} meta_task_batch_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Meta-learning statistics
 */
typedef struct {
    uint64_t total_meta_steps;       /**< Total meta-training steps */
    uint64_t total_inner_steps;      /**< Total inner loop steps */
    uint64_t tasks_processed;        /**< Tasks processed */

    /* Loss statistics */
    float avg_support_loss;          /**< Average support set loss */
    float avg_query_loss;            /**< Average query set loss */
    float min_query_loss;            /**< Minimum query loss */

    /* Accuracy statistics */
    float avg_support_accuracy;      /**< Average support accuracy */
    float avg_query_accuracy;        /**< Average query accuracy */
    float adaptation_improvement;    /**< Query - support accuracy */

    /* Gradient statistics */
    float avg_inner_grad_norm;       /**< Average inner gradient norm */
    float avg_outer_grad_norm;       /**< Average outer gradient norm */
    float avg_second_order_norm;     /**< Second-order gradient norm */

    /* Timing */
    double inner_loop_time_ms;       /**< Time in inner loops */
    double outer_loop_time_ms;       /**< Time in outer loops */
    double total_time_ms;            /**< Total training time */
} meta_stats_t;

//=============================================================================
// Opaque Types
//=============================================================================

/**
 * @brief Meta-learning context (opaque)
 */
typedef struct meta_ctx_s meta_ctx_t;

/**
 * @brief Adapted model state (after inner loop)
 */
typedef struct meta_adapted_params_s meta_adapted_params_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default meta-learning configuration
 *
 * DEFAULTS:
 * - MAML algorithm
 * - 5-way 5-shot
 * - 5 inner steps
 * - Inner LR: 0.01, Outer LR: 0.001
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
int meta_default_config(meta_config_t* config);

/**
 * @brief Create meta-learning context
 *
 * @param config Meta-learning configuration
 * @return Meta-learning context or NULL on failure
 */
meta_ctx_t* meta_create(const meta_config_t* config);

/**
 * @brief Destroy meta-learning context
 *
 * @param ctx Context to destroy (NULL-safe)
 */
void meta_destroy(meta_ctx_t* ctx);

/**
 * @brief Register model parameters for meta-learning
 *
 * @param ctx Meta-learning context
 * @param params Parameter tensors
 * @param num_params Number of parameter tensors
 * @return 0 on success, negative on error
 */
int meta_register_params(
    meta_ctx_t* ctx,
    nimcp_tensor_t** params,
    uint32_t num_params
);

//=============================================================================
// Task API
//=============================================================================

/**
 * @brief Create task from data
 *
 * @param ctx Meta-learning context
 * @param data Full dataset tensor
 * @param labels Full label tensor
 * @param n_way Number of classes
 * @param k_shot Support examples per class
 * @param query_size Query examples per class
 * @return Task or NULL on failure
 */
meta_task_t* meta_create_task(
    meta_ctx_t* ctx,
    const nimcp_tensor_t* data,
    const nimcp_tensor_t* labels,
    uint32_t n_way,
    uint32_t k_shot,
    uint32_t query_size
);

/**
 * @brief Destroy task
 *
 * @param task Task to destroy (NULL-safe)
 */
void meta_destroy_task(meta_task_t* task);

/**
 * @brief Sample task batch from task distribution
 *
 * @param ctx Meta-learning context
 * @param num_tasks Number of tasks to sample
 * @return Task batch or NULL on failure
 */
meta_task_batch_t* meta_sample_tasks(
    meta_ctx_t* ctx,
    uint32_t num_tasks
);

/**
 * @brief Destroy task batch
 *
 * @param batch Batch to destroy (NULL-safe)
 */
void meta_destroy_task_batch(meta_task_batch_t* batch);

//=============================================================================
// Inner Loop API
//=============================================================================

/**
 * @brief Perform inner loop adaptation on task
 *
 * WHAT: Adapt model to single task using support set
 * WHY:  Learn task-specific parameters
 * HOW:  Gradient descent on support set loss
 *
 * COMPARISON (learn2learn equivalent):
 * ```python
 * learner = maml.clone()
 * for step in range(inner_steps):
 *     support_loss = criterion(learner(support_x), support_y)
 *     learner.adapt(support_loss)
 * ```
 *
 * @param ctx Meta-learning context
 * @param task Task to adapt to
 * @param forward_fn Model forward function
 * @param adapted_params Output adapted parameters
 * @return Support set loss
 */
float meta_inner_loop(
    meta_ctx_t* ctx,
    const meta_task_t* task,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    meta_adapted_params_t** adapted_params
);

/**
 * @brief Compute query loss with adapted parameters
 *
 * @param ctx Meta-learning context
 * @param task Task
 * @param adapted_params Adapted parameters from inner loop
 * @param forward_fn Model forward function
 * @param query_loss Output query loss
 * @param query_accuracy Output query accuracy (optional)
 * @return 0 on success, negative on error
 */
int meta_query_loss(
    meta_ctx_t* ctx,
    const meta_task_t* task,
    const meta_adapted_params_t* adapted_params,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    float* query_loss,
    float* query_accuracy
);

/**
 * @brief Free adapted parameters
 *
 * @param adapted Adapted parameters to free
 */
void meta_free_adapted(meta_adapted_params_t* adapted);

/**
 * @brief Compute second-order meta-gradient for full MAML
 *
 * WHAT: Compute gradient that accounts for inner loop adaptation
 * WHY:  Full MAML requires differentiating through the inner loop
 * HOW:  Use Hessian-vector products to compute d(theta')/d(theta)
 *
 * The meta-gradient for full MAML is:
 *   d(L_query(theta'))/d(theta) = d(L_query)/d(theta') * (I - alpha * H)
 *
 * Where H is the Hessian of the support loss and alpha is the inner LR.
 *
 * @param ctx Meta-learning context
 * @param query_grad Gradient of query loss w.r.t. adapted parameters
 * @param support_grad Gradient of support loss w.r.t. original parameters
 * @param meta_grad Output: meta-gradient incorporating second-order terms
 * @param param_count Total number of parameters
 * @param inner_lr Inner loop learning rate
 * @return 0 on success, negative on error
 */
int meta_compute_second_order_gradient(
    meta_ctx_t* ctx,
    float* query_grad,
    float* support_grad,
    float* meta_grad,
    size_t param_count,
    float inner_lr
);

//=============================================================================
// Outer Loop API
//=============================================================================

/**
 * @brief Perform meta-training step
 *
 * WHAT: Update meta-parameters using task batch
 * WHY:  Learn initialization that adapts quickly
 * HOW:  Average query losses across tasks, compute meta-gradient
 *
 * COMPARISON (learn2learn equivalent):
 * ```python
 * meta_optimizer.zero_grad()
 * for task in task_batch:
 *     learner = maml.clone()
 *     learner.adapt(support_loss)
 *     query_loss = criterion(learner(query_x), query_y)
 *     query_loss.backward()
 * meta_optimizer.step()
 * ```
 *
 * @param ctx Meta-learning context
 * @param task_batch Batch of tasks
 * @param forward_fn Model forward function
 * @param model Model to meta-train
 * @param avg_query_loss Output average query loss
 * @return 0 on success, negative on error
 */
int meta_step(
    meta_ctx_t* ctx,
    const meta_task_batch_t* task_batch,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    float* avg_query_loss
);

/**
 * @brief Reptile meta-step
 *
 * WHAT: Simplified meta-learning without second-order gradients
 * WHY:  Faster than MAML, similar performance
 * HOW:  Interpolate toward task-adapted parameters
 *
 * @param ctx Meta-learning context
 * @param task Task to adapt to
 * @param forward_fn Model forward function
 * @param model Model to update
 * @return Task loss
 */
float meta_reptile_step(
    meta_ctx_t* ctx,
    const meta_task_t* task,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model
);

//=============================================================================
// Metric-Based Methods API
//=============================================================================

/**
 * @brief Compute prototypical network predictions
 *
 * WHAT: Classify query examples based on class prototypes
 * WHY:  Simple, effective metric-based meta-learning
 * HOW:  Compute class means, classify by distance
 *
 * @param ctx Meta-learning context
 * @param task Task with support and query sets
 * @param embed_fn Embedding function
 * @param model Embedding model
 * @param predictions Output predicted classes
 * @return 0 on success, negative on error
 */
int meta_prototypical_predict(
    meta_ctx_t* ctx,
    const meta_task_t* task,
    nimcp_tensor_t* (*embed_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    nimcp_tensor_t* predictions
);

/**
 * @brief Compute prototypical loss
 *
 * @param ctx Meta-learning context
 * @param task Task
 * @param embed_fn Embedding function
 * @param model Model
 * @param loss Output loss
 * @param accuracy Output accuracy
 * @return 0 on success, negative on error
 */
int meta_prototypical_loss(
    meta_ctx_t* ctx,
    const meta_task_t* task,
    nimcp_tensor_t* (*embed_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    float* loss,
    float* accuracy
);

//=============================================================================
// Evaluation API
//=============================================================================

/**
 * @brief Evaluate meta-learned model on new task
 *
 * @param ctx Meta-learning context
 * @param task New task
 * @param forward_fn Model forward function
 * @param model Model to evaluate
 * @param accuracy Output accuracy
 * @return 0 on success, negative on error
 */
int meta_evaluate_task(
    meta_ctx_t* ctx,
    const meta_task_t* task,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    float* accuracy
);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Connect to gradient manager
 *
 * @param ctx Meta-learning context
 * @param grad_manager Gradient manager
 * @return 0 on success, negative on error
 */
int meta_connect_gradient_manager(
    meta_ctx_t* ctx,
    nimcp_gradient_manager_ctx_t* grad_manager
);

/**
 * @brief Connect to brain factory for task-switching regions
 *
 * BIOLOGICAL BASIS:
 * - PFC: Executive control, rule switching
 * - Striatum: Action selection, task gating
 * - Dopamine: Learning rate modulation
 *
 * @param ctx Meta-learning context
 * @param brain_factory Brain factory
 * @return 0 on success, negative on error
 */
int meta_connect_brain_factory(meta_ctx_t* ctx, void* brain_factory);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get meta-learning statistics
 *
 * @param ctx Meta-learning context
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int metalearn_get_stats(const meta_ctx_t* ctx, meta_stats_t* stats);

/**
 * @brief Reset meta-learning statistics
 *
 * @param ctx Meta-learning context
 */
void metalearn_reset_stats(meta_ctx_t* ctx);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get algorithm name
 */
const char* meta_algorithm_name(meta_algorithm_t alg);

/**
 * @brief Validate meta-learning configuration
 */
int meta_validate_config(const meta_config_t* config);

//=============================================================================
// Compatibility Aliases
//=============================================================================

/**
 * @brief Type alias for meta_ctx_t (for test compatibility)
 */
typedef meta_ctx_t meta_learner_t;

/**
 * @brief Get default meta-learning configuration (returns struct by value)
 *
 * This is a convenience wrapper for meta_default_config().
 *
 * @return Default configuration
 */
static inline meta_config_t meta_learning_default_config(void) {
    meta_config_t config;
    meta_default_config(&config);
    return config;
}

/**
 * @brief Create meta-learner (alias for meta_create)
 */
#define meta_learning_create meta_create

/**
 * @brief Destroy meta-learner (alias for meta_destroy)
 */
#define meta_learning_destroy meta_destroy

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_META_LEARNING_H */
