/**
 * @file nimcp_swarm_memory_gpu.h
 * @brief GPU-accelerated Swarm Memory Operations using CUDA
 *
 * WHAT: CUDA kernels for swarm memory consolidation and experience replay
 * WHY:  GPU acceleration enables massive parallelism for reinforcement learning
 *       experience replay buffers and multi-agent memory coordination
 * HOW:  Custom kernels for prioritized replay, consolidation, and agent sync
 *
 * ARCHITECTURE:
 * - Prioritized Experience Replay (PER) with sum-tree on GPU
 * - Memory consolidation (hippocampal replay, systems consolidation)
 * - Multi-agent memory coordination (federated averaging, consensus)
 * - Episodic memory with k-NN similarity search
 *
 * PARALLELIZATION STRATEGY:
 * - Parallelize across experiences in replay buffer
 * - Parallelize across agents for memory aggregation
 * - Batch sampling with warp-efficient sum-tree traversal
 * - Coalesced memory access for replay buffer operations
 *
 * BIOLOGICAL INSPIRATION:
 * - Sleep-based memory consolidation in mammals
 * - Hippocampal replay during slow-wave sleep
 * - Complementary learning systems theory
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_SWARM_MEMORY_GPU_H
#define NIMCP_SWARM_MEMORY_GPU_H

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

//=============================================================================
// Constants
//=============================================================================

/** Maximum agents in multi-agent memory system */
#define SWARM_MEMORY_GPU_MAX_AGENTS         1024

/** Default replay buffer capacity */
#define SWARM_MEMORY_GPU_DEFAULT_CAPACITY   100000

/** Default batch size for sampling */
#define SWARM_MEMORY_GPU_DEFAULT_BATCH      256

/** Maximum episode length for episodic memory */
#define SWARM_MEMORY_GPU_MAX_EPISODE_LEN    1000

/** Default priority exponent (alpha) */
#define SWARM_MEMORY_GPU_DEFAULT_ALPHA      0.6f

/** Default IS weight exponent (beta) */
#define SWARM_MEMORY_GPU_DEFAULT_BETA       0.4f

/** Default TD-error epsilon (small constant for priorities) */
#define SWARM_MEMORY_GPU_PRIORITY_EPS       1e-6f

//=============================================================================
// Replay Buffer Structures
//=============================================================================

/**
 * @brief GPU-resident experience replay buffer with prioritized sampling
 *
 * WHAT: Circular buffer storing (s, a, r, s', done) transitions on GPU
 * WHY:  GPU-accelerated sampling for fast batch generation
 * HOW:  Sum-tree for O(log n) prioritized sampling, circular write
 *
 * MEMORY LAYOUT:
 * - States/next_states: [capacity, state_dim] contiguous float
 * - Actions: [capacity, action_dim] contiguous float (or int for discrete)
 * - Rewards/dones: [capacity] contiguous float
 * - Priorities: [capacity] for PER, with sum-tree [2*capacity-1]
 */
typedef struct nimcp_replay_buffer_gpu_s {
    /* GPU context */
    nimcp_gpu_context_t* ctx;           /**< GPU context for operations */

    /* Experience storage tensors */
    nimcp_gpu_tensor_t* states;         /**< [capacity, state_dim] float */
    nimcp_gpu_tensor_t* actions;        /**< [capacity, action_dim] float */
    nimcp_gpu_tensor_t* rewards;        /**< [capacity] float */
    nimcp_gpu_tensor_t* next_states;    /**< [capacity, state_dim] float */
    nimcp_gpu_tensor_t* dones;          /**< [capacity] float (0 or 1) */

    /* Priority storage for PER */
    nimcp_gpu_tensor_t* priorities;     /**< [capacity] raw priorities */
    nimcp_gpu_tensor_t* sum_tree;       /**< [2*capacity-1] sum-tree for sampling */
    nimcp_gpu_tensor_t* min_tree;       /**< [2*capacity-1] min-tree for IS weights */

    /* Buffer state */
    size_t capacity;                    /**< Maximum transitions */
    size_t current_size;                /**< Current number of transitions */
    size_t write_idx;                   /**< Next write position */
    size_t state_dim;                   /**< State dimensionality */
    size_t action_dim;                  /**< Action dimensionality */

    /* PER parameters */
    float priority_alpha;               /**< Priority exponent [0, 1] */
    float priority_beta;                /**< IS weight exponent [0, 1] */
    float max_priority;                 /**< Current maximum priority */
    float priority_epsilon;             /**< Small constant for priorities */
    bool use_per;                       /**< Enable prioritized replay */

    /* Statistics */
    uint64_t total_stored;              /**< Total transitions stored */
    uint64_t total_sampled;             /**< Total transitions sampled */
} nimcp_replay_buffer_gpu_t;

