/**
 * @file nimcp_meta_learning.h
 * @brief Phase 10.8: Meta-Learning - MAML and Few-Shot Learning
 *
 * WHAT: Model-Agnostic Meta-Learning (MAML) for rapid task adaptation
 * WHY:  Enable learning from few examples (1-shot, 5-shot, 10-shot)
 * HOW:  Learn-to-learn via gradient-based meta-optimization
 *
 * FEATURES:
 * - MAML (Model-Agnostic Meta-Learning) algorithm
 * - Few-shot learning (K=1,5,10 examples)
 * - Task similarity metrics for transfer learning
 * - Adaptive learning rates per brain region
 * - Meta-optimization for fast adaptation
 *
 * TRAINING IMPACT: MODERATE
 * - New meta-training mode (support/query sets)
 * - Inner/outer loop optimization
 * - Does NOT change base network architecture
 *
 * REFERENCE:
 * Finn et al. (2017) "Model-Agnostic Meta-Learning for Fast Adaptation"
 *
 * @author NIMCP Phase 10 Team
 * @date 2025-11-09
 */

#ifndef NIMCP_META_LEARNING_H
#define NIMCP_META_LEARNING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Dependencies
//=============================================================================

// Forward declare brain_t to avoid circular dependency
// NOTE: brain.h defines: typedef struct brain_struct* brain_t;
struct brain_struct;

// If brain_t hasn't been typedef'd yet (e.g., brain.h not included),
// create a compatible typedef. This allows meta_learning.h to be
// included standalone while remaining compatible with brain.h.
#ifndef NIMCP_BRAIN_H
typedef struct brain_struct* brain_t;
#endif

//=============================================================================
// Meta-Learning Configuration
//=============================================================================

/**
 * @brief Few-shot learning modes
 *
 * WHAT: Predefined K-shot configurations
 * WHY:  Standard benchmarks for meta-learning evaluation
 */
typedef enum {
    FEW_SHOT_1 = 1,    /**< 1-shot learning (hardest) */
    FEW_SHOT_5 = 5,    /**< 5-shot learning (standard) */
    FEW_SHOT_10 = 10   /**< 10-shot learning (easier) */
} few_shot_mode_t;

/**
 * @brief Meta-learning algorithm selection
 */
typedef enum {
    META_ALGORITHM_MAML,       /**< Model-Agnostic Meta-Learning */
    META_ALGORITHM_REPTILE,    /**< Reptile (simpler variant) */
    META_ALGORITHM_FOMAML      /**< First-Order MAML (faster) */
} meta_algorithm_t;

/**
 * @brief Meta-learning configuration
 *
 * WHAT: Hyperparameters for meta-training
 * WHY:  Control inner/outer loop optimization
 */
typedef struct {
    // Algorithm selection
    meta_algorithm_t algorithm;  /**< Which meta-learning algorithm */
    few_shot_mode_t few_shot_k;  /**< K examples per class */

    // Inner loop (task adaptation)
    float inner_learning_rate;   /**< Learning rate for task adaptation */
    uint32_t inner_steps;        /**< Gradient steps per task */

    // Outer loop (meta-optimization)
    float outer_learning_rate;   /**< Meta-learning rate */
    uint32_t outer_batch_size;   /**< Tasks per meta-update */

    // Transfer learning
    bool enable_task_similarity; /**< Compute task similarity metrics */
    bool enable_adaptive_lr;     /**< Adaptive LR per region */
    float similarity_threshold;  /**< Threshold for transfer (0.0-1.0) */

    // Performance tracking
    bool track_adaptation_speed; /**< Measure adaptation convergence */
    uint32_t max_adaptation_steps; /**< Max steps for adaptation */
} meta_learning_config_t;

//=============================================================================
// Meta-Learning State
//=============================================================================

/**
 * @brief Brain region types for adaptive learning rates
 *
 * WHAT: Anatomical brain regions with different plasticity
 * WHY:  Some regions (V1) are stable, others (PFC) are flexible
 */
