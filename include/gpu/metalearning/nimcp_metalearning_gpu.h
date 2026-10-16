/**
 * @file nimcp_metalearning_gpu.h
 * @brief GPU-accelerated Meta-Learning Kernels
 *
 * WHAT: CUDA kernels for meta-learning and few-shot learning
 * WHY:  GPU acceleration for rapid task adaptation
 * HOW:  Custom kernels for MAML, Reptile, ProtoNets, memory-based meta-learning
 *
 * ARCHITECTURE:
 * - MAML: Model-Agnostic Meta-Learning with second-order gradients
 * - Reptile: First-order approximation for efficiency
 * - ProtoNets: Prototypical networks for few-shot classification
 * - Memory-Augmented: External memory for fast adaptation
 * - Task Embedding: Learn task representations for transfer
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_METALEARNING_GPU_H
#define NIMCP_METALEARNING_GPU_H

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Meta-Learning Types
//=============================================================================

/**
 * @brief Meta-learning algorithm type
 */
typedef enum {
    NIMCP_META_MAML = 0,          /**< Model-Agnostic Meta-Learning */
    NIMCP_META_FOMAML = 1,        /**< First-Order MAML */
    NIMCP_META_REPTILE = 2,       /**< Reptile algorithm */
    NIMCP_META_PROTONET = 3,      /**< Prototypical Networks */
    NIMCP_META_MATCHING = 4,      /**< Matching Networks */
    NIMCP_META_RELATION = 5,      /**< Relation Networks */
    NIMCP_META_MEMORY = 6         /**< Memory-Augmented Meta-Learning */
} nimcp_meta_algorithm_t;

//=============================================================================
// MAML Parameters and Structures
//=============================================================================

/**
 * @brief MAML training parameters
 */
typedef struct {
    float inner_lr;               /**< Inner loop learning rate (alpha) */
    float outer_lr;               /**< Outer loop learning rate (beta) */
    int inner_steps;              /**< Number of inner gradient steps */
    int outer_steps;              /**< Number of outer optimization steps */
    bool second_order;            /**< Use second-order gradients */
    float clip_grad;              /**< Gradient clipping threshold */
    float weight_decay;           /**< L2 regularization */
    int num_tasks;                /**< Number of tasks per meta-batch */
    int k_shot;                   /**< K examples per class */
    int n_way;                    /**< N-way classification */
} nimcp_gpu_maml_params_t;

/**
 * @brief MAML state for meta-learning
 */
typedef struct {
    nimcp_gpu_tensor_t* meta_weights;     /**< Meta-learned initialization */
    nimcp_gpu_tensor_t* adapted_weights;  /**< Task-adapted weights */
    nimcp_gpu_tensor_t* inner_grads;      /**< Inner loop gradients */
    nimcp_gpu_tensor_t* outer_grads;      /**< Outer loop gradients */
    nimcp_gpu_tensor_t* hessian_prod;     /**< Hessian-vector product (for 2nd order) */
    nimcp_gpu_tensor_t* momentum;         /**< Optimizer momentum */
    size_t n_params;                      /**< Number of model parameters */
} nimcp_gpu_maml_state_t;

//=============================================================================
// Reptile Parameters
//=============================================================================

/**
 * @brief Reptile algorithm parameters
 */
typedef struct {
    float inner_lr;               /**< Inner loop learning rate */
    float outer_lr;               /**< Meta-update learning rate */
    int inner_steps;              /**< Number of inner SGD steps */
    int num_tasks;                /**< Tasks per meta-batch */
    float epsilon;                /**< Interpolation epsilon */
} nimcp_gpu_reptile_params_t;

//=============================================================================
// Prototypical Networks Parameters
//=============================================================================

/**
 * @brief ProtoNet parameters
 */
typedef struct {
    int embedding_dim;            /**< Embedding dimension */
    float temperature;            /**< Softmax temperature */
    int k_shot;                   /**< Support examples per class */
    int n_query;                  /**< Query examples per class */
    int n_way;                    /**< Number of classes */
    bool normalize_prototypes;    /**< L2 normalize prototypes */
    float margin;                 /**< Margin for triplet loss */
} nimcp_gpu_protonet_params_t;

/**
 * @brief ProtoNet state
 */
typedef struct {
    nimcp_gpu_tensor_t* support_embeddings;  /**< Support set embeddings */
    nimcp_gpu_tensor_t* query_embeddings;    /**< Query set embeddings */
    nimcp_gpu_tensor_t* prototypes;          /**< Class prototypes */
    nimcp_gpu_tensor_t* distances;           /**< Query-prototype distances */
    nimcp_gpu_tensor_t* logits;              /**< Classification logits */
    size_t n_classes;                        /**< Number of classes */
    size_t embedding_dim;                    /**< Embedding dimension */
} nimcp_gpu_protonet_state_t;

//=============================================================================
// Memory-Augmented Meta-Learning
//=============================================================================

/**
 * @brief Memory-augmented meta-learning parameters
 */