/**
 * @brief Configuration for replay buffer creation
 */
typedef struct {
    size_t capacity;                    /**< Buffer capacity */
    size_t state_dim;                   /**< State dimensionality */
    size_t action_dim;                  /**< Action dimensionality */
    bool use_prioritized;               /**< Enable PER */
    float priority_alpha;               /**< Priority exponent */
    float priority_beta;                /**< IS weight exponent */
    float priority_epsilon;             /**< Priority epsilon */
} nimcp_replay_buffer_gpu_config_t;

//=============================================================================
// Swarm Memory GPU State
//=============================================================================

/**
 * @brief Hippocampal replay simulation parameters
 */
typedef struct {
    float compression_ratio;            /**< Replay speed compression (10-20x) */
    float replay_noise;                 /**< Stochastic noise level */
    float trace_decay;                  /**< Eligibility trace decay */
    bool enable_compressed_replay;      /**< Use compressed replay */
} nimcp_hippocampal_gpu_params_t;

/**
 * @brief Memory consolidation parameters
 */
typedef struct {
    float learning_rate;                /**< Consolidation learning rate */
    float decay_rate;                   /**< Memory decay rate */
    float min_strength;                 /**< Minimum memory strength threshold */
    float importance_threshold;         /**< Threshold for selective consolidation */
    float sws_consolidation_rate;       /**< Consolidation rate during SWS */
    float awake_consolidation_rate;     /**< Consolidation rate while awake */
} nimcp_consolidation_gpu_params_t;

/**
 * @brief Multi-agent coordination parameters
 */
typedef struct {
    float averaging_weight;             /**< Weight for federated averaging */
    float conflict_resolution_alpha;    /**< Conflict resolution blending */
    float confidence_threshold;         /**< Minimum confidence for sharing */
    bool enable_differential_privacy;   /**< Add DP noise to updates */
    float dp_noise_scale;               /**< Differential privacy noise scale */
} nimcp_agent_coordination_params_t;

/**
 * @brief GPU-accelerated swarm memory consolidation system
 *
 * WHAT: Complete swarm memory system with replay, consolidation, multi-agent
 * WHY:  Enables GPU-accelerated RL training with biological memory model
 * HOW:  Integrates replay buffer, hippocampal replay, agent coordination
 *
 * BIOLOGICAL BASIS:
 * - Hippocampus: Fast learning, episodic storage
 * - Cortex: Slow learning, semantic abstraction
 * - Sleep: Memory consolidation and replay
 */
