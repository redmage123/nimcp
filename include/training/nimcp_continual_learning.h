/**
 * @file nimcp_continual_learning.h
 * @brief Continual/Lifelong Learning for NIMCP
 *
 * WHAT: Learn new tasks without forgetting previous tasks
 * WHY:  Real-world systems must adapt continuously without catastrophic forgetting
 * HOW:  EWC, PackNet, Progressive Networks, experience replay
 *
 * FRAMEWORK COMPARISON:
 * - PyTorch: Avalanche, continuum, learn2learn
 * - JAX: Manual implementation of EWC/MAS
 * - TensorFlow: continual-learning library
 *
 * NIMCP APPROACH:
 * - Deep integration with memory consolidation systems
 * - Bio-inspired via hippocampal replay and synaptic consolidation
 * - Leverages brain immune system for stability
 *
 * BIOLOGICAL GROUNDING:
 * - Synaptic consolidation: Important synapses stabilized over time
 * - Hippocampal replay: Interleaved replay of old memories during sleep
 * - Complementary learning systems: Fast hippocampus + slow neocortex
 * - Synaptic tagging and capture: Mark synapses for consolidation
 *
 * INTEGRATION POINTS:
 * - memory_consolidation: Sleep-based replay
 * - brain_immune: Stability mechanisms
 * - training_module: Task-specific training
 * - gradient_manager: Gradient modification for EWC
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#ifndef NIMCP_CONTINUAL_LEARNING_H
#define NIMCP_CONTINUAL_LEARNING_H

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

#define CL_MAX_TASKS                  256      /**< Maximum tasks */
#define CL_DEFAULT_EWC_LAMBDA         400.0f   /**< Default EWC importance */
#define CL_DEFAULT_MAS_LAMBDA         100.0f   /**< Default MAS importance */
#define CL_DEFAULT_REPLAY_RATIO       0.1f     /**< Default replay ratio */
#define CL_DEFAULT_BUFFER_SIZE        5000     /**< Default replay buffer size */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Continual learning strategy
 *
 * COMPARISON:
 * - EWC (Kirkpatrick 2017): Elastic Weight Consolidation
 * - MAS (Aljundi 2018): Memory Aware Synapses
 * - PackNet (Mallya 2018): Iterative pruning and re-training
 * - Progressive (Rusu 2016): Lateral connections, no forgetting
 */
typedef enum {
    CL_STRATEGY_NAIVE = 0,           /**< Naive fine-tuning (baseline) */
    CL_STRATEGY_EWC,                 /**< Elastic Weight Consolidation */
    CL_STRATEGY_EWC_ONLINE,          /**< Online EWC (running Fisher) */
    CL_STRATEGY_MAS,                 /**< Memory Aware Synapses */
    CL_STRATEGY_SI,                  /**< Synaptic Intelligence */
    CL_STRATEGY_LWF,                 /**< Learning Without Forgetting */
    CL_STRATEGY_PACKNET,             /**< PackNet (pruning-based) */
    CL_STRATEGY_PROGRESSIVE,         /**< Progressive Neural Networks */
    CL_STRATEGY_HAT,                 /**< Hard Attention to Task */
    CL_STRATEGY_GEM,                 /**< Gradient Episodic Memory */
    CL_STRATEGY_AGEM,                /**< Averaged GEM */
    CL_STRATEGY_REPLAY,              /**< Experience Replay */
    CL_STRATEGY_GENERATIVE_REPLAY,   /**< Generative Replay (GAN) */
    CL_STRATEGY_HYBRID,              /**< Combine multiple strategies */
    CL_STRATEGY_COUNT
} cl_strategy_t;

/**
 * @brief Memory importance estimation method
 *
 * BIOLOGICAL BASIS:
 * - Fisher diagonal ≈ synaptic importance
 * - MAS ≈ gradient sensitivity
 * - Path integral ≈ synaptic activity history
 */
typedef enum {
    CL_IMPORTANCE_FISHER = 0,        /**< Fisher information diagonal */
    CL_IMPORTANCE_MAS_OMEGA,         /**< MAS omega (gradient magnitude) */
    CL_IMPORTANCE_PATH_INTEGRAL,     /**< SI path integral */
    CL_IMPORTANCE_HYBRID,            /**< Combine methods */
    CL_IMPORTANCE_COUNT
} cl_importance_method_t;

