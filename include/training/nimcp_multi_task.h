/**
 * @file nimcp_multi_task.h
 * @brief Multi-Task Learning (MTL) Coordination for NIMCP
 *
 * WHAT: Train single model on multiple related tasks simultaneously
 * WHY:  Improve generalization, efficiency, and learn shared representations
 * HOW:  Task-specific heads, gradient balancing, loss weighting
 *
 * FRAMEWORK COMPARISON:
 * - PyTorch: torchMTL, LibMTL, PCGrad
 * - JAX: Manual implementation
 * - TensorFlow: tf.keras multi-output models
 *
 * NIMCP APPROACH:
 * - Deep integration with brain region specialization
 * - Bio-inspired via cortical module specialization
 * - Leverages thalamic routing for task-specific pathways
 *
 * BIOLOGICAL GROUNDING:
 * - Prefrontal cortex: Task switching and executive control
 * - Shared visual cortex for multiple visual tasks
 * - Thalamic routing: Selective attention to task-relevant info
 * - Modular brain organization: Specialized + shared processing
 *
 * INTEGRATION POINTS:
 * - brain_factory: Brain region specialization
 * - thalamic_router: Task-specific routing
 * - gradient_manager: Gradient balancing
 * - training_callbacks: Task scheduling
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#ifndef NIMCP_MULTI_TASK_H
#define NIMCP_MULTI_TASK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/tensor/nimcp_tensor.h"
#include "utils/error/nimcp_error_codes.h"
#include "middleware/training/nimcp_gradient_manager.h"
#include "middleware/training/nimcp_loss_functions.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define MTL_MAX_TASKS                 64       /**< Maximum tasks */
#define MTL_MAX_HEADS                 64       /**< Maximum output heads */
#define MTL_DEFAULT_GRADIENT_NORM     1.0f     /**< Default gradient norm */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Multi-task architecture type
 *
 * BIOLOGICAL BASIS:
 * - Hard sharing: Shared early processing (V1 for all vision)
 * - Soft sharing: Cross-task communication (lateral connections)
 * - Modular: Specialized + routing (cortical columns)
 */
typedef enum {
    MTL_ARCH_HARD_SHARING = 0,       /**< Shared encoder, task-specific heads */
    MTL_ARCH_SOFT_SHARING,           /**< Task-specific with cross-stitch */
    MTL_ARCH_MODULAR,                /**< Modular with routing */
    MTL_ARCH_PROGRESSIVE,            /**< Progressive addition of tasks */
    MTL_ARCH_MIXTURE_OF_EXPERTS,     /**< MoE with task routing */
    MTL_ARCH_COUNT
} mtl_architecture_t;

/**
 * @brief Loss weighting strategy
 *
 * COMPARISON:
 * - Uniform: Equal weights (baseline)
 * - Uncertainty (Kendall 2018): Homoscedastic uncertainty
 * - GradNorm (Chen 2018): Dynamic gradient normalization
 * - MGDA (Sener 2018): Multi-gradient descent
 * - PCGrad (Yu 2020): Project conflicting gradients
 */
typedef enum {
    MTL_WEIGHT_UNIFORM = 0,          /**< Equal task weights */
    MTL_WEIGHT_UNCERTAINTY,          /**< Uncertainty weighting */
    MTL_WEIGHT_GRADNORM,             /**< GradNorm balancing */
    MTL_WEIGHT_DWA,                  /**< Dynamic Weight Average */
    MTL_WEIGHT_MGDA,                 /**< Multi-objective gradient descent */
    MTL_WEIGHT_NASH,                 /**< Nash equilibrium optimization */
    MTL_WEIGHT_IMTL,                 /**< Impartial multi-task learning */
    MTL_WEIGHT_RANDOM,               /**< Random loss scale */
    MTL_WEIGHT_LEARNED,              /**< Learnable weights */
    MTL_WEIGHT_COUNT
} mtl_weight_strategy_t;

/**
 * @brief Gradient conflict handling
 *
 * BIOLOGICAL BASIS:
 * - Gradient conflicts ≈ competing neural activities
 * - PCGrad ≈ lateral inhibition
 * - CAGrad ≈ attentional modulation
 */
typedef enum {
    MTL_GRAD_NONE = 0,               /**< No gradient modification */
    MTL_GRAD_PCGRAD,                 /**< Project conflicting gradients */
    MTL_GRAD_CAGRAD,                 /**< Conflict-averse gradient descent */
    MTL_GRAD_GRADDROP,               /**< Randomly drop conflicting grads */
    MTL_GRAD_VACCINE,                /**< Vaccine for task interference */
    MTL_GRAD_COUNT
} mtl_gradient_method_t;

