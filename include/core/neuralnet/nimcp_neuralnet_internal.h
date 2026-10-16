/**
 * @file nimcp_neuralnet_internal.h
 * @brief SINGLE authoritative definition of neural_network_struct
 *
 * WARNING: This is the ONLY place neural_network_struct should be defined.
 * All files needing access to the internal struct MUST include this header.
 * DO NOT duplicate this struct definition in any other file.
 *
 * After changing neuron_t or neural_network_struct, run: make -j4 (full rebuild)
 *
 * HISTORY:
 * - Consolidated from 7+ duplicate definitions (2026-02-26)
 * - Several duplicates were stale (missing sparse synapse pools, bulk allocation,
 *   wrong activation_strategy_table_t sizes, bogus immune_system field)
 */
#ifndef NIMCP_NEURALNET_INTERNAL_H
#define NIMCP_NEURALNET_INTERNAL_H

#include "core/neuralnet/nimcp_neuralnet.h"       /* neuron_t, network_config_t, neural_network_t */
#include "core/neuralnet/nimcp_sparse_synapse.h"   /* sparse_synapse_pool_t, synapse_metadata_pool_t */
#include "async/nimcp_bio_async.h"                 /* bio_module_context_t */
#include "gpu/embedding/nimcp_embedding_relevance_gpu.h"  /* nimcp_embedding_gpu_state_t */

/**
 * @brief Function pointer type for activation strategies
 *
 * WHY: Eliminates switch statements, enables O(1) dispatch
 * Follows Strategy pattern - each activation type is its own algorithm
 *
 * @param input Raw input value
 * @param threshold Adaptive threshold for spike-based activations
 * @return Activated output value
 */
typedef float (*activation_fn_t)(float input, float threshold);

/**
 * @brief Activation function strategy table
 *
 * INVARIANT: All entries must be non-NULL after initialization
 * PATTERN: Strategy pattern - function pointer dispatch
 */
typedef struct {
    activation_fn_t functions[8];  /* Max activation types */
} activation_strategy_table_t;

/**
 * @brief Internal neural network structure
 *
 * INVARIANTS:
 * - num_neurons <= MAX_NEURONS
 * - current_time <= network_time
 * - 0.0 <= global_activity <= 1.0
 * - 0.0 <= network_stability <= 1.0
 *
 * PATTERN: Handle/Body idiom - opaque pointer hides implementation
 * COMPLEXITY: O(1) access to all fields
 *
 * WARNING: Field order matters! Adding fields MUST be done at the end,
 * before the closing brace. Reordering fields will break all compiled
 * modules until a full rebuild.
 */
struct neural_network_struct {
    neuron_t* neurons;     /**< Dynamically allocated neurons array */
    uint32_t num_neurons;  /**< Current number of active neurons */
    uint32_t capacity;     /**< Allocated capacity (for add_neuron) */
    uint64_t current_time;
    network_config_t config;
    uint64_t network_time;
    float global_activity;
    float network_stability;
    float learning_momentum;
    float last_avg_weight;
    uint64_t last_maintenance;
    activation_strategy_table_t activation_strategies;

    /* NIMCP 2.7: NLP Integration - Subsystems for synapse compute context */
    void* neuromodulator_system;  /**< Neuromodulator system (opaque pointer) */
    float* global_state;          /**< Global state for synapse computation (attention output, etc) */
    uint32_t global_state_size;   /**< Size of global state buffer */

    /* NIMCP Phase 6: Glial Integration - Neuro-glial signaling */
    void* glial_integration;      /**< Glial integration system (opaque pointer) */

    /* Axon Integration - Signal propagation with realistic conduction delays */
    void* axon_network;           /**< Axon network for spike propagation (axon_network_t*, NULL = no axons) */

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    /* Sparse synapse pools (NIMCP 2.11: Memory-efficient synapse storage) */
    sparse_synapse_pool_t synapse_handle_pool;
    synapse_metadata_pool_t synapse_metadata_pool;
    synapse_cold_pool_t synapse_cold_pool;  /**< Cold data pool for STP/BCM/eligibility/compute/type */

    /* Bulk allocation for large networks - reduces 2M tracked allocs to 2 */
    spike_record_t* spike_history_bulk;   /**< Single contiguous spike history allocation */
    float* activity_history_bulk;         /**< Single contiguous activity history allocation */
    uint32_t bulk_neuron_count;           /**< Neurons using bulk arrays (0 = individual allocs) */

    /* Phase: Active Neuron Set Tracking (40-watt brain optimization)
     * Only 1-5% of neurons fire at any time. Track active set to skip dormant neurons.
     * active_neuron_ids: pool of neuron IDs that fired or received input last step
     * num_active_neurons: count of active neurons from last compute_step
     * active_set_valid: whether the active set is up to date */
    uint32_t* active_neuron_ids;      /**< Pool of active neuron IDs */
    uint32_t num_active_neurons;      /**< Count of active neurons last step */
    uint32_t active_neuron_capacity;  /**< Allocated capacity for active set */
    bool active_set_valid;            /**< Whether active set is current */

    /* Cached neuromodulation level (computed once per compute_step, used per-synapse) */
    float cached_neuromod_level;

