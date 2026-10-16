/**
 * @file nimcp_memory_consolidation_gpu.h
 * @brief GPU-accelerated Memory Consolidation Kernels
 *
 * WHAT: CUDA kernels for memory consolidation operations
 * WHY:  GPU acceleration for hippocampal replay, systems consolidation,
 *       and memory engram operations enabling massive parallelism
 * HOW:  Custom kernels for replay, consolidation, and similarity search
 *
 * ARCHITECTURE:
 * - Hippocampal replay (parallel pattern reactivation)
 * - Systems consolidation (hippocampus to cortex transfer)
 * - Memory engram updates (weight modifications)
 * - Episodic-semantic transfer
 * - Memory indexing and retrieval
 * - Similarity-based memory search
 *
 * PARALLELIZATION STRATEGY:
 * - Parallelize across memory traces
 * - Parallelize across engrams
 * - Batch processing for replay events
 * - Coalesced memory access patterns
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_MEMORY_CONSOLIDATION_GPU_H
#define NIMCP_MEMORY_CONSOLIDATION_GPU_H

#include "gpu/context/nimcp_gpu_context.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// Constants
//=============================================================================

#define MEMORY_GPU_MAX_ENGRAM_NEURONS       256     /**< Max neurons per engram */
#define MEMORY_GPU_MAX_FEATURE_DIM          128     /**< Max semantic feature dim */
#define MEMORY_GPU_MAX_NEIGHBORS            32      /**< Max cortical neighbors */
#define MEMORY_GPU_DEFAULT_BATCH_SIZE       256     /**< Default batch processing */

//=============================================================================
// Memory Consolidation Parameters
//=============================================================================

/**
 * @brief Parameters for hippocampal replay
 */
typedef struct {
    float replay_strength;      /**< Strength of replay activation [0.0-1.0] */
    float tau_decay;            /**< Decay time constant (ms) */
    float noise_stddev;         /**< Noise level for stochastic replay */
    bool compressed_replay;     /**< Enable 10-20x compressed replay */
    float compression_factor;   /**< Replay speed compression (10-20x) */
} nimcp_replay_params_t;

/**
 * @brief Parameters for systems consolidation
 */
typedef struct {
    float transfer_rate;        /**< Hip -> Cortex transfer rate */
    float semantic_threshold;   /**< When episodic becomes semantic [0.0-1.0] */
    float forgetting_rate;      /**< Decay rate for unrehearsed memories */
    float similarity_threshold; /**< Min similarity for linking [0.0-1.0] */
    float consolidation_rate_sws;  /**< Consolidation rate during SWS */
    float consolidation_rate_awake; /**< Consolidation rate while awake */
} nimcp_consolidation_params_t;

/**
 * @brief Parameters for engram weight updates
 */
typedef struct {
    float learning_rate;        /**< Weight update learning rate */
    float weight_decay;         /**< L2 regularization factor */
    float momentum;             /**< Momentum for updates */
    float max_weight;           /**< Maximum weight magnitude */
    float min_weight;           /**< Minimum weight magnitude */
    bool use_hebbian;           /**< Use Hebbian learning rule */
} nimcp_engram_update_params_t;

//=============================================================================
// GPU Memory Consolidation State
//=============================================================================

/**
 * @brief GPU-resident engram representation
 */
typedef struct {
    nimcp_gpu_tensor_t* neuron_ids;         /**< [batch, max_neurons] uint32 */
    nimcp_gpu_tensor_t* activations;        /**< [batch, max_neurons] float */
    nimcp_gpu_tensor_t* consolidation_strength; /**< [batch] float */
    nimcp_gpu_tensor_t* hippocampal_dependency; /**< [batch] float */
    nimcp_gpu_tensor_t* emotional_salience;     /**< [batch] float */
    nimcp_gpu_tensor_t* memory_age;             /**< [batch] float (seconds) */
    size_t batch_size;                      /**< Number of engrams in batch */
    size_t max_neurons;                     /**< Max neurons per engram */
} nimcp_gpu_engram_batch_t;

