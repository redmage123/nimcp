//=============================================================================
// nimcp_synapse_compute.h - Programmable Synapse Computation API
//=============================================================================
/**
 * @file nimcp_synapse_compute.h
 * @brief Public API for synapse-level computation and learning
 *
 * NIMCP 2.7 MAJOR FEATURE: Synapse-Level Computation
 *
 * ARCHITECTURE:
 * - Strategy Pattern: Function pointers enable per-synapse customization
 * - Template Method Pattern: Consistent interface, variable implementation
 * - Memento Pattern: compute_state preserves synapse-specific memory
 *
 * PERFORMANCE CHARACTERISTICS:
 * - Function call overhead: 1 indirect jump (~5 cycles)
 * - Memory overhead: 24 bytes per synapse (3 pointers on 64-bit)
 * - Cache efficiency: Function pointers hot in L1, state in L1/L2
 * - Parallelization: Fully parallelizable across synapses
 * - GPU-friendly: Function pointers can be device functions (CUDA)
 *
 * DESIGN DECISIONS:
 * - Function pointers over virtual dispatch: Zero C++ dependency
 * - Context parameter: Enables global state without tight coupling
 * - Const neuron pointers: Documents read-only access, enables optimization
 * - Local memory first: 16 floats (64 bytes) before heap allocation
 *
 * BIOLOGICAL MOTIVATION:
 * Real synapses are not passive weights - they actively compute:
 * - Dendritic computation (Mel, 1993; London & Häusser, 2005)
 * - Active conductances in spines (Yuste & Denk, 1995)
 * - Local protein synthesis (Sutton & Schuman, 2006)
 * - Heterosynaptic plasticity (Lynch et al., 1977)
 *
 * This transforms NIMCP from 100K passive weights → 100K active processors
 *
 * @author Claude Code + NIMCP Development Team
 * @date 2025-11-07
 * @version 2.7.0
 */

#ifndef NIMCP_SYNAPSE_COMPUTE_H
#define NIMCP_SYNAPSE_COMPUTE_H

#include <stdint.h>
#include <stdbool.h>

// CRITICAL: Must include nimcp_neuralnet.h for full struct definitions
// WHY: synapse_compute functions need access to neuron_t internals
// HOW: Include neuralnet.h first, which forward-declares our types
#include "core/neuralnet/nimcp_neuralnet.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations (redundant after neuralnet.h include, but explicit)
//=============================================================================

// neuron_t and synapse_t are fully defined in nimcp_neuralnet.h (included above)

//=============================================================================
// Context and State Structures
//=============================================================================

/**
 * @brief Context data for synapse computation
 *
 * WHAT: Shared context passed to all synapse compute functions in a timestep
 * WHY: Enables coordination without tight coupling or global variables
 * HOW: Single struct passed by pointer, read-only access recommended
 * WHEN: Created once per timestep, shared across all active synapses
 *
 * USE CASES:
 * - Global attention context (query/key vectors for all synapses)
 * - Neuromodulation levels (dopamine, serotonin, acetylcholine)
 * - Time-dependent modulation (circadian rhythms, oscillations)
 * - Reward signals for three-factor learning
 *
 * PERFORMANCE: Single allocation per timestep, zero overhead per synapse
 */
typedef struct synapse_compute_context_t {
    float* global_state;        /**< Global network state (e.g., Transformer Q/K/V) */
    uint32_t global_state_size; /**< Size of global state vector */

    float neuromodulation;      /**< Neuromodulation level [0,1] (dopamine/etc) */
    uint64_t current_time;      /**< Current simulation timestamp (ms) */

    void* custom_data;          /**< User-defined context data (task-specific) */
    uint32_t custom_data_size;  /**< Size of custom data (bytes) */

    // Three-factor learning integration (Phase 2.7.1)
    void* network;              /**< Neural network pointer (for accessing neuromod system) */
    void* neuromodulator_system; /**< Neuromodulator system pointer (for direct access) */

    // Hot/cold split (NIMCP 2.11): Cold synapse data passed through context
    void* synapse_cold;         /**< synapse_cold_t* for accessing type_state, compute_state, etc. */
} synapse_compute_context_t;

//=============================================================================
// Function Pointer Types - Strategy Pattern Interface
//=============================================================================