/**
 * @brief Experience replay strategy
 *
 * BIOLOGICAL BASIS:
 * - Random replay ≈ random hippocampal activation
 * - Prioritized ≈ salient memory replay
 * - Generative ≈ memory reconstruction from gist
 */
typedef enum {
    CL_REPLAY_NONE = 0,              /**< No replay */
    CL_REPLAY_RANDOM,                /**< Random sampling */
    CL_REPLAY_RESERVOIR,             /**< Reservoir sampling */
    CL_REPLAY_PRIORITIZED,           /**< Priority based on loss */
    CL_REPLAY_HERDING,               /**< Class-balanced herding */
    CL_REPLAY_GENERATIVE,            /**< GAN-based generation */
    CL_REPLAY_COUNT
} cl_replay_strategy_t;

/**
 * @brief Task boundary detection
 */
typedef enum {
    CL_BOUNDARY_KNOWN = 0,           /**< Task boundaries are provided */
    CL_BOUNDARY_DETECT_LOSS,         /**< Detect via loss spike */
    CL_BOUNDARY_DETECT_GRADIENT,     /**< Detect via gradient change */
    CL_BOUNDARY_CONTINUOUS,          /**< No discrete boundaries */
    CL_BOUNDARY_COUNT
} cl_boundary_detection_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief EWC configuration
 *
 * BIOLOGICAL BASIS:
 * - lambda controls synaptic consolidation strength
 * - Fisher ≈ synaptic importance for task performance
 */
typedef struct {
    float lambda;                    /**< Consolidation strength */
    float fisher_damping;            /**< Fisher matrix damping */
    uint32_t fisher_samples;         /**< Samples for Fisher estimation */
    bool normalize_fisher;           /**< Normalize Fisher per parameter */
    bool keep_all_fishers;           /**< Keep Fisher for all tasks */
    bool online_update;              /**< Online Fisher update */
    float online_gamma;              /**< Online decay factor */
} cl_ewc_config_t;

/**
 * @brief MAS configuration
 */
typedef struct {
    float lambda;                    /**< Importance weight */
    uint32_t omega_samples;          /**< Samples for omega estimation */
    float omega_decay;               /**< Omega decay for online update */
} cl_mas_config_t;

/**
 * @brief Synaptic Intelligence configuration
 */
typedef struct {
    float c;                         /**< Scaling coefficient */
    float epsilon;                   /**< Damping for numerical stability */
    float dampening;                 /**< Path integral dampening */
} cl_si_config_t;

/**
 * @brief Learning Without Forgetting configuration
 */
typedef struct {
    float temperature;               /**< Distillation temperature */
    float alpha;                     /**< Distillation weight */
    bool use_hard_labels;            /**< Also use hard labels */
} cl_lwf_config_t;

/**
 * @brief PackNet configuration
 */
typedef struct {
    float prune_ratio;               /**< Fraction to prune per task */
    bool retrain_after_prune;        /**< Retrain after pruning */
    uint32_t retrain_epochs;         /**< Retrain epochs */
} cl_packnet_config_t;

/**
 * @brief GEM configuration
 */
typedef struct {
    uint32_t memory_size;            /**< Memory per task */
    float margin;                    /**< Gradient constraint margin */
    bool use_agem;                   /**< Use averaged GEM */
} cl_gem_config_t;

/**
 * @brief Experience replay configuration
 */
typedef struct {
    cl_replay_strategy_t strategy;   /**< Replay strategy */
    uint32_t buffer_size;            /**< Total buffer size */
    uint32_t samples_per_task;       /**< Samples stored per task */
    float replay_ratio;              /**< Ratio of replay to new samples */
    bool class_balanced;             /**< Balance classes in buffer */
} cl_replay_config_t;

/**
 * @brief Complete continual learning configuration
 */
typedef struct {
    cl_strategy_t strategy;          /**< Primary CL strategy */

    /* Strategy-specific configs */
    cl_ewc_config_t ewc;
    cl_mas_config_t mas;
    cl_si_config_t si;
    cl_lwf_config_t lwf;
    cl_packnet_config_t packnet;
    cl_gem_config_t gem;
    cl_replay_config_t replay;

    /* Task management */
    cl_boundary_detection_t boundary_detection;
    bool auto_task_increment;        /**< Auto-increment task ID */
    uint32_t max_tasks;              /**< Maximum tasks */

    /* Multi-head output */
    bool task_specific_heads;        /**< Use task-specific output heads */
    bool expandable_output;          /**< Expand output for new classes */

    /* Integration */
    bool integrate_gradient_manager; /**< Modify gradients via GM */
    bool integrate_memory_consolidation; /**< Use sleep replay */
    bool integrate_brain_immune;     /**< Use stability mechanisms */

    /* Evaluation */
    bool track_forgetting;           /**< Track backward transfer */
    bool track_forward_transfer;     /**< Track forward transfer */

    /* Debugging */
    bool verbose;
    bool track_statistics;
} cl_config_t;