/**
 * @brief GPU-resident cortical memory node batch
 */
typedef struct {
    nimcp_gpu_tensor_t* features;           /**< [batch, feature_dim] float */
    nimcp_gpu_tensor_t* consolidation_strength; /**< [batch] float */
    nimcp_gpu_tensor_t* hippocampal_dependency; /**< [batch] float */
    nimcp_gpu_tensor_t* node_type;          /**< [batch] int32 (episodic/semantic/schema) */
    nimcp_gpu_tensor_t* neighbor_indices;   /**< [batch, max_neighbors] int32 */
    nimcp_gpu_tensor_t* neighbor_weights;   /**< [batch, max_neighbors] float */
    size_t batch_size;                      /**< Number of nodes in batch */
    size_t feature_dim;                     /**< Semantic feature dimensionality */
    size_t max_neighbors;                   /**< Max neighbors per node */
} nimcp_gpu_cortical_batch_t;

/**
 * @brief GPU replay event batch
 */
typedef struct {
    nimcp_gpu_tensor_t* engram_indices;     /**< [batch] int32 - which engrams to replay */
    nimcp_gpu_tensor_t* priorities;         /**< [batch] float - replay priorities */
    nimcp_gpu_tensor_t* emotional_salience; /**< [batch] float */
    nimcp_gpu_tensor_t* is_completed;       /**< [batch] int32 - completion flags */
    size_t batch_size;                      /**< Number of replay events */
} nimcp_gpu_replay_batch_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create GPU engram batch
 *
 * @param ctx GPU context
 * @param batch_size Number of engrams in batch
 * @param max_neurons Max neurons per engram
 * @return GPU engram batch or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_engram_batch_t* nimcp_gpu_engram_batch_create(
    nimcp_gpu_context_t* ctx,
    size_t batch_size,
    size_t max_neurons
);

/**
 * @brief Destroy GPU engram batch
 */
NIMCP_EXPORT void nimcp_gpu_engram_batch_destroy(nimcp_gpu_engram_batch_t* batch);

/**
 * @brief Create GPU cortical node batch
 *
 * @param ctx GPU context
 * @param batch_size Number of nodes in batch
 * @param feature_dim Semantic feature dimensionality
 * @param max_neighbors Max neighbors per node
 * @return GPU cortical batch or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_cortical_batch_t* nimcp_gpu_cortical_batch_create(
    nimcp_gpu_context_t* ctx,
    size_t batch_size,
    size_t feature_dim,
    size_t max_neighbors
);

/**
 * @brief Destroy GPU cortical batch
 */
NIMCP_EXPORT void nimcp_gpu_cortical_batch_destroy(nimcp_gpu_cortical_batch_t* batch);

/**
 * @brief Create GPU replay event batch
 *
 * @param ctx GPU context
 * @param batch_size Number of replay events
 * @return GPU replay batch or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_replay_batch_t* nimcp_gpu_replay_batch_create(
    nimcp_gpu_context_t* ctx,
    size_t batch_size
);

/**
 * @brief Destroy GPU replay batch
 */
NIMCP_EXPORT void nimcp_gpu_replay_batch_destroy(nimcp_gpu_replay_batch_t* batch);

//=============================================================================
// Hippocampal Replay Kernels
//=============================================================================