typedef enum {
    META_REGION_SENSORY,     /**< V1, A1 (low plasticity) */
    META_REGION_ASSOCIATION, /**< IT, STS (medium plasticity) */
    META_REGION_PREFRONTAL,  /**< PFC, OFC (high plasticity) */
    META_REGION_COUNT        /**< Number of region types */
} meta_region_type_t;

/**
 * @brief Task representation for similarity computation
 *
 * WHAT: Task metadata for transfer learning
 * WHY:  Need to compare tasks to decide what to transfer
 */
typedef struct {
    char name[64];              /**< Task identifier */
    uint32_t num_classes;       /**< Output classes */
    uint32_t input_dim;         /**< Input dimensionality */
    float* class_prototypes;    /**< Class embeddings [num_classes × dim] */
    float average_loss;         /**< Task difficulty metric */
    uint32_t samples_seen;      /**< Training examples */
} meta_task_t;

/**
 * @brief Per-task adaptation statistics
 *
 * WHAT: Track how quickly brain adapts to new tasks
 * WHY:  Measure meta-learning effectiveness
 */
typedef struct {
    float initial_loss;         /**< Loss before adaptation */
    float final_loss;           /**< Loss after adaptation */
    uint32_t steps_to_converge; /**< Adaptation speed */
    float adaptation_gain;      /**< (initial - final) / initial */
} adaptation_stats_t;

/**
 * @brief Opaque meta-learner handle
 *
 * WHAT: Meta-learning state manager
 * WHY:  Encapsulate MAML internals, allow implementation changes
 * HOW:  Opaque pointer pattern (struct defined in .c file)
 */
typedef struct meta_learner_s* meta_learner_t;

//=============================================================================
// Meta-Learning API
//=============================================================================

/**
 * @brief Create meta-learner
 *
 * WHAT: Initialize MAML meta-learning system
 * WHY:  Enable few-shot learning and fast adaptation
 * HOW:  Allocate state, initialize learning rates per region
 *
 * @param config Meta-learning configuration (NULL for defaults)
 * @param num_regions Number of brain regions (for adaptive LR)
 * @return Meta-learner handle or NULL on error
 *
 * COMPLEXITY: O(num_regions)
 * MEMORY: O(num_regions + num_tasks_max)
 */
meta_learner_t meta_learner_create(const meta_learning_config_t* config,
                                   uint32_t num_regions);

/**
 * @brief Destroy meta-learner
 *
 * WHAT: Free meta-learning resources
 * WHY:  Prevent memory leaks
 * HOW:  Free all task history and learning rate state
 *
 * @param meta Meta-learner handle
 *
 * COMPLEXITY: O(num_tasks_seen)
 */
void meta_learner_destroy(meta_learner_t meta);

/**
 * @brief Get default meta-learning configuration
 *
 * WHAT: Sensible defaults for MAML
 * WHY:  Simplify initialization
 * HOW:  Return struct with proven hyperparameters
 *
 * @return Default configuration
 *
 * COMPLEXITY: O(1)
 */
meta_learning_config_t meta_learning_default_config(void);

//=============================================================================
// MAML Core Functions
//=============================================================================

/**
 * @brief Perform MAML inner loop (task adaptation)
 *
 * WHAT: Adapt brain to new task using K examples (support set)
 * WHY:  Core of few-shot learning - fast adaptation
 * HOW:
 * 1. Clone brain parameters (θ → θ')
 * 2. Compute gradients on support set
 * 3. Take K gradient steps (inner loop)
 * 4. Return adapted parameters
 *
 * @param meta Meta-learner handle
 * @param brain Brain to adapt
 * @param support_inputs Support set inputs [K × input_dim]
 * @param support_labels Support set labels [K]
 * @param num_support Number of support examples (K)
 * @param adapted_brain Output: adapted brain (caller must free)
 * @return true on success, false on error
 *
 * ALGORITHM:
 * θ' ← θ  (clone parameters)
 * for step in 1..inner_steps:
 *     L ← loss(θ', support_inputs, support_labels)
 *     θ' ← θ' - α * ∇_θ' L  (gradient step)
 * return θ'
 *
 * COMPLEXITY: O(inner_steps * network_forward)
 */