//=============================================================================
// Task Structures
//=============================================================================

/**
 * @brief Continual learning task
 */
typedef struct {
    uint32_t task_id;                /**< Task identifier */
    const char* name;                /**< Task name */

    /* Task data (optional, for replay) */
    nimcp_tensor_t* train_data;      /**< Training data */
    nimcp_tensor_t* train_labels;    /**< Training labels */
    nimcp_tensor_t* val_data;        /**< Validation data */
    nimcp_tensor_t* val_labels;      /**< Validation labels */

    /* Task metadata */
    uint32_t num_classes;            /**< Number of classes */
    uint32_t* class_ids;             /**< Class IDs for this task */
    bool active;                     /**< Currently being trained */
} cl_task_t;

/**
 * @brief Parameter importance information
 */
typedef struct {
    float* importance;               /**< Per-parameter importance */
    float* reference_params;         /**< Reference parameters */
    uint32_t task_id;                /**< Associated task ID */
    size_t num_params;               /**< Number of parameters */
} cl_importance_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Continual learning statistics
 */
typedef struct {
    uint64_t total_steps;            /**< Total training steps */
    uint32_t tasks_learned;          /**< Number of tasks learned */
    uint32_t current_task;           /**< Current task ID */

    /* Accuracy tracking */
    float* task_accuracies;          /**< Accuracy per task */
    float avg_accuracy;              /**< Average accuracy across tasks */
    float final_avg_accuracy;        /**< Final average accuracy */

    /* Forgetting metrics */
    float* forgetting;               /**< Forgetting per task */
    float avg_forgetting;            /**< Average forgetting */
    float backward_transfer;         /**< Backward transfer metric */
    float forward_transfer;          /**< Forward transfer metric */

    /* Regularization */
    float ewc_penalty;               /**< Current EWC penalty */
    float mas_penalty;               /**< Current MAS penalty */
    float si_penalty;                /**< Current SI penalty */

    /* Replay statistics */
    uint64_t replay_samples;         /**< Replay samples used */
    float replay_loss;               /**< Average replay loss */
} cl_stats_t;

//=============================================================================
// Opaque Types
//=============================================================================

/**
 * @brief Continual learning context (opaque)
 */
typedef struct cl_ctx_s cl_ctx_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default CL configuration
 *
 * DEFAULTS:
 * - EWC strategy
 * - lambda = 400
 * - Fisher samples = 1000
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
int cl_default_config(cl_config_t* config);

/**
 * @brief Create CL context
 *
 * @param config CL configuration
 * @return CL context or NULL on failure
 */
cl_ctx_t* cl_create(const cl_config_t* config);

/**
 * @brief Destroy CL context
 *
 * @param ctx Context to destroy (NULL-safe)
 */
void cl_destroy(cl_ctx_t* ctx);

/**
 * @brief Register model parameters for CL
 *
 * @param ctx CL context
 * @param params Parameter tensors
 * @param num_params Number of parameter tensors
 * @return 0 on success, negative on error
 */
int cl_register_params(
    cl_ctx_t* ctx,
    nimcp_tensor_t** params,
    uint32_t num_params
);

//=============================================================================
// Task Management API
//=============================================================================

/**
 * @brief Start new task
 *
 * WHAT: Begin training on new task
 * WHY:  Initialize task-specific tracking
 * HOW:  Create task entry, initialize importance if needed
 *
 * @param ctx CL context
 * @param task_id Task identifier
 * @param name Task name (optional)
 * @return 0 on success, negative on error
 */
int cl_start_task(cl_ctx_t* ctx, uint32_t task_id, const char* name);

/**
 * @brief End current task
 *
 * WHAT: Finish training on current task
 * WHY:  Compute importance, update consolidation
 * HOW:  Estimate Fisher/omega, store reference params
 *
 * BIOLOGICAL BASIS:
 * - End of task ≈ sleep consolidation
 * - Fisher estimation ≈ synaptic tagging
 *
 * @param ctx CL context
 * @return 0 on success, negative on error
 */