/**
 * @brief Execute parallel hippocampal replay
 *
 * WHAT: Reactivate engram patterns in parallel
 * WHY:  Simulates compressed hippocampal replay during sleep
 * HOW:  Parallel pattern completion with stochastic noise
 *
 * BIOLOGICAL BASIS:
 * - Sharp-wave ripples in hippocampus during SWS
 * - 10-20x compressed replay of experiences
 * - Wilson & McNaughton (1994)
 *
 * @param ctx GPU context
 * @param engrams Source engram batch
 * @param replay_events Which engrams to replay
 * @param output Reactivated patterns [batch, max_neurons]
 * @param params Replay parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_hippocampal_replay(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_engram_batch_t* engrams,
    const nimcp_gpu_replay_batch_t* replay_events,
    nimcp_gpu_tensor_t* output,
    const nimcp_replay_params_t* params
);

/**
 * @brief Pattern completion with partial cue
 *
 * WHAT: Complete memory patterns from partial input
 * WHY:  Enable recall from incomplete cues
 * HOW:  Parallel overlap computation and pattern reactivation
 *
 * @param ctx GPU context
 * @param engrams Stored engram batch
 * @param cue_patterns Partial cue patterns [n_queries, max_neurons]
 * @param cue_masks Valid neuron masks [n_queries, max_neurons]
 * @param completed_patterns Output completed patterns
 * @param match_scores Similarity scores for each engram
 * @param completion_threshold Min overlap for completion
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_pattern_completion(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_engram_batch_t* engrams,
    const nimcp_gpu_tensor_t* cue_patterns,
    const nimcp_gpu_tensor_t* cue_masks,
    nimcp_gpu_tensor_t* completed_patterns,
    nimcp_gpu_tensor_t* match_scores,
    float completion_threshold
);

//=============================================================================
// Systems Consolidation Kernels
//=============================================================================

/**
 * @brief Transfer engrams to cortical representations
 *
 * WHAT: Extract semantic features from engrams
 * WHY:  Models hippocampus to cortex memory transfer
 * HOW:  Parallel feature extraction and dimensionality reduction
 *
 * BIOLOGICAL BASIS:
 * - Complementary learning systems (McClelland et al., 1995)
 * - Gradual transfer during sleep
 * - Semantic abstraction
 *
 * @param ctx GPU context
 * @param engrams Source hippocampal engrams
 * @param cortical_nodes Target cortical representations
 * @param replay_strength How strongly to update [0.0-1.0]
 * @param params Consolidation parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_systems_consolidation(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_engram_batch_t* engrams,
    nimcp_gpu_cortical_batch_t* cortical_nodes,
    float replay_strength,
    const nimcp_consolidation_params_t* params
);

/**
 * @brief Update consolidation strength for all nodes
 *
 * WHAT: Strengthen cortical memories over time
 * WHY:  Models gradual consolidation
 * HOW:  Parallel update with sleep-dependent rate
 *
 * @param ctx GPU context
 * @param cortical_nodes Cortical memory nodes
 * @param time_delta_seconds Time since last update
 * @param is_sleeping Sleep state (faster consolidation)
 * @param params Consolidation parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_consolidation_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_cortical_batch_t* cortical_nodes,
    float time_delta_seconds,
    bool is_sleeping,
    const nimcp_consolidation_params_t* params
);

/**
 * @brief Apply memory decay/forgetting
 *
 * WHAT: Reduce consolidation strength for unrehearsed memories
 * WHY:  Models Ebbinghaus forgetting curve
 * HOW:  Parallel exponential decay
 *
 * @param ctx GPU context
 * @param cortical_nodes Cortical memory nodes
 * @param time_delta_seconds Time since last update
 * @param last_activation_times When each memory was last activated
 * @param forgetting_rate Decay rate
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_memory_decay(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_cortical_batch_t* cortical_nodes,
    float time_delta_seconds,
    const nimcp_gpu_tensor_t* last_activation_times,
    float forgetting_rate
);

//=============================================================================
// Engram Update Kernels
//=============================================================================

/**
 * @brief Update engram activation weights
 *
 * WHAT: Modify engram neuron activations
 * WHY:  Learning and consolidation modify memory traces
 * HOW:  Parallel Hebbian or gradient-based updates
 *
 * @param ctx GPU context
 * @param engrams Engram batch to update
 * @param updates Weight update deltas [batch, max_neurons]
 * @param params Update parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_engram_weight_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_engram_batch_t* engrams,
    const nimcp_gpu_tensor_t* updates,
    const nimcp_engram_update_params_t* params
);

/**
 * @brief Compute engram overlap (pattern separation metric)
 *
 * WHAT: Calculate overlap between engram pairs
 * WHY:  Pattern separation in hippocampus
 * HOW:  Parallel pairwise overlap computation
 *
 * @param ctx GPU context
 * @param engrams_a First engram batch
 * @param engrams_b Second engram batch
 * @param overlap_matrix Output overlap scores [n_a, n_b]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_engram_overlap(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_engram_batch_t* engrams_a,
    const nimcp_gpu_engram_batch_t* engrams_b,
    nimcp_gpu_tensor_t* overlap_matrix
);

//=============================================================================
// Similarity Search Kernels
//=============================================================================

/**
 * @brief Similarity-based memory search
 *
 * WHAT: Find semantically similar cortical memories
 * WHY:  Generalization and schema activation
 * HOW:  Parallel cosine similarity with top-k selection
 *
 * @param ctx GPU context
 * @param cortical_nodes Cortical memory nodes to search
 * @param query_features Query feature vectors [n_queries, feature_dim]
 * @param top_k Number of results per query
 * @param result_indices Top-k node indices [n_queries, top_k]
 * @param result_similarities Similarity scores [n_queries, top_k]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_similarity_search(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_cortical_batch_t* cortical_nodes,
    const nimcp_gpu_tensor_t* query_features,
    size_t top_k,
    nimcp_gpu_tensor_t* result_indices,
    nimcp_gpu_tensor_t* result_similarities
);

/**
 * @brief Build semantic similarity graph
 *
 * WHAT: Connect cortical nodes by semantic similarity
 * WHY:  Models lateral cortical connections
 * HOW:  Parallel pairwise similarity with threshold
 *
 * @param ctx GPU context
 * @param cortical_nodes Cortical memory nodes
 * @param similarity_threshold Min similarity for connection
 * @return true on success (updates neighbor_indices/weights in batch)
 */