typedef struct {
    int memory_size;              /**< Number of memory slots */
    int key_dim;                  /**< Key dimension */
    int value_dim;                /**< Value dimension */
    float read_strength;          /**< Read head strength */
    float write_strength;         /**< Write head strength */
    float forget_rate;            /**< Memory decay rate */
    bool use_attention;           /**< Use attention-based read */
    float temperature;            /**< Attention temperature */
} nimcp_gpu_meta_memory_params_t;

/**
 * @brief Meta-learning memory state
 */
typedef struct {
    nimcp_gpu_tensor_t* keys;             /**< Memory keys */
    nimcp_gpu_tensor_t* values;           /**< Memory values */
    nimcp_gpu_tensor_t* usage;            /**< Memory usage counters */
    nimcp_gpu_tensor_t* read_weights;     /**< Read attention weights */
    nimcp_gpu_tensor_t* write_weights;    /**< Write attention weights */
    nimcp_gpu_tensor_t* read_output;      /**< Memory read output */
    size_t memory_size;                   /**< Number of slots */
    size_t key_dim;                       /**< Key dimension */
    size_t value_dim;                     /**< Value dimension */
} nimcp_gpu_meta_memory_state_t;

//=============================================================================
// Task Embedding
//=============================================================================

/**
 * @brief Task embedding parameters
 */
typedef struct {
    int task_embed_dim;           /**< Task embedding dimension */
    int context_size;             /**< Context examples for inference */
    float lr;                     /**< Learning rate */
    bool use_film;                /**< Use FiLM conditioning */
    bool use_task_net;            /**< Use task inference network */
} nimcp_gpu_task_embed_params_t;

/**
 * @brief Task embedding state
 */
typedef struct {
    nimcp_gpu_tensor_t* task_embedding;   /**< Current task embedding */
    nimcp_gpu_tensor_t* context_encoder;  /**< Context encoding */
    nimcp_gpu_tensor_t* film_gamma;       /**< FiLM scale parameters */
    nimcp_gpu_tensor_t* film_beta;        /**< FiLM shift parameters */
    size_t embed_dim;                     /**< Embedding dimension */
} nimcp_gpu_task_embed_state_t;

//=============================================================================
// Default Parameter Functions
//=============================================================================

NIMCP_EXPORT nimcp_gpu_maml_params_t nimcp_gpu_maml_params_default(void);
NIMCP_EXPORT nimcp_gpu_reptile_params_t nimcp_gpu_reptile_params_default(void);
NIMCP_EXPORT nimcp_gpu_protonet_params_t nimcp_gpu_protonet_params_default(void);
NIMCP_EXPORT nimcp_gpu_meta_memory_params_t nimcp_gpu_meta_memory_params_default(void);
NIMCP_EXPORT nimcp_gpu_task_embed_params_t nimcp_gpu_task_embed_params_default(void);

//=============================================================================
// MAML Functions
//=============================================================================

/**
 * @brief Perform inner loop adaptation for MAML
 */
NIMCP_EXPORT bool nimcp_gpu_maml_inner_loop(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_maml_state_t* state,
    const nimcp_gpu_tensor_t* support_x,
    const nimcp_gpu_tensor_t* support_y,
    const nimcp_gpu_maml_params_t* params);

/**
 * @brief Compute outer loop meta-gradient
 */
NIMCP_EXPORT bool nimcp_gpu_maml_outer_gradient(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_maml_state_t* state,
    const nimcp_gpu_tensor_t* query_x,
    const nimcp_gpu_tensor_t* query_y,
    const nimcp_gpu_maml_params_t* params);

/**
 * @brief Update meta-parameters with outer gradient
 */
NIMCP_EXPORT bool nimcp_gpu_maml_meta_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_maml_state_t* state,
    const nimcp_gpu_maml_params_t* params);

/**
 * @brief Complete MAML step (inner + outer)
 */
NIMCP_EXPORT bool nimcp_gpu_maml_step(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_maml_state_t* state,
    const nimcp_gpu_tensor_t* support_x,
    const nimcp_gpu_tensor_t* support_y,
    const nimcp_gpu_tensor_t* query_x,
    const nimcp_gpu_tensor_t* query_y,
    const nimcp_gpu_maml_params_t* params);

/**
 * @brief Compute Hessian-vector product for second-order MAML
 */
NIMCP_EXPORT bool nimcp_gpu_maml_hessian_vector_product(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_maml_state_t* state,
    const nimcp_gpu_tensor_t* vector,
    nimcp_gpu_tensor_t* hvp_out,
    const nimcp_gpu_maml_params_t* params);

//=============================================================================
// Reptile Functions
//=============================================================================

/**
 * @brief Reptile inner loop training on task
 */
NIMCP_EXPORT bool nimcp_gpu_reptile_inner_loop(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* task_x,
    const nimcp_gpu_tensor_t* task_y,
    const nimcp_gpu_reptile_params_t* params);