int cl_end_task(cl_ctx_t* ctx);

/**
 * @brief Get current task ID
 *
 * @param ctx CL context
 * @return Current task ID or -1 if none
 */
int cl_get_current_task(const cl_ctx_t* ctx);

/**
 * @brief Get number of tasks learned
 *
 * @param ctx CL context
 * @return Number of tasks
 */
uint32_t cl_get_num_tasks(const cl_ctx_t* ctx);

//=============================================================================
// Importance Estimation API
//=============================================================================

/**
 * @brief Compute Fisher information diagonal
 *
 * WHAT: Estimate parameter importance using Fisher information
 * WHY:  Identify critical parameters for task
 * HOW:  Average squared gradients over samples
 *
 * @param ctx CL context
 * @param data Training data for estimation
 * @param labels Training labels
 * @param forward_fn Model forward function
 * @param model Model
 * @return 0 on success, negative on error
 */
int cl_compute_fisher(
    cl_ctx_t* ctx,
    const nimcp_tensor_t* data,
    const nimcp_tensor_t* labels,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model
);

/**
 * @brief Compute MAS omega (importance)
 *
 * @param ctx CL context
 * @param data Training data
 * @param forward_fn Model forward function
 * @param model Model
 * @return 0 on success, negative on error
 */
int cl_compute_omega(
    cl_ctx_t* ctx,
    const nimcp_tensor_t* data,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model
);

/**
 * @brief Update SI path integral
 *
 * @param ctx CL context
 * @param gradients Current gradients
 * @param learning_rate Current learning rate
 * @return 0 on success, negative on error
 */
int cl_update_path_integral(
    cl_ctx_t* ctx,
    const float* gradients,
    float learning_rate
);

/**
 * @brief Get importance for specific task
 *
 * @param ctx CL context
 * @param task_id Task ID
 * @param importance Output importance structure
 * @return 0 on success, negative on error
 */
int cl_get_importance(
    cl_ctx_t* ctx,
    uint32_t task_id,
    cl_importance_t* importance
);

//=============================================================================
// Regularization API
//=============================================================================

/**
 * @brief Compute EWC regularization loss
 *
 * WHAT: Compute penalty for deviating from important parameters
 * WHY:  Prevent catastrophic forgetting
 * HOW:  L_ewc = sum(F_i * (theta_i - theta_ref_i)^2)
 *
 * @param ctx CL context
 * @param current_params Current parameter values
 * @param num_params Number of parameters
 * @return EWC penalty value
 */
float cl_ewc_penalty(
    cl_ctx_t* ctx,
    const float* current_params,
    size_t num_params
);

/**
 * @brief Compute MAS regularization loss
 *
 * @param ctx CL context
 * @param current_params Current parameter values
 * @param num_params Number of parameters
 * @return MAS penalty value
 */
float cl_mas_penalty(
    cl_ctx_t* ctx,
    const float* current_params,
    size_t num_params
);

/**
 * @brief Compute SI regularization loss
 *
 * @param ctx CL context
 * @param current_params Current parameter values
 * @param num_params Number of parameters
 * @return SI penalty value
 */
float cl_si_penalty(
    cl_ctx_t* ctx,
    const float* current_params,
    size_t num_params
);

/**
 * @brief Compute combined regularization penalty
 *
 * @param ctx CL context
 * @param current_params Current parameters
 * @param num_params Number of parameters
 * @return Combined penalty
 */
float cl_compute_penalty(
    cl_ctx_t* ctx,
    const float* current_params,
    size_t num_params
);

/**
 * @brief Modify gradients for GEM constraint
 *
 * WHAT: Project gradients to avoid harming previous tasks
 * WHY:  Gradient episodic memory approach
 *
 * @param ctx CL context
 * @param gradients Gradients (modified in place)
 * @param num_params Number of parameters
 * @return 0 on success, negative on error
 */
int cl_gem_project_gradients(
    cl_ctx_t* ctx,
    float* gradients,
    size_t num_params
);

//=============================================================================
// Experience Replay API
//=============================================================================

/**
 * @brief Add sample to replay buffer
 *
 * @param ctx CL context
 * @param input Input tensor
 * @param label Label tensor
 * @param task_id Associated task ID
 * @return 0 on success, negative on error
 */