/**
 * @brief Task sampling strategy
 */
typedef enum {
    MTL_SAMPLE_UNIFORM = 0,          /**< Uniform sampling */
    MTL_SAMPLE_PROPORTIONAL,         /**< Proportional to dataset size */
    MTL_SAMPLE_INVERSE_SQRT,         /**< Inverse square root of size */
    MTL_SAMPLE_TEMPERATURE,          /**< Temperature-based sampling */
    MTL_SAMPLE_ROUND_ROBIN,          /**< Round-robin through tasks */
    MTL_SAMPLE_COUNT
} mtl_task_sampling_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Task definition
 */
typedef struct {
    uint32_t task_id;                /**< Task identifier */
    const char* name;                /**< Task name */
    nimcp_loss_type_t loss_type;     /**< Loss function type */
    uint32_t output_dim;             /**< Output dimension */
    float weight;                    /**< Initial task weight */
    bool active;                     /**< Task is active for training */

    /* Task metadata */
    uint32_t num_classes;            /**< For classification tasks */
    bool is_regression;              /**< Regression vs classification */
    float loss_scale;                /**< Loss scaling factor */
} mtl_task_def_t;

/**
 * @brief Uncertainty weighting configuration
 *
 * REFERENCE: Kendall et al. 2018 "Multi-Task Learning Using Uncertainty"
 */
typedef struct {
    float* log_vars;                 /**< Log variance per task */
    float prior_strength;            /**< Regularization strength */
    bool learn_log_vars;             /**< Learn log variances */
} mtl_uncertainty_config_t;

/**
 * @brief GradNorm configuration
 *
 * REFERENCE: Chen et al. 2018 "GradNorm"
 */
typedef struct {
    float alpha;                     /**< Asymmetry parameter */
    float initial_weights;           /**< Initial task weights */
    float weight_lr;                 /**< Learning rate for weights */
    bool use_last_layer;             /**< Only balance last layer */
} mtl_gradnorm_config_t;

/**
 * @brief PCGrad configuration
 *
 * REFERENCE: Yu et al. 2020 "Gradient Surgery"
 */
typedef struct {
    float projection_eps;            /**< Projection epsilon */
    bool normalize_gradients;        /**< Normalize before projection */
    bool use_random_order;           /**< Random task order */
} mtl_pcgrad_config_t;

/**
 * @brief CAGrad configuration
 *
 * REFERENCE: Liu et al. 2021 "Conflict-Averse Gradient Descent"
 */
typedef struct {
    float c;                         /**< Conflict weight */
    float rescale;                   /**< Rescale factor */
} mtl_cagrad_config_t;

/**
 * @brief Complete MTL configuration
 */
typedef struct {
    mtl_architecture_t architecture; /**< Architecture type */
    mtl_weight_strategy_t weighting; /**< Loss weighting strategy */
    mtl_gradient_method_t gradient_method; /**< Gradient handling */
    mtl_task_sampling_t sampling;    /**< Task sampling strategy */

    /* Task definitions */
    mtl_task_def_t* tasks;           /**< Array of task definitions */
    uint32_t num_tasks;              /**< Number of tasks */

    /* Strategy-specific configs */
    mtl_uncertainty_config_t uncertainty;
    mtl_gradnorm_config_t gradnorm;
    mtl_pcgrad_config_t pcgrad;
    mtl_cagrad_config_t cagrad;

    /* Training settings */
    float shared_lr;                 /**< Learning rate for shared params */
    float task_lr_multiplier;        /**< Multiplier for task-specific LR */
    float auxiliary_weight;          /**< Weight for auxiliary tasks */

    /* Task scheduling */
    float temperature;               /**< Sampling temperature */
    uint32_t warmup_steps;           /**< Warmup steps per task */
    bool balance_batches;            /**< Balance batch across tasks */

    /* Integration */
    bool integrate_gradient_manager; /**< Use gradient_manager */
    bool integrate_brain_factory;    /**< Connect to brain regions */
    bool integrate_thalamic_router;  /**< Task-specific routing */

    /* Debugging */
    bool verbose;
    bool track_statistics;
} mtl_config_t;

//=============================================================================
// Batch Structures
//=============================================================================

/**
 * @brief Multi-task batch
 */