/**
 * @brief Reptile meta-update (interpolation)
 */
NIMCP_EXPORT bool nimcp_gpu_reptile_meta_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* meta_weights,
    const nimcp_gpu_tensor_t* adapted_weights,
    const nimcp_gpu_reptile_params_t* params);

/**
 * @brief Complete Reptile step
 */
NIMCP_EXPORT bool nimcp_gpu_reptile_step(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* meta_weights,
    const nimcp_gpu_tensor_t* task_x,
    const nimcp_gpu_tensor_t* task_y,
    const nimcp_gpu_reptile_params_t* params);

//=============================================================================
// Prototypical Networks Functions
//=============================================================================

/**
 * @brief Compute class prototypes from support set
 */
NIMCP_EXPORT bool nimcp_gpu_protonet_compute_prototypes(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_protonet_state_t* state,
    const nimcp_gpu_tensor_t* support_embeddings,
    const nimcp_gpu_tensor_t* support_labels,
    const nimcp_gpu_protonet_params_t* params);

/**
 * @brief Classify queries based on prototype distances
 */
NIMCP_EXPORT bool nimcp_gpu_protonet_classify(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_protonet_state_t* state,
    const nimcp_gpu_tensor_t* query_embeddings,
    nimcp_gpu_tensor_t* predictions,
    const nimcp_gpu_protonet_params_t* params);

/**
 * @brief Compute ProtoNet loss
 */
NIMCP_EXPORT bool nimcp_gpu_protonet_loss(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_protonet_state_t* state,
    const nimcp_gpu_tensor_t* query_labels,
    float* loss_out,
    const nimcp_gpu_protonet_params_t* params);

/**
 * @brief Complete ProtoNet episode
 */
NIMCP_EXPORT bool nimcp_gpu_protonet_episode(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_protonet_state_t* state,
    const nimcp_gpu_tensor_t* support_embeddings,
    const nimcp_gpu_tensor_t* support_labels,
    const nimcp_gpu_tensor_t* query_embeddings,
    const nimcp_gpu_tensor_t* query_labels,
    float* loss_out,
    nimcp_gpu_tensor_t* predictions,
    const nimcp_gpu_protonet_params_t* params);

//=============================================================================
// Memory-Augmented Meta-Learning Functions
//=============================================================================

/**
 * @brief Read from meta-learning memory
 */
NIMCP_EXPORT bool nimcp_gpu_meta_memory_read(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_meta_memory_state_t* state,
    const nimcp_gpu_tensor_t* query_key,
    nimcp_gpu_tensor_t* read_output,
    const nimcp_gpu_meta_memory_params_t* params);

/**
 * @brief Write to meta-learning memory
 */
NIMCP_EXPORT bool nimcp_gpu_meta_memory_write(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_meta_memory_state_t* state,
    const nimcp_gpu_tensor_t* key,
    const nimcp_gpu_tensor_t* value,
    const nimcp_gpu_meta_memory_params_t* params);

/**
 * @brief Update memory usage and decay
 */
NIMCP_EXPORT bool nimcp_gpu_meta_memory_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_meta_memory_state_t* state,
    const nimcp_gpu_meta_memory_params_t* params);

/**
 * @brief Clear memory for new task
 */
NIMCP_EXPORT bool nimcp_gpu_meta_memory_reset(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_meta_memory_state_t* state);

//=============================================================================
// Task Embedding Functions
//=============================================================================

/**
 * @brief Infer task embedding from context examples
 */
NIMCP_EXPORT bool nimcp_gpu_task_embed_infer(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_task_embed_state_t* state,
    const nimcp_gpu_tensor_t* context_x,
    const nimcp_gpu_tensor_t* context_y,
    const nimcp_gpu_task_embed_params_t* params);

/**
 * @brief Apply task embedding via FiLM conditioning
 */
NIMCP_EXPORT bool nimcp_gpu_task_embed_film(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_task_embed_state_t* state,
    nimcp_gpu_tensor_t* activations,
    const nimcp_gpu_task_embed_params_t* params);

/**
 * @brief Compute task similarity
 */
NIMCP_EXPORT bool nimcp_gpu_task_embed_similarity(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* task_embed1,
    const nimcp_gpu_tensor_t* task_embed2,
    float* similarity_out);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Sample N-way K-shot episode from dataset
 */
NIMCP_EXPORT bool nimcp_gpu_sample_episode(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* data_x,
    const nimcp_gpu_tensor_t* data_y,
    nimcp_gpu_tensor_t* support_x,
    nimcp_gpu_tensor_t* support_y,
    nimcp_gpu_tensor_t* query_x,
    nimcp_gpu_tensor_t* query_y,
    int n_way,
    int k_shot,
    int n_query);

/**
 * @brief Compute few-shot accuracy
 */
NIMCP_EXPORT bool nimcp_gpu_few_shot_accuracy(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* predictions,
    const nimcp_gpu_tensor_t* labels,
    float* accuracy_out);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_METALEARNING_GPU_H