//
// NOTE: synapse_compute_fn and synapse_learn_fn are typedef'd in nimcp_neuralnet.h
// WHY: Breaking circular dependency - neuralnet.h needs these types for synapse_t
// This file provides the struct definitions; neuralnet.h provides the typedefs
//
// For reference, the function signatures are:
//
// typedef float (*synapse_compute_fn)(
//     struct synapse_t* syn,
//     const neuron_t* pre_neuron,
//     const neuron_t* post_neuron,
//     float pre_activity,
//     struct struct synapse_compute_context_t* context
// );
//
// typedef void (*synapse_learn_fn)(
//     struct synapse_t* syn,
//     const neuron_t* pre_neuron,
//     const neuron_t* post_neuron,
//     float pre_spike_time,
//     float post_spike_time,
//     float reward_signal,
//     struct struct synapse_compute_context_t* context
// );
//
// WHAT: Function pointer types for custom synapse computation and learning
// WHY: Enables per-synapse algorithmic customization via Strategy pattern
// HOW: Called once per active synapse per timestep during network update
// WHEN: Invoked by neural_network_step() in sum_synaptic_inputs()
//
// PERFORMANCE: Called once per active synapse per timestep
// Keep computation lightweight (target: <100 cycles)

/**
 * @brief Extended synapse state for programmable functions (Memento Pattern)
 *
 * WHAT: Per-synapse memory for compute function state
 * WHY: Enables stateful computation without global variables
 * HOW: Allocated on-demand when synapse needs custom state
 * WHEN: Attached to synapses using custom compute functions
 *
 * MEMORY LAYOUT:
 * - local_memory: 64 bytes (16 floats) - fits in L1 cache line
 * - extended_memory: Heap-allocated for larger state (embeddings, traces)
 * - function_data: Opaque pointer for arbitrary custom data
 *
 * LIFECYCLE:
 * 1. Created when compute function is attached
 * 2. Updated during computation/learning
 * 3. Cleaned up via cleanup_fn when synapse is destroyed
 *
 * PERFORMANCE: Local memory is cache-friendly, extended memory on-demand
 */
typedef struct synapse_compute_state_t {
    float local_memory[16];     /**< Fast scratchpad (64B, L1-cached) */
    float* extended_memory;     /**< Heap for large state (embeddings, etc) */
    uint32_t extended_size;     /**< Size of extended memory (floats) */

    void* function_data;        /**< Opaque function-specific data */
    void (*cleanup_fn)(void*);  /**< Cleanup for function_data (NULL = no cleanup) */
} synapse_compute_state_t;

//=============================================================================
// Built-In Compute Functions - Standard Library
//=============================================================================

/**
 * @brief Default synapse computation (baseline)
 *
 * output = weight × pre_activity × STP_modulation
 *
 * COMPLEXITY: O(1)
 * PERFORMANCE: ~10 cycles (2 muls)
 */
float synapse_compute_default(
    struct synapse_t* syn,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_activity,
    struct synapse_compute_context_t* context
);

/**
 * @brief Attention-modulated synapse computation
 *
 * Computes attention weight based on query-key similarity.
 *
 * ALGORITHM:
 * 1. Extract query from postsynaptic neuron state
 * 2. Extract key from presynaptic neuron state
 * 3. Compute similarity: attention = exp(query · key / √d)
 * 4. Return: weight × pre_activity × attention × STP
 *
 * CONTEXT REQUIREMENTS:
 * - global_state contains query/key embeddings
 *
 * COMPLEXITY: O(d) where d = embedding dimension
 * TYPICAL d = 16-64, so ~100-500 cycles
 *
 * BIOLOGICAL INSPIRATION: Top-down attentional modulation
 * (Desimone & Duncan, 1995; Reynolds & Heeger, 2009)
 */
float synapse_compute_attention(
    struct synapse_t* syn,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_activity,
    struct synapse_compute_context_t* context
);

/**
 * @brief Semantic similarity-modulated synapse
 *
 * For NLP: modulates transmission by semantic similarity between
 * word embeddings.
 *
 * ALGORITHM:
 * 1. Lookup word embedding for pre/post neurons
 * 2. Compute cosine similarity
 * 3. Return: weight × pre_activity × similarity × STP
 *
 * USE CASE: Connect word populations, strengthen related words
 *
 * STORAGE: Embeddings stored in synapse_compute_state_t.extended_memory
 *
 * COMPLEXITY: O(d) where d = embedding dimension (typically 300)
 */
float synapse_compute_semantic(
    struct synapse_t* syn,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_activity,
    struct synapse_compute_context_t* context
);

/**
 * @brief Gating synapse (multiplicative modulation)
 *
 * Acts as a gate controlled by a third neuron population.
 *
 * ALGORITHM:
 * output = weight × pre_activity × gate_signal × STP
 *
 * WHERE: gate_signal read from context.global_state
 *
 * USE CASES:
 * - LSTM-like gates (input/forget/output)
 * - Attention gating
 * - Context-dependent routing
 *
 * BIOLOGICAL: Shunting inhibition (Koch, 1999)
 */