    /* Bulk BCM and eligibility pools — eliminates per-connection heap allocations.
     * For 10K neurons × 64 fan-in = 674K connections × 2 allocs each = 1.35M mallocs
     * replaced by 2 bulk allocations. Synapse bcm/eligibility pointers index into these. */
    bcm_synapse_t* bcm_pool;                /**< Bulk BCM array (NULL = per-synapse alloc) */
    uint32_t bcm_pool_capacity;             /**< Allocated pool capacity */
    uint32_t bcm_pool_used;                 /**< Next available slot */
    eligibility_trace_t* eligibility_pool;  /**< Bulk eligibility array (NULL = per-synapse alloc) */
    uint32_t eligibility_pool_capacity;     /**< Allocated pool capacity */
    uint32_t eligibility_pool_used;         /**< Next available slot */

    /* CPU-staged embedding pool — full embedding vectors live in CPU RAM (pinned),
     * only transferred to GPU in batches for relevance recomputation.
     * Forward pass reads only the cached semantic_relevance float (always in CPU). */
    float* embedding_pool;                  /**< Pinned CPU buffer: capacity × dim floats */
    uint32_t embedding_pool_capacity;       /**< Max embeddings in pool */
    uint32_t embedding_pool_used;           /**< Next bump-allocator slot */
    uint32_t* embedding_free_list;          /**< Stack of freed slot indices */
    uint32_t embedding_free_count;          /**< Free list depth */
    uint16_t embedding_dim;                 /**< Embedding dimension (2048) */
    bool embedding_pool_pinned;             /**< True if pool uses cudaHostAlloc */

    /* GPU-side buffers for batch relevance recomputation */
    nimcp_embedding_gpu_state_t embedding_gpu_state;
    bool embedding_gpu_initialized;         /**< True if GPU state is ready */

    /* Learnable layer normalization affine parameters (Phase 3: Architecture Eval) */
    float** layer_norm_gamma;               /**< Per-layer scale (init 1.0), NULL = raw norm */
    float** layer_norm_beta;                /**< Per-layer shift (init 0.0) */
    uint32_t num_norm_layers;               /**< Number of layers with learnable norm params */

    /* Residual/skip connections (Phase 4: Architecture Eval) */
    bool enable_residual;                   /**< Enable skip connections (layer L -> L+2) */
    float** residual_projections;           /**< Projection matrices for dim-mismatched skips (NULL=identity) */
    uint32_t* residual_proj_src_dim;        /**< Source dimension for each projection */
    uint32_t* residual_proj_dst_dim;        /**< Destination dimension for each projection */
    uint32_t num_residual_pairs;            /**< Number of residual skip pairs */
    float** residual_saved_states;          /**< Saved pre-activation states for residual add */
};

//=============================================================================
// CPU-Staged Embedding Pool Functions (nimcp_synapse_embeddings.c)
//=============================================================================

/** Create embedding pool with pinned CPU memory (or regular fallback) */
bool embedding_pool_create(neural_network_t net, uint32_t capacity, uint16_t dim);

/** Destroy embedding pool and free all memory */
void embedding_pool_destroy(neural_network_t net);

/** Allocate a slot from pool, returns NIMCP_EMBEDDING_POOL_NONE on exhaustion */
uint32_t embedding_pool_allocate(neural_network_t net);

/** Free a slot back to pool for reuse */
void embedding_pool_free_slot(neural_network_t net, uint32_t index);

/** Get pointer to embedding vector at index (do not cache — pointer invalidated on pool growth) */
float* embedding_pool_get(neural_network_t net, uint32_t index);

/** Initialize a synapse embedding from the network pool */
bool synapse_init_embedding_pooled(neural_network_t net, synapse_t *synapse);

/** Bulk-initialize embeddings for all incoming synapses that lack one */
uint32_t embedding_pool_init_all_synapses(neural_network_t net);

/** Destroy a synapse embedding and return slot to pool */
void synapse_destroy_embedding_pooled(neural_network_t net, synapse_t *synapse);

/** Compute similarity between two synapses using pooled embeddings */
float synapse_semantic_similarity_pooled(neural_network_t net,
                                          const synapse_t *syn1,
                                          const synapse_t *syn2);

/** Update embedding via gradient descent toward target */
bool synapse_update_embedding_pooled(neural_network_t net, synapse_t *synapse,
                                      const float *target_embedding, float learning_rate);

/** Compute relevance of a synapse to a context embedding */
float synapse_compute_relevance_pooled(neural_network_t net, synapse_t *synapse,
                                        const float *context_embedding, uint16_t context_dim);

/** Batch-recompute relevance for ALL synapses in network (CPU fallback) */
uint32_t embedding_pool_recompute_relevance_cpu(neural_network_t net,
                                                 const float *context_embedding,
                                                 uint16_t context_dim);

/** Batch-recompute relevance for all active embeddings (GPU with CPU fallback).
 *  Lazily initializes GPU state on first call if gpu_ctx is provided. */
uint32_t embedding_pool_recompute_relevance(
    neural_network_t net,
    nimcp_gpu_context_t* gpu_ctx,
    const float* context_embedding,
    uint16_t context_dim);

#endif /* NIMCP_NEURALNET_INTERNAL_H */