bool meta_maml_inner_loop(meta_learner_t meta, brain_t brain,
                          const float** support_inputs, const uint32_t* support_labels,
                          uint32_t num_support, brain_t* adapted_brain);

/**
 * @brief Perform MAML outer loop (meta-update)
 *
 * WHAT: Update meta-parameters for better adaptation
 * WHY:  Learn initialization that enables fast learning
 * HOW:
 * 1. Sample batch of tasks
 * 2. For each task: inner loop → query set loss
 * 3. Meta-gradient: ∂L_query/∂θ (not ∂L_query/∂θ')
 * 4. Update θ ← θ - β * meta_gradient
 *
 * @param meta Meta-learner handle
 * @param brain Brain with meta-parameters θ
 * @param tasks Array of tasks for meta-training
 * @param num_tasks Number of tasks in batch
 * @return true on success, false on error
 *
 * ALGORITHM:
 * meta_gradient ← 0
 * for each task in batch:
 *     θ' ← inner_loop(θ, task.support)
 *     L_query ← loss(θ', task.query)
 *     meta_gradient += ∂L_query/∂θ  (backprop through inner loop)
 * θ ← θ - β * meta_gradient / num_tasks
 *
 * COMPLEXITY: O(num_tasks * inner_steps * network_forward)
 */
bool meta_maml_outer_loop(meta_learner_t meta, brain_t brain,
                          meta_task_t** tasks, uint32_t num_tasks);

/**
 * @brief Evaluate adaptation performance
 *
 * WHAT: Measure how well brain adapts to new task
 * WHY:  Quantify meta-learning effectiveness
 * HOW:  Compare loss before/after adaptation
 *
 * @param meta Meta-learner handle
 * @param brain Original brain (θ)
 * @param adapted_brain Adapted brain (θ')
 * @param query_inputs Query set inputs [N × input_dim]
 * @param query_labels Query set labels [N]
 * @param num_query Number of query examples
 * @param stats Output: adaptation statistics
 * @return true on success, false on error
 *
 * COMPLEXITY: O(num_query * network_forward)
 */
bool meta_evaluate_adaptation(meta_learner_t meta, brain_t brain, brain_t adapted_brain,
                              const float** query_inputs, const uint32_t* query_labels,
                              uint32_t num_query, adaptation_stats_t* stats);

//=============================================================================
// Task Similarity & Transfer Learning
//=============================================================================

/**
 * @brief Compute task similarity
 *
 * WHAT: Measure how similar two tasks are
 * WHY:  Decide whether to transfer knowledge
 * HOW:  Compare class prototypes using cosine similarity
 *
 * @param meta Meta-learner handle
 * @param task_a First task
 * @param task_b Second task
 * @return Similarity score [0.0 = different, 1.0 = identical]
 *
 * ALGORITHM:
 * 1. Extract class prototypes (centroids)
 * 2. Compute pairwise cosine similarities
 * 3. Average similarity across classes
 *
 * COMPLEXITY: O(num_classes * embedding_dim)
 */
float meta_compute_task_similarity(meta_learner_t meta,
                                   const meta_task_t* task_a,
                                   const meta_task_t* task_b);

/**
 * @brief Transfer knowledge between brains
 *
 * WHAT: Copy learned features from source to target
 * WHY:  Accelerate learning on similar tasks
 * HOW:
 * 1. Compute task similarity
 * 2. If similar enough, copy sensory/association weights
 * 3. Keep PFC random (task-specific)
 *
 * @param meta Meta-learner handle
 * @param source_brain Brain trained on source task
 * @param target_brain Brain to initialize for target task
 * @param similarity Computed similarity (or -1 to compute)
 * @return true if transfer applied, false if tasks too dissimilar
 *
 * COMPLEXITY: O(network_size)
 */