typedef struct nimcp_swarm_memory_gpu_s {
    /* GPU context */
    nimcp_gpu_context_t* ctx;           /**< GPU context for operations */

    /* Experience replay buffer */
    nimcp_replay_buffer_gpu_t* replay_buffer; /**< Prioritized replay buffer */

    /* Hippocampal replay simulation */
    nimcp_gpu_tensor_t* hippocampal_trace;    /**< [trace_len, state_dim] */
    nimcp_gpu_tensor_t* eligibility_traces;   /**< [capacity] eligibility */
    size_t trace_len;                         /**< Hippocampal trace length */

    /* Cortical memory representation */
    nimcp_gpu_tensor_t* cortical_weights;     /**< [state_dim, cortical_dim] */
    nimcp_gpu_tensor_t* cortical_bias;        /**< [cortical_dim] */
    size_t cortical_dim;                      /**< Cortical representation dim */

    /* Memory consolidation state */
    nimcp_gpu_tensor_t* consolidation_mask;   /**< [capacity] binary mask */
    nimcp_gpu_tensor_t* memory_strength;      /**< [capacity] strength values */
    nimcp_gpu_tensor_t* memory_importance;    /**< [capacity] importance scores */

    /* Multi-agent memory coordination */
    int num_agents;                           /**< Number of agents */
    nimcp_gpu_tensor_t* agent_memories;       /**< [num_agents, memory_dim] */
    nimcp_gpu_tensor_t* agent_confidence;     /**< [num_agents, memory_dim] */
    nimcp_gpu_tensor_t* shared_knowledge;     /**< [memory_dim] consensus */
    nimcp_gpu_tensor_t* agent_gradients;      /**< [num_agents, memory_dim] */
    size_t memory_dim;                        /**< Memory vector dimension */

    /* Episodic memory storage */
    nimcp_gpu_tensor_t* episode_buffer;       /**< [max_episodes, max_len, state_dim] */
    nimcp_gpu_tensor_t* episode_lengths;      /**< [max_episodes] */
    nimcp_gpu_tensor_t* episode_rewards;      /**< [max_episodes] cumulative */
    size_t max_episodes;                      /**< Maximum stored episodes */
    size_t current_episode_count;             /**< Current episode count */
    size_t episode_write_idx;                 /**< Next episode write position */

    /* Parameters */
    nimcp_hippocampal_gpu_params_t hippocampal_params;
    nimcp_consolidation_gpu_params_t consolidation_params;
    nimcp_agent_coordination_params_t coordination_params;

    /* Temporary buffers for operations */
    nimcp_gpu_tensor_t* temp_batch_states;    /**< Temp batch buffer */
    nimcp_gpu_tensor_t* temp_batch_indices;   /**< Temp index buffer */
    nimcp_gpu_tensor_t* temp_random;          /**< Random number buffer */

    /* Statistics */
    uint64_t consolidation_count;       /**< Total consolidation operations */
    uint64_t agent_sync_count;          /**< Total agent sync operations */
    float avg_consolidation_time_ms;    /**< Average consolidation time */
} nimcp_swarm_memory_gpu_t;

/**
 * @brief Sampled batch from replay buffer
 */
typedef struct {
    nimcp_gpu_tensor_t* states;         /**< [batch_size, state_dim] */
    nimcp_gpu_tensor_t* actions;        /**< [batch_size, action_dim] */
    nimcp_gpu_tensor_t* rewards;        /**< [batch_size] */
    nimcp_gpu_tensor_t* next_states;    /**< [batch_size, state_dim] */
    nimcp_gpu_tensor_t* dones;          /**< [batch_size] */
    nimcp_gpu_tensor_t* weights;        /**< [batch_size] IS weights */
    nimcp_gpu_tensor_t* indices;        /**< [batch_size] buffer indices */
    size_t batch_size;                  /**< Number of samples */
} nimcp_replay_batch_t;

//=============================================================================
// Replay Buffer Lifecycle
//=============================================================================

/**
 * @brief Get default replay buffer configuration
 *
 * @param config Output configuration
 */
NIMCP_EXPORT void nimcp_replay_buffer_gpu_default_config(
    nimcp_replay_buffer_gpu_config_t* config
);

/**
 * @brief Create GPU replay buffer
 *
 * @param ctx GPU context
 * @param config Buffer configuration
 * @return Replay buffer or NULL on failure
 */
NIMCP_EXPORT nimcp_replay_buffer_gpu_t* nimcp_replay_buffer_gpu_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_replay_buffer_gpu_config_t* config
);

/**
 * @brief Destroy GPU replay buffer
 *
 * @param buffer Buffer to destroy
 */