int cl_replay_add(
    cl_ctx_t* ctx,
    const nimcp_tensor_t* input,
    const nimcp_tensor_t* label,
    uint32_t task_id
);

/**
 * @brief Sample from replay buffer
 *
 * @param ctx CL context
 * @param batch_size Samples to retrieve
 * @param inputs Output input batch
 * @param labels Output label batch
 * @param task_ids Output task IDs
 * @return Number of samples retrieved
 */
int cl_replay_sample(
    cl_ctx_t* ctx,
    uint32_t batch_size,
    nimcp_tensor_t** inputs,
    nimcp_tensor_t** labels,
    uint32_t** task_ids
);

/**
 * @brief Get replay buffer size
 *
 * @param ctx CL context
 * @return Number of samples in buffer
 */
uint32_t cl_replay_buffer_size(const cl_ctx_t* ctx);

/**
 * @brief Update priority of a replay buffer entry
 *
 * Used in prioritized experience replay to update priorities based on
 * TD-error or loss. Higher errors = higher priorities for more frequent
 * sampling of difficult examples.
 *
 * @param ctx CL context
 * @param idx Entry index in replay buffer
 * @param new_priority New priority value (should be positive)
 * @return 0 on success, negative on error
 */
int cl_update_replay_priority(cl_ctx_t* ctx, uint32_t idx, float new_priority);

/**
 * @brief Normalize all priorities in the replay buffer
 *
 * Normalizes priorities so they sum to 1.0, forming a valid probability
 * distribution for sampling. Should be called periodically, especially
 * after many priority updates.
 *
 * BIOLOGICAL BASIS:
 * - Importance-weighted memory consolidation
 * - Salient memories replayed more frequently
 *
 * @param ctx CL context
 * @return 0 on success, negative on error
 */
int cl_normalize_replay_priorities(cl_ctx_t* ctx);

//=============================================================================
// Evaluation API
//=============================================================================

/**
 * @brief Evaluate on all tasks
 *
 * @param ctx CL context
 * @param forward_fn Model forward function
 * @param model Model to evaluate
 * @param accuracies Output accuracy per task
 * @return Average accuracy
 */
float cl_evaluate_all_tasks(
    cl_ctx_t* ctx,
    nimcp_tensor_t* (*forward_fn)(void* model, const nimcp_tensor_t* input),
    void* model,
    float* accuracies
);

/**
 * @brief Compute forgetting metric
 *
 * @param ctx CL context
 * @param current_accuracies Current accuracies per task
 * @param forgetting Output forgetting per task
 * @return Average forgetting
 */
float cl_compute_forgetting(
    cl_ctx_t* ctx,
    const float* current_accuracies,
    float* forgetting
);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Connect to gradient manager
 *
 * @param ctx CL context
 * @param grad_manager Gradient manager
 * @return 0 on success, negative on error
 */
int cl_connect_gradient_manager(
    cl_ctx_t* ctx,
    nimcp_gradient_manager_ctx_t* grad_manager
);

/**
 * @brief Connect to memory consolidation system
 *
 * BIOLOGICAL BASIS:
 * - Hippocampal-cortical consolidation during sleep
 * - Memory replay for gradual transfer
 *
 * @param ctx CL context
 * @param memory_consolidation Memory consolidation module
 * @return 0 on success, negative on error
 */
int cl_connect_memory_consolidation(cl_ctx_t* ctx, void* memory_consolidation);

/**
 * @brief Connect to brain immune system
 *
 * BIOLOGICAL BASIS:
 * - Stability mechanisms prevent runaway plasticity
 * - Homeostatic regulation maintains network function
 *
 * @param ctx CL context
 * @param brain_immune Brain immune system
 * @return 0 on success, negative on error
 */
int cl_connect_brain_immune(cl_ctx_t* ctx, void* brain_immune);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get CL statistics
 *
 * @param ctx CL context
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int cl_get_stats(const cl_ctx_t* ctx, cl_stats_t* stats);

/**
 * @brief Reset CL statistics
 *
 * @param ctx CL context
 */
void cl_reset_stats(cl_ctx_t* ctx);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get strategy name
 */
const char* cl_strategy_name(cl_strategy_t strategy);

/**
 * @brief Validate CL configuration
 */
int cl_validate_config(const cl_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CONTINUAL_LEARNING_H */