typedef struct {
    nimcp_tensor_t** inputs;         /**< Input per task [num_tasks] */
    nimcp_tensor_t** labels;         /**< Labels per task [num_tasks] */
    uint32_t* batch_sizes;           /**< Batch size per task */
    uint32_t* task_ids;              /**< Task IDs in this batch */
    uint32_t num_active_tasks;       /**< Number of active tasks */
} mtl_batch_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Multi-task learning statistics
 */
typedef struct {
    uint64_t total_steps;            /**< Total training steps */
    uint64_t steps_per_task[MTL_MAX_TASKS]; /**< Steps per task */

    /* Loss statistics per task */
    float task_losses[MTL_MAX_TASKS];/**< Current loss per task */
    float task_weights[MTL_MAX_TASKS];/**< Current weight per task */
    float avg_loss;                  /**< Average weighted loss */

    /* Gradient statistics */
    float task_grad_norms[MTL_MAX_TASKS]; /**< Gradient norm per task */
    float gradient_similarity;       /**< Average gradient cosine sim */
    float conflict_ratio;            /**< Fraction of conflicting grads */

    /* Performance */
    float task_accuracies[MTL_MAX_TASKS]; /**< Accuracy per task */
    float avg_accuracy;              /**< Average accuracy */

    /* Weight dynamics */
    float weight_variance;           /**< Variance of task weights */
    float weight_entropy;            /**< Entropy of weight distribution */
} mtl_stats_t;

//=============================================================================
// Opaque Types
//=============================================================================

/**
 * @brief Multi-task learning context (opaque)
 */
typedef struct mtl_ctx_s mtl_ctx_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default MTL configuration
 *
 * DEFAULTS:
 * - Hard sharing architecture
 * - Uncertainty weighting
 * - PCGrad for gradient conflicts
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
int mtl_default_config(mtl_config_t* config);

/**
 * @brief Create MTL context
 *
 * @param config MTL configuration
 * @return MTL context or NULL on failure
 */
mtl_ctx_t* mtl_create(const mtl_config_t* config);

/**
 * @brief Destroy MTL context
 *
 * @param ctx Context to destroy (NULL-safe)
 */
void mtl_destroy(mtl_ctx_t* ctx);

//=============================================================================
// Task Management API
//=============================================================================

/**
 * @brief Register task
 *
 * @param ctx MTL context
 * @param task Task definition
 * @return Task index or negative on error
 */
int mtl_register_task(mtl_ctx_t* ctx, const mtl_task_def_t* task);

/**
 * @brief Set task active/inactive
 *
 * @param ctx MTL context
 * @param task_id Task ID
 * @param active Whether task is active
 * @return 0 on success, negative on error
 */
int mtl_set_task_active(mtl_ctx_t* ctx, uint32_t task_id, bool active);

/**
 * @brief Get task weight
 *
 * @param ctx MTL context
 * @param task_id Task ID
 * @return Current task weight
 */
float mtl_get_task_weight(const mtl_ctx_t* ctx, uint32_t task_id);

/**
 * @brief Set task weight manually
 *
 * @param ctx MTL context
 * @param task_id Task ID
 * @param weight New weight
 * @return 0 on success, negative on error
 */
int mtl_set_task_weight(mtl_ctx_t* ctx, uint32_t task_id, float weight);

//=============================================================================
// Training API
//=============================================================================

/**
 * @brief Compute multi-task loss
 *
 * WHAT: Compute weighted sum of task losses
 * WHY:  Main training objective for MTL
 * HOW:  Forward each task, weight losses, sum
 *
 * @param ctx MTL context
 * @param batch Multi-task batch
 * @param predictions Predictions per task [num_tasks]
 * @param total_loss Output total loss
 * @param task_losses Output individual task losses (optional)
 * @return 0 on success, negative on error
 */
int mtl_compute_loss(
    mtl_ctx_t* ctx,
    const mtl_batch_t* batch,
    nimcp_tensor_t** predictions,
    float* total_loss,
    float* task_losses
);

/**
 * @brief Process gradients for MTL
 *
 * WHAT: Apply gradient modifications (PCGrad, etc.)
 * WHY:  Handle gradient conflicts between tasks
 * HOW:  Project/drop conflicting gradients
 *
 * @param ctx MTL context
 * @param gradients Gradients per task [num_tasks][num_params]
 * @param num_params Number of parameters
 * @param combined_gradient Output combined gradient
 * @return 0 on success, negative on error
 */
int mtl_process_gradients(
    mtl_ctx_t* ctx,
    float** gradients,
    size_t num_params,
    float* combined_gradient
);