NIMCP_EXPORT void nimcp_replay_buffer_gpu_destroy(
    nimcp_replay_buffer_gpu_t* buffer
);

/**
 * @brief Clear replay buffer
 *
 * @param buffer Buffer to clear
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_replay_buffer_gpu_clear(
    nimcp_replay_buffer_gpu_t* buffer
);

//=============================================================================
// Swarm Memory Lifecycle
//=============================================================================

/**
 * @brief Create GPU swarm memory system
 *
 * @param ctx GPU context
 * @param replay_capacity Replay buffer capacity
 * @param num_agents Number of agents for multi-agent coordination
 * @param state_dim State dimensionality
 * @param action_dim Action dimensionality
 * @param memory_dim Memory vector dimension for agent coordination
 * @return Swarm memory system or NULL on failure
 */
NIMCP_EXPORT nimcp_swarm_memory_gpu_t* nimcp_swarm_memory_gpu_create(
    nimcp_gpu_context_t* ctx,
    size_t replay_capacity,
    int num_agents,
    int state_dim,
    int action_dim,
    int memory_dim
);

/**
 * @brief Destroy GPU swarm memory system
 *
 * @param mem System to destroy
 */
NIMCP_EXPORT void nimcp_swarm_memory_gpu_destroy(
    nimcp_swarm_memory_gpu_t* mem
);

//=============================================================================
// Experience Replay Operations
//=============================================================================

/**
 * @brief Store transition in replay buffer
 *
 * Stores a single (s, a, r, s', done) transition with max priority.
 *
 * @param mem Swarm memory system
 * @param state Current state [state_dim]
 * @param action Action taken [action_dim]
 * @param reward Reward received
 * @param next_state Next state [state_dim]
 * @param done Episode termination flag
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_swarm_memory_gpu_store(
    nimcp_swarm_memory_gpu_t* mem,
    nimcp_gpu_tensor_t* state,
    nimcp_gpu_tensor_t* action,
    float reward,
    nimcp_gpu_tensor_t* next_state,
    bool done
);

/**
 * @brief Store batch of transitions
 *
 * Efficiently stores multiple transitions at once.
 *
 * @param mem Swarm memory system
 * @param states States [batch, state_dim]
 * @param actions Actions [batch, action_dim]
 * @param rewards Rewards [batch]
 * @param next_states Next states [batch, state_dim]
 * @param dones Done flags [batch]
 * @param batch_size Number of transitions
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_swarm_memory_gpu_store_batch(
    nimcp_swarm_memory_gpu_t* mem,
    nimcp_gpu_tensor_t* states,
    nimcp_gpu_tensor_t* actions,
    nimcp_gpu_tensor_t* rewards,
    nimcp_gpu_tensor_t* next_states,
    nimcp_gpu_tensor_t* dones,
    size_t batch_size
);

/**
 * @brief Sample batch from replay buffer
 *
 * Samples transitions using prioritized experience replay (PER).
 * Returns importance sampling weights for bias correction.
 *
 * @param mem Swarm memory system
 * @param batch_size Number of transitions to sample
 * @param batch Output sampled batch (caller allocates)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_swarm_memory_gpu_sample(
    nimcp_swarm_memory_gpu_t* mem,
    size_t batch_size,
    nimcp_replay_batch_t* batch
);

/**
 * @brief Create replay batch structure
 *
 * @param ctx GPU context
 * @param batch_size Batch size
 * @param state_dim State dimension
 * @param action_dim Action dimension
 * @return Replay batch or NULL on failure
 */
NIMCP_EXPORT nimcp_replay_batch_t* nimcp_replay_batch_create(
    nimcp_gpu_context_t* ctx,
    size_t batch_size,
    size_t state_dim,
    size_t action_dim
);

/**
 * @brief Destroy replay batch
 *
 * @param batch Batch to destroy
 */
NIMCP_EXPORT void nimcp_replay_batch_destroy(nimcp_replay_batch_t* batch);