NIMCP_EXPORT bool nimcp_gpu_build_similarity_graph(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_cortical_batch_t* cortical_nodes,
    float similarity_threshold
);

//=============================================================================
// Episodic-Semantic Transfer
//=============================================================================

/**
 * @brief Extract semantic features from episodic memories
 *
 * WHAT: Compute abstracted semantic features
 * WHY:  Episodic to semantic transformation
 * HOW:  Feature averaging and abstraction
 *
 * @param ctx GPU context
 * @param engrams Source episodic engrams
 * @param semantic_features Output semantic features [batch, feature_dim]
 * @param feature_dim Output feature dimensionality
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_extract_semantic_features(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_engram_batch_t* engrams,
    nimcp_gpu_tensor_t* semantic_features,
    size_t feature_dim
);

/**
 * @brief Check episodic to semantic transition
 *
 * WHAT: Determine if memories should transition to semantic
 * WHY:  Episodic details fade, gist remains
 * HOW:  Threshold-based parallel check
 *
 * @param ctx GPU context
 * @param cortical_nodes Cortical memory nodes
 * @param semantic_threshold Consolidation threshold for transition
 * @param should_transition Output flags [batch]
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_check_semantic_transition(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_cortical_batch_t* cortical_nodes,
    float semantic_threshold,
    nimcp_gpu_tensor_t* should_transition
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get default replay parameters
 */
NIMCP_EXPORT nimcp_replay_params_t nimcp_replay_params_default(void);

/**
 * @brief Get default consolidation parameters
 */
NIMCP_EXPORT nimcp_consolidation_params_t nimcp_consolidation_params_default(void);

/**
 * @brief Get default engram update parameters
 */
NIMCP_EXPORT nimcp_engram_update_params_t nimcp_engram_update_params_default(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MEMORY_CONSOLIDATION_GPU_H