bool meta_transfer_knowledge(meta_learner_t meta,
                             brain_t source_brain,
                             brain_t target_brain,
                             float similarity);

//=============================================================================
// Adaptive Learning Rates
//=============================================================================

/**
 * @brief Get learning rate for brain region
 *
 * WHAT: Retrieve adaptive learning rate
 * WHY:  Different regions have different plasticity
 * HOW:  Return current LR for specified region
 *
 * @param meta Meta-learner handle
 * @param region Brain region type
 * @return Learning rate (> 0.0)
 *
 * COMPLEXITY: O(1)
 */
float meta_get_learning_rate(meta_learner_t meta, meta_region_type_t region);

/**
 * @brief Update learning rate based on performance
 *
 * WHAT: Adapt learning rate using loss feedback
 * WHY:  Optimize per-region plasticity
 * HOW:
 * - If loss decreasing: increase LR (bold driver)
 * - If loss increasing: decrease LR (backtrack)
 *
 * @param meta Meta-learner handle
 * @param region Brain region type
 * @param loss Current loss on task
 * @return Updated learning rate
 *
 * COMPLEXITY: O(1)
 */
float meta_adapt_learning_rate(meta_learner_t meta,
                               meta_region_type_t region,
                               float loss);

//=============================================================================
// Task Management
//=============================================================================

/**
 * @brief Create task representation
 *
 * WHAT: Allocate and initialize task metadata
 * WHY:  Track task characteristics for similarity
 * HOW:  Compute class prototypes from examples
 *
 * @param name Task identifier
 * @param num_classes Number of output classes
 * @param input_dim Input dimensionality
 * @return Task handle or NULL on error
 *
 * COMPLEXITY: O(num_classes * input_dim)
 */
meta_task_t* meta_task_create(const char* name, uint32_t num_classes, uint32_t input_dim);

/**
 * @brief Destroy task representation
 *
 * @param task Task to destroy
 *
 * COMPLEXITY: O(1)
 */
void meta_task_destroy(meta_task_t* task);

/**
 * @brief Update task prototypes with new examples
 *
 * WHAT: Incrementally update class centroids
 * WHY:  Refine task representation as more data arrives
 * HOW:  Running average of class embeddings
 *
 * @param task Task to update
 * @param inputs Training examples [N × input_dim]
 * @param labels Class labels [N]
 * @param num_examples Number of examples
 * @return true on success
 *
 * COMPLEXITY: O(num_examples * input_dim)
 */
bool meta_task_update_prototypes(meta_task_t* task,
                                 const float** inputs,
                                 const uint32_t* labels,
                                 uint32_t num_examples);

//=============================================================================
// Statistics & Debugging
//=============================================================================

/**
 * @brief Get meta-learning statistics
 *
 * WHAT: Retrieve performance metrics
 * WHY:  Monitor meta-learning progress
 *
 * @param meta Meta-learner handle
 * @param num_tasks_seen Output: total tasks trained
 * @param avg_adaptation_gain Output: average improvement from adaptation
 * @param avg_steps_to_converge Output: average adaptation speed
 * @return true on success
 *
 * COMPLEXITY: O(1)
 */
bool meta_get_statistics(meta_learner_t meta,
                        uint32_t* num_tasks_seen,
                        float* avg_adaptation_gain,
                        float* avg_steps_to_converge);

/**
 * @brief Print meta-learning state
 *
 * WHAT: Debug output of meta-learner state
 * WHY:  Debugging and monitoring
 *
 * @param meta Meta-learner handle
 *
 * COMPLEXITY: O(num_regions + num_tasks)
 */
void meta_print_state(meta_learner_t meta);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_META_LEARNING_H