/**
 * @brief Update priorities after learning
 *
 * Updates priorities based on TD-errors from learning step.
 *
 * @param mem Swarm memory system
 * @param indices Buffer indices [batch_size]
 * @param td_errors TD-errors from learning [batch_size]
 * @param batch_size Number of transitions updated
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_swarm_memory_gpu_update_priorities(
    nimcp_swarm_memory_gpu_t* mem,
    nimcp_gpu_tensor_t* indices,
    nimcp_gpu_tensor_t* td_errors,
    size_t batch_size
);

/**
 * @brief Anneal beta parameter over training
 *
 * @param mem Swarm memory system
 * @param new_beta New beta value
 */
NIMCP_EXPORT void nimcp_swarm_memory_gpu_set_beta(
    nimcp_swarm_memory_gpu_t* mem,
    float new_beta
);

//=============================================================================
// Memory Consolidation Operations
//=============================================================================

/**
 * @brief Execute memory consolidation step
 *
 * Performs hippocampal replay and systems consolidation.
 *
 * @param mem Swarm memory system
 * @param dt Time step for decay
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_swarm_memory_gpu_consolidate(
    nimcp_swarm_memory_gpu_t* mem,
    float dt
);

/**
 * @brief Apply memory decay to all stored memories
 *
 * Implements Ebbinghaus forgetting curve.
 *
 * @param ctx GPU context
 * @param memory_strength Memory strength tensor [n]
 * @param decay_rate Per-step decay rate
 * @param min_strength Minimum strength (below = forget)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_swarm_memory_gpu_decay(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* memory_strength,
    float decay_rate,
    float min_strength
);

/**
 * @brief Selective memory consolidation
 *
 * Consolidates only memories above importance threshold.
 *
 * @param ctx GPU context
 * @param memories Memory tensor [n, dim]
 * @param importance Importance scores [n]
 * @param consolidated Output consolidated memories [n, dim]
 * @param threshold Importance threshold
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_swarm_memory_gpu_selective_consolidate(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* memories,
    const nimcp_gpu_tensor_t* importance,
    nimcp_gpu_tensor_t* consolidated,
    float threshold
);

/**
 * @brief Hippocampal compression (sequence compression)
 *
 * Compresses experience sequences for efficient replay.
 *
 * @param ctx GPU context
 * @param sequence Sequence tensor [seq_len, state_dim]
 * @param compressed Output compressed [compressed_len, state_dim]
 * @param seq_len Sequence length
 * @param state_dim State dimension
 * @param compression_ratio Target compression ratio
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_swarm_memory_gpu_hippocampal_compress(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* sequence,
    nimcp_gpu_tensor_t* compressed,
    size_t seq_len,
    size_t state_dim,
    float compression_ratio
);

/**
 * @brief Systems consolidation (hippocampus to cortex)
 *
 * Transfers memories from hippocampal to cortical representation.
 *
 * @param ctx GPU context
 * @param hippocampal_mem Hippocampal memories [n, state_dim]
 * @param cortical_weights Cortical weight matrix [state_dim, cortical_dim]
 * @param consolidation_gate Gating signal [n]
 * @param learning_rate Update learning rate
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_swarm_memory_gpu_systems_consolidation(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* hippocampal_mem,
    nimcp_gpu_tensor_t* cortical_weights,
    const nimcp_gpu_tensor_t* consolidation_gate,
    float learning_rate
);

/**
 * @brief SWS (Slow-Wave Sleep) replay
 *
 * Orders memories by importance for replay during sleep.
 *
 * @param ctx GPU context
 * @param memories Memory indices [num_memories]
 * @param replay_order Output replay order [replay_count]
 * @param importance_scores Importance scores [num_memories]
 * @param num_memories Number of memories
 * @param replay_count Number to replay
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_swarm_memory_gpu_sws_replay(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* memories,
    nimcp_gpu_tensor_t* replay_order,
    const nimcp_gpu_tensor_t* importance_scores,
    size_t num_memories,
    size_t replay_count
);

//=============================================================================
// Multi-Agent Memory Coordination
//=============================================================================

/**
 * @brief Synchronize memories across agents
 *
 * Performs federated averaging of agent memories.
 *
 * @param mem Swarm memory system
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_swarm_memory_gpu_sync_agents(
    nimcp_swarm_memory_gpu_t* mem
);

/**
 * @brief Aggregate memories across agents
 *
 * Weighted averaging of agent memories into shared knowledge.
 *
 * @param ctx GPU context
 * @param agent_memories Agent memory tensors [num_agents, memory_dim]
 * @param aggregated Output aggregated memory [memory_dim]
 * @param agent_weights Weight per agent [num_agents]
 * @param num_agents Number of agents
 * @param memory_dim Memory dimension
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_swarm_memory_gpu_aggregate_memories(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* agent_memories,
    nimcp_gpu_tensor_t* aggregated,
    const nimcp_gpu_tensor_t* agent_weights,
    int num_agents,
    int memory_dim
);

/**
 * @brief Federated averaging update
 *
 * Updates shared knowledge using federated averaging protocol.
 *
 * @param ctx GPU context
 * @param agent_updates Agent gradient updates [num_agents, dim]
 * @param shared_knowledge Shared knowledge to update [dim]
 * @param num_agents Number of agents
 * @param knowledge_dim Knowledge dimension
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_swarm_memory_gpu_federated_average(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* agent_updates,
    nimcp_gpu_tensor_t* shared_knowledge,
    int num_agents,
    int knowledge_dim
);

/**
 * @brief Broadcast memory to selected agents
 *
 * Copies source memory to target agents.
 *
 * @param ctx GPU context
 * @param source_memory Source memory [memory_dim]
 * @param target_memories Target agent memories [num_targets, memory_dim]
 * @param target_agents Target agent indices [num_targets]
 * @param num_targets Number of target agents
 * @param memory_dim Memory dimension
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_swarm_memory_gpu_broadcast(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* source_memory,
    nimcp_gpu_tensor_t* target_memories,
    const nimcp_gpu_tensor_t* target_agents,
    int num_targets,
    int memory_dim
);

/**
 * @brief Resolve conflicting memories between agents
 *
 * Resolves conflicts using confidence-weighted averaging.
 *
 * @param ctx GPU context
 * @param memories Agent memories [num_agents, memory_dim]
 * @param confidence Agent confidence [num_agents, memory_dim]
 * @param resolved Output resolved memory [memory_dim]
 * @param num_agents Number of agents
 * @param memory_dim Memory dimension
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_swarm_memory_gpu_conflict_resolution(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* memories,
    const nimcp_gpu_tensor_t* confidence,
    nimcp_gpu_tensor_t* resolved,
    int num_agents,
    int memory_dim
);

//=============================================================================
// Episodic Memory Operations
//=============================================================================

/**
 * @brief Store complete episode
 *
 * @param mem Swarm memory system
 * @param episode Episode states [episode_len, state_dim]
 * @param actions Episode actions [episode_len, action_dim]
 * @param rewards Episode rewards [episode_len]
 * @param episode_len Episode length
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_swarm_memory_gpu_store_episode(
    nimcp_swarm_memory_gpu_t* mem,
    nimcp_gpu_tensor_t* episode,
    nimcp_gpu_tensor_t* actions,
    nimcp_gpu_tensor_t* rewards,
    size_t episode_len
);

/**
 * @brief Compute episode similarity (k-NN search)
 *
 * Finds most similar episodes to query state.
 *
 * @param ctx GPU context
 * @param query Query state [state_dim]
 * @param episodes Stored episodes [num_episodes, max_len, state_dim]
 * @param similarities Output similarities [num_episodes]
 * @param num_episodes Number of episodes
 * @param episode_len Maximum episode length
 * @param state_dim State dimension
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_swarm_memory_gpu_episode_similarity(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* query,
    const nimcp_gpu_tensor_t* episodes,
    nimcp_gpu_tensor_t* similarities,
    size_t num_episodes,
    size_t episode_len,
    size_t state_dim
);

/**
 * @brief Replay selected episodes for learning
 *
 * @param ctx GPU context
 * @param episodes Episode buffer [num_episodes, max_len, state_dim]
 * @param replay_indices Indices to replay [batch_size]
 * @param replayed_states Output states [batch_size, max_len, state_dim]
 * @param replayed_actions Output actions [batch_size, max_len, action_dim]
 * @param batch_size Number of episodes to replay
 * @param episode_len Episode length
 * @param state_dim State dimension
 * @param action_dim Action dimension
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_swarm_memory_gpu_episode_replay(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* episodes,
    const nimcp_gpu_tensor_t* replay_indices,
    nimcp_gpu_tensor_t* replayed_states,
    nimcp_gpu_tensor_t* replayed_actions,
    size_t batch_size,
    size_t episode_len,
    size_t state_dim,
    size_t action_dim
);

//=============================================================================
// Sum-Tree Operations (for PER)
//=============================================================================

/**
 * @brief Build sum-tree from priorities
 *
 * Constructs O(log n) sampling tree from priority array.
 *
 * @param ctx GPU context
 * @param priorities Priority values [capacity]
 * @param sum_tree Output sum-tree [2*capacity-1]
 * @param capacity Buffer capacity
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_swarm_memory_gpu_build_sum_tree(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* priorities,
    nimcp_gpu_tensor_t* sum_tree,
    size_t capacity
);

/**
 * @brief Update single priority in sum-tree
 *
 * O(log n) update for single priority change.
 *
 * @param ctx GPU context
 * @param sum_tree Sum-tree to update [2*capacity-1]
 * @param leaf_idx Leaf index to update
 * @param new_priority New priority value
 * @param capacity Buffer capacity
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_swarm_memory_gpu_update_sum_tree(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* sum_tree,
    size_t leaf_idx,
    float new_priority,
    size_t capacity
);

/**
 * @brief Sample from sum-tree
 *
 * O(log n) prioritized sampling.
 *
 * @param ctx GPU context
 * @param sum_tree Sum-tree [2*capacity-1]
 * @param random_vals Random values for sampling [batch_size]
 * @param sampled_indices Output indices [batch_size]
 * @param batch_size Number of samples
 * @param capacity Buffer capacity
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_swarm_memory_gpu_sample_sum_tree(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* sum_tree,
    const nimcp_gpu_tensor_t* random_vals,
    nimcp_gpu_tensor_t* sampled_indices,
    size_t batch_size,
    size_t capacity
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get default hippocampal parameters
 *
 * @param params Output parameters
 */