/**
 * @brief Update task weights (for learned weighting)
 *
 * @param ctx MTL context
 * @param task_losses Current task losses
 * @param task_grad_norms Task gradient norms (for GradNorm)
 * @return 0 on success, negative on error
 */
int mtl_update_weights(
    mtl_ctx_t* ctx,
    const float* task_losses,
    const float* task_grad_norms
);

/**
 * @brief Sample next task for training
 *
 * @param ctx MTL context
 * @return Task ID to train next
 */
uint32_t mtl_sample_task(mtl_ctx_t* ctx);

/**
 * @brief Sample batch of tasks
 *
 * @param ctx MTL context
 * @param num_tasks Number of tasks to sample
 * @param task_ids Output task IDs
 * @return Number of tasks sampled
 */
uint32_t mtl_sample_tasks(
    mtl_ctx_t* ctx,
    uint32_t num_tasks,
    uint32_t* task_ids
);

//=============================================================================
// Gradient Conflict API
//=============================================================================

/**
 * @brief Compute gradient similarity
 *
 * @param grad1 First gradient
 * @param grad2 Second gradient
 * @param num_params Number of parameters
 * @return Cosine similarity [-1, 1]
 */
float mtl_gradient_similarity(
    const float* grad1,
    const float* grad2,
    size_t num_params
);

/**
 * @brief Check if gradients conflict
 *
 * @param grad1 First gradient
 * @param grad2 Second gradient
 * @param num_params Number of parameters
 * @return true if cosine similarity < 0
 */
bool mtl_gradients_conflict(
    const float* grad1,
    const float* grad2,
    size_t num_params
);

/**
 * @brief Apply PCGrad projection
 *
 * @param ctx MTL context
 * @param main_grad Main task gradient (modified in place)
 * @param other_grads Other task gradients
 * @param num_other Number of other gradients
 * @param num_params Number of parameters
 * @return 0 on success, negative on error
 */
int mtl_pcgrad_project(
    mtl_ctx_t* ctx,
    float* main_grad,
    float** other_grads,
    uint32_t num_other,
    size_t num_params
);

/**
 * @brief Apply CAGrad update
 *
 * @param ctx MTL context
 * @param gradients All task gradients [num_tasks][num_params]
 * @param num_tasks Number of tasks
 * @param num_params Number of parameters
 * @param combined Output combined gradient
 * @return 0 on success, negative on error
 */
int mtl_cagrad_combine(
    mtl_ctx_t* ctx,
    float** gradients,
    uint32_t num_tasks,
    size_t num_params,
    float* combined
);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Connect to gradient manager
 *
 * @param ctx MTL context
 * @param grad_manager Gradient manager
 * @return 0 on success, negative on error
 */
int mtl_connect_gradient_manager(
    mtl_ctx_t* ctx,
    nimcp_gradient_manager_ctx_t* grad_manager
);

/**
 * @brief Connect to brain factory
 *
 * BIOLOGICAL BASIS:
 * - Map tasks to brain regions
 * - Shared processing in early visual cortex
 * - Task-specific processing in higher areas
 *
 * @param ctx MTL context
 * @param brain_factory Brain factory
 * @return 0 on success, negative on error
 */
int mtl_connect_brain_factory(mtl_ctx_t* ctx, void* brain_factory);

/**
 * @brief Connect to thalamic router
 *
 * BIOLOGICAL BASIS:
 * - Thalamus gates task-relevant information
 * - Attention modulates task processing
 *
 * @param ctx MTL context
 * @param router Thalamic router
 * @return 0 on success, negative on error
 */
int mtl_connect_thalamic_router(mtl_ctx_t* ctx, void* router);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get MTL statistics
 *
 * @param ctx MTL context
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int mtl_get_stats(const mtl_ctx_t* ctx, mtl_stats_t* stats);

/**
 * @brief Reset MTL statistics
 *
 * @param ctx MTL context
 */
void mtl_reset_stats(mtl_ctx_t* ctx);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get architecture name
 */
const char* mtl_architecture_name(mtl_architecture_t arch);

/**
 * @brief Get weighting strategy name
 */
const char* mtl_weight_strategy_name(mtl_weight_strategy_t strategy);

/**
 * @brief Get gradient method name
 */
const char* mtl_gradient_method_name(mtl_gradient_method_t method);

/**
 * @brief Validate MTL configuration
 */
int mtl_validate_config(const mtl_config_t* config);

/**
 * @brief Free MTL batch
 */
void mtl_free_batch(mtl_batch_t* batch);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MULTI_TASK_H */