float synapse_compute_gating(
    struct synapse_t* syn,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_activity,
    struct synapse_compute_context_t* context
);

/**
 * @brief Neuromodulator-sensitive synapse
 *
 * Transmission modulated by dopamine/serotonin/ACh levels.
 *
 * ALGORITHM:
 * modulation = 1.0 + context.neuromodulation × sensitivity
 * output = weight × pre_activity × modulation × STP
 *
 * WHERE: sensitivity stored in local_memory[0]
 *
 * BIOLOGICAL: D1/D2 receptor modulation in striatum
 * (Surmeier et al., 2007)
 */
float synapse_compute_neuromodulated(
    struct synapse_t* syn,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_activity,
    struct synapse_compute_context_t* context
);

/**
 * @brief Dendritic computation synapse
 *
 * Implements local dendritic non-linearity.
 *
 * ALGORITHM:
 * local_sum = Σ(nearby synapses)  // From synapse neighborhood
 * nonlinear = sigmoid(local_sum)
 * output = weight × pre_activity × nonlinear × STP
 *
 * REQUIRES: Synapse neighborhood information in function_data
 *
 * BIOLOGICAL: NMDA spikes in dendrites (Schiller et al., 2000)
 *
 * COMPLEXITY: O(k) where k = dendritic branch size (~10-50)
 */
float synapse_compute_dendritic(
    struct synapse_t* syn,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_activity,
    struct synapse_compute_context_t* context
);

// ============================================================================
// BUILT-IN LEARNING FUNCTIONS
// ============================================================================

/**
 * @brief Three-factor learning rule
 *
 * Combines STDP, eligibility traces, and reward signals.
 *
 * ALGORITHM:
 * 1. Update eligibility trace with STDP
 * 2. Apply reward-modulated update: Δw = η × e × r
 *
 * WHERE:
 * - e = eligibility trace (decays with τ = 1 second)
 * - r = reward signal
 * - η = learning rate
 *
 * BIOLOGICAL: Dopaminergic reward learning in basal ganglia
 * (Reynolds et al., 2001; Schultz et al., 1997)
 */
void synapse_learn_three_factor(
    struct synapse_t* syn,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike_time,
    float post_spike_time,
    float reward_signal,
    struct synapse_compute_context_t* context
);

/**
 * @brief Attention-modulated learning
 *
 * Learning rate scaled by current attention weight.
 *
 * ALGORITHM:
 * attention = compute_attention_weight(syn)
 * Δw = η × attention × STDP(Δt)
 *
 * EFFECT: Synapses receiving attention learn faster
 *
 * USE CASE: Focus learning on relevant inputs
 */
void synapse_learn_attention_modulated(
    struct synapse_t* syn,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike_time,
    float post_spike_time,
    float reward_signal,
    struct synapse_compute_context_t* context
);

/**
 * @brief Meta-plasticity learning
 *
 * Learning rate adapts based on recent activity.
 *
 * ALGORITHM:
 * θ = sliding threshold (BCM-like)
 * η_effective = η_base × f(activity - θ)
 *
 * EFFECT: Active synapses become less plastic (consolidation)
 *
 * BIOLOGICAL: Metaplasticity (Abraham & Bear, 1996)
 */
void synapse_learn_metaplastic(
    struct synapse_t* syn,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike_time,
    float post_spike_time,
    float reward_signal,
    struct synapse_compute_context_t* context
);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Initialize synapse compute state
 *
 * Allocates and initializes extended memory if needed.
 *
 * @param state Pointer to state structure
 * @param extended_size Size of extended memory to allocate (0 = none)
 * @return 0 on success, -1 on allocation failure
 */
int synapse_compute_state_init(synapse_compute_state_t* state, uint32_t extended_size);

/**
 * @brief Cleanup synapse compute state
 *
 * Frees extended memory and calls cleanup_fn if registered.
 *
 * @param state Pointer to state structure
 */
void synapse_compute_state_cleanup(synapse_compute_state_t* state);

/**
 * @brief Set synapse compute function
 *
 * Helper to attach compute function to synapse with initialization.
 *
 * @param syn Synapse to modify
 * @param compute_fn Compute function to attach
 * @param learn_fn Learning function to attach (NULL = use default)
 * @param function_data Opaque data for function (NULL = none)
 * @param cleanup_fn Cleanup for function_data (NULL = none)
 * @return 0 on success, -1 on failure
 */
int synapse_set_compute_function(
    neural_network_t net,
    struct synapse_t* syn,
    synapse_compute_fn compute_fn,
    synapse_learn_fn learn_fn,
    void* function_data,
    void (*cleanup_fn)(void*)
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SYNAPSE_COMPUTE_H