NIMCP_EXPORT void nimcp_hippocampal_gpu_default_params(
    nimcp_hippocampal_gpu_params_t* params
);

/**
 * @brief Get default consolidation parameters
 *
 * @param params Output parameters
 */
NIMCP_EXPORT void nimcp_consolidation_gpu_default_params(
    nimcp_consolidation_gpu_params_t* params
);

/**
 * @brief Get default agent coordination parameters
 *
 * @param params Output parameters
 */
NIMCP_EXPORT void nimcp_agent_coordination_gpu_default_params(
    nimcp_agent_coordination_params_t* params
);

/**
 * @brief Get replay buffer statistics
 *
 * @param buffer Replay buffer
 * @param size_out Current size
 * @param capacity_out Total capacity
 * @param total_stored_out Total stored count
 * @param total_sampled_out Total sampled count
 */
NIMCP_EXPORT void nimcp_replay_buffer_gpu_stats(
    const nimcp_replay_buffer_gpu_t* buffer,
    size_t* size_out,
    size_t* capacity_out,
    uint64_t* total_stored_out,
    uint64_t* total_sampled_out
);

/**
 * @brief Print swarm memory info to log
 *
 * @param mem Swarm memory system
 */
NIMCP_EXPORT void nimcp_swarm_memory_gpu_print_info(
    const nimcp_swarm_memory_gpu_t* mem
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SWARM_MEMORY_GPU_H */
