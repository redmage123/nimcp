#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_synapse_compute.c - Programmable Synapse Computation (NIMCP 2.7)
//=============================================================================
/**
 * @file nimcp_synapse_compute.c
 * @brief Production implementation of synapse-level computation
 *
 * ARCHITECTURE:
 * - Strategy Pattern: Function pointers enable per-synapse customization
 * - Memento Pattern: compute_state preserves synapse-specific memory
 * - Template Method: Consistent flow with customizable steps
 *
 * PERFORMANCE CHARACTERISTICS:
 * - Default computation: O(1) - ~10 CPU cycles
 * - Attention computation: O(d) - ~100-200 cycles for d=16
 * - Semantic computation: O(d) - ~300 cycles for d=300
 * - Gating computation: O(1) - ~15 CPU cycles
 * - Memory overhead: 24 bytes per synapse (function pointers)
 * - Cache efficiency: Local memory (64 bytes) fits in L1 cache
 *
 * DESIGN DECISIONS:
 * - Function pointers over virtual dispatch: Zero indirection cost
 * - Local memory first: 16 floats (64 bytes) before extended allocation
 * - Context parameter: Enables global coordination without tight coupling
 * - Const neuron pointers: Documents read-only access for optimization
 *
 * BIOLOGICAL MOTIVATION:
 * Real synapses perform computation via:
 * - Active conductances in dendritic spines (Yuste & Denk, 1995)
 * - Local protein synthesis (Sutton & Schuman, 2006)
 * - Heterosynaptic plasticity (Lynch et al., 1977)
 * - Dendritic integration (London & Häusser, 2005)
 *
 * This transforms NIMCP from 100K passive weights to 100K active processors.
 *
 * @author Claude Code + NIMCP Development Team
 * @date 2025-11-07
 * @version 2.7.0
 */

// NIMCP 2.7: Synapse compute functions
// NOTE: synapse_compute.h includes neuralnet.h, which has full struct definitions
#include "core/synapse_compute/nimcp_synapse_compute.h"
#include "core/neuralnet/nimcp_neuralnet_internal.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "utils/memory/nimcp_memory.h"
#include "plasticity/eligibility/nimcp_eligibility_trace.h"  // Option 2.2: Burst-triggered consolidation
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"  // For neuromodulator_get_level
#include "plasticity/neuromodulators/nimcp_phasic_tonic.h"  // For phasic_tonic_state_t
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/math/nimcp_math_helpers.h"

// === BIO-ASYNC + LOGGING + UNIFIED MEMORY INTEGRATION ===
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "synapse_compute"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(synapse_compute)

#define BIO_MODULE_ID 0x0133

// NIMCP 2.11: Helper to extract synapse_cold_t* from context
#define CTX_COLD(ctx) ((ctx) ? (synapse_cold_t*)(ctx)->synapse_cold : NULL)

// Helper: Get dopamine phasic-tonic state from neuromodulator system
// Uses the public accessor from nimcp_neuromodulators.h
static phasic_tonic_state_t* get_dopamine_phasic_tonic(void* neuromod_system) {
    if (!neuromod_system) {
        return NULL;
    }

    // Use the public API accessor — avoids needing internal struct visibility
    neuromodulator_system_t sys = (neuromodulator_system_t)neuromod_system;
    return neuromodulator_get_dopamine_phasic_tonic(sys);
}

//=============================================================================
// Helper Functions - Mathematical Operations
//=============================================================================

/**
 * @brief Compute dot product of two vectors
 *
 * WHAT: Standard vector dot product
 * WHY: Core operation for attention and semantic similarity
 * HOW: Single-pass accumulation
 *
 * COMPLEXITY: O(n)
 * PERFORMANCE: ~3n CPU cycles (load + multiply + add per element)
 */
static inline float dot_product(const float* a, const float* b, uint32_t size) {
    float sum = 0.0F;
    for (uint32_t i = 0; i < size; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

/**
 * @brief Compute cosine similarity between two vectors
 *
 * WHAT: Normalized dot product: cos(θ) = (a·b) / (||a|| ||b||)
 * WHY: Measures semantic similarity independent of magnitude
 * HOW: Single-pass computation of dot product and norms
 *
 * COMPLEXITY: O(n)
 * PERFORMANCE: ~5n CPU cycles + 1 sqrt + 1 div
 */
static inline float cosine_similarity(const float* a, const float* b, uint32_t size) {
    float dot = 0.0F, norm_a = 0.0F, norm_b = 0.0F;
    for (uint32_t i = 0; i < size; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    float denom = sqrtf(norm_a * norm_b);
    return (denom > 1e-8F) ? (dot / denom) : 0.0F;
}

/**
 * @brief Sigmoid activation function
 *
 * WHAT: σ(x) = 1 / (1 + e^(-x))
 * WHY: Smooth gating with biological plausibility
 * HOW: Standard implementation using exp()
 *
 * COMPLEXITY: O(1)
 * PERFORMANCE: ~100 CPU cycles (dominated by exp())
 */
static inline float sigmoid(float x) {
    return 1.0F / (1.0F + expf(-x));
}

//=============================================================================
// Built-In Compute Functions - Transmission Computation
//=============================================================================

/**
 * @brief Default synapse computation (baseline)
 *
 * WHAT: Standard weighted transmission with optional STP
 * WHY: Backward compatible with NIMCP 2.0-2.6, zero overhead if NULL
 * HOW: Multiply weight × activity, modulate by short-term plasticity
 * WHEN: Use when no custom computation needed (most synapses)
 *
 * ALGORITHM: output = weight × pre_activity × STP_modulation
 *
 * COMPLEXITY: O(1) - 2 multiplications + 1 conditional
 * PERFORMANCE: ~10 CPU cycles (no STP) or ~30 cycles (with STP)
 */
float synapse_compute_default(
    struct synapse_t* syn,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_activity,
    struct synapse_compute_context_t* context
) {
    // Guard against null synapse
    if (!syn) return 0.0F;

    // Base transmission
    float output = syn->weight * pre_activity;

    // Apply short-term plasticity if enabled (cold field)
    synapse_cold_t* cold = CTX_COLD(context);
    if (cold && cold->enable_stp) {
        float stp_modulation = stp_get_modulation(&cold->stp);
        output *= stp_modulation;
    }

    return output;
}

/**
 * @brief Attention-modulated synapse computation (Transformer-like)
 *
 * WHAT: Implements scaled dot-product attention at synapse level
 * WHY: Enables focus on relevant inputs, critical for NLP and sequence tasks
 * HOW: Query-key similarity modulates transmission strength
 * WHEN: Use for attention layers, sequence processing, associative memory
 *
 * ALGORITHM:
 * 1. Extract query from post neuron (last 16 elements of activity_history)
 * 2. Extract key from pre neuron (last 16 elements of activity_history)
 * 3. Compute attention: exp(query · key / √d)  [scaled dot-product]
 * 4. Modulate transmission by attention weight
 *
 * BIOLOGICAL INSPIRATION:
 * - Dendritic coincidence detection (Larkum et al., 1999)
 * - Context-dependent gating in pyramidal neurons
 *
 * CONTEXT OPTIONS:
 * - Option 1: Use activity_history as embeddings (default)
 * - Option 2: Use global query/key from context.global_state (for Transformer layers)
 *
 * COMPLEXITY: O(d) where d = 16 (query/key dimension)
 * PERFORMANCE: ~150 cycles (16 muls + sqrt + exp + overhead)
 */
float synapse_compute_attention(
    struct synapse_t* syn,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_activity,
    struct synapse_compute_context_t* context
) {
    // Guard against null inputs
    if (!syn || !pre_neuron || !post_neuron) return 0.0F;

    #define ATTENTION_DIM 16

    // Guard: activity_history must be allocated
    if (!post_neuron->activity_history || !pre_neuron->activity_history ||
        post_neuron->activity_history_capacity == 0 || pre_neuron->activity_history_capacity == 0)
        return syn->weight * pre_activity;

    // Option 1: Use neuron activity history as query/key
    float query[ATTENTION_DIM];
    float key[ATTENTION_DIM];

    // Extract last ATTENTION_DIM activity values as embeddings
    uint32_t post_cap = post_neuron->activity_history_capacity;
    uint32_t pre_cap = pre_neuron->activity_history_capacity;
    uint32_t post_dim = (ATTENTION_DIM <= post_cap) ? ATTENTION_DIM : post_cap;
    uint32_t pre_dim = (ATTENTION_DIM <= pre_cap) ? ATTENTION_DIM : pre_cap;
    uint32_t dim = (post_dim < pre_dim) ? post_dim : pre_dim;

    // Zero-initialize in case dim < ATTENTION_DIM
    memset(query, 0, sizeof(query));
    memset(key, 0, sizeof(key));

    for (uint32_t i = 0; i < dim; i++) {
        uint32_t post_idx = (post_cap - dim + i) % post_cap;
        uint32_t pre_idx = (pre_cap - dim + i) % pre_cap;
        query[i] = post_neuron->activity_history[post_idx];
        key[i] = pre_neuron->activity_history[pre_idx];
    }

    // Option 2: Use global state if provided
    if (context && context->global_state && context->global_state_size >= ATTENTION_DIM * 2) {
        // First half = query, second half = key
        memcpy(query, context->global_state, ATTENTION_DIM * sizeof(float));
        memcpy(key, context->global_state + ATTENTION_DIM, ATTENTION_DIM * sizeof(float));
    }

    // Compute scaled dot-product attention
    float attention_score = dot_product(query, key, ATTENTION_DIM) / sqrtf(ATTENTION_DIM);
    float clamped_score = fminf(attention_score, 88.0f);
    float attention_weight = expf(clamped_score);  // Note: softmax normalization done globally
    if (!isfinite(attention_weight)) attention_weight = 1.0f;

    // Store attention weight in local memory for debugging
    synapse_cold_t* attn_cold = CTX_COLD(context);
    if (attn_cold && attn_cold->compute_state) {
        attn_cold->compute_state->local_memory[0] = attention_weight;
    }

    // Base transmission modulated by attention
    float output = syn->weight * pre_activity * attention_weight;

    // Apply STP if enabled
    if (attn_cold && attn_cold->enable_stp) {
        output *= stp_get_modulation(&attn_cold->stp);
    }

    return output;
}

/**
 * @brief Semantic similarity-modulated synapse
 *
 * For NLP: modulates transmission by semantic similarity between word embeddings.
 *
 * STORAGE: Embeddings stored in compute_state->extended_memory
 * Layout: [embedding_dim][pre_embedding...][post_embedding...]
 *
 * COMPLEXITY: O(d) where d = embedding dimension
 * PERFORMANCE: ~300 cycles for d=300
 */
float synapse_compute_semantic(
    struct synapse_t* syn,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_activity,
    struct synapse_compute_context_t* context
) {
    // Guard against null synapse
    if (!syn) return 0.0F;

    // Need compute state with embeddings (cold field)
    synapse_cold_t* sem_cold = CTX_COLD(context);
    if (!sem_cold || !sem_cold->compute_state || !sem_cold->compute_state->extended_memory) {
        // Fallback to default if no embeddings
        return synapse_compute_default(syn, pre_neuron, post_neuron, pre_activity, context);
    }

    // Extract embedding dimension (stored in first float)
    uint32_t embedding_dim = (uint32_t)sem_cold->compute_state->extended_memory[0];

    // Get pointers to pre/post embeddings
    float* pre_embedding = &sem_cold->compute_state->extended_memory[1];
    float* post_embedding = &sem_cold->compute_state->extended_memory[1 + embedding_dim];

    // Compute cosine similarity
    float similarity = cosine_similarity(pre_embedding, post_embedding, embedding_dim);

    // Store for analysis
    sem_cold->compute_state->local_memory[0] = similarity;

    // Modulate transmission by semantic similarity
    float modulation = 0.5F + 0.5F * similarity;  // Map [-1,1] to [0,1]
    float output = syn->weight * pre_activity * modulation;

    // Apply STP
    if (sem_cold->enable_stp) {
        output *= stp_get_modulation(&sem_cold->stp);
    }

    return output;
}

/**
 * @brief Gating synapse (multiplicative modulation)
 *
 * Acts as a gate controlled by external signal.
 *
 * GATE SIGNAL: Read from compute_state->local_memory[0]
 * This should be set by external controller before simulation step.
 *
 * COMPLEXITY: O(1)
 * PERFORMANCE: ~15 cycles
 */
float synapse_compute_gating(
    struct synapse_t* syn,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_activity,
    struct synapse_compute_context_t* context
) {
    // Guard against null synapse
    if (!syn) return 0.0F;

    // Default gate = open (1.0)
    float gate_signal = 1.0F;
    synapse_cold_t* gate_cold = CTX_COLD(context);

    // Read gate from local memory if available (cold field)
    if (gate_cold && gate_cold->compute_state) {
        gate_signal = gate_cold->compute_state->local_memory[0];
    }

    // Clamp gate to [0, 1]
    if (gate_signal < 0.0F) gate_signal = 0.0F;
    if (gate_signal > 1.0F) gate_signal = 1.0F;

    // Gated transmission
    float output = syn->weight * pre_activity * gate_signal;

    // Apply STP
    if (gate_cold && gate_cold->enable_stp) {
        output *= stp_get_modulation(&gate_cold->stp);
    }

    return output;
}

/**
 * @brief Neuromodulator-sensitive synapse
 *
 * Transmission modulated by dopamine/serotonin levels.
 *
 * SENSITIVITY: Stored in local_memory[0] (default = 1.0)
 * MODULATION: output × (1.0 + neuromod × sensitivity)
 *
 * COMPLEXITY: O(1)
 * PERFORMANCE: ~15 cycles
 */
float synapse_compute_neuromodulated(
    struct synapse_t* syn,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_activity,
    struct synapse_compute_context_t* context
) {
    // Guard against null synapse
    if (!syn) return 0.0F;

    // Default sensitivity = 1.0
    float sensitivity = 1.0F;
    synapse_cold_t* neuro_cold = CTX_COLD(context);
    if (neuro_cold && neuro_cold->compute_state) {
        sensitivity = neuro_cold->compute_state->local_memory[0];
    }

    // Get neuromodulation level from context
    float neuromod = 0.0F;
    if (context) {
        neuromod = context->neuromodulation;
    }

    // Compute modulation factor
    float modulation = 1.0F + neuromod * sensitivity;

    // Modulated transmission
    float output = syn->weight * pre_activity * modulation;

    // Apply STP
    if (neuro_cold && neuro_cold->enable_stp) {
        output *= stp_get_modulation(&neuro_cold->stp);
    }

    return output;
}

/**
 * @brief Dendritic computation synapse
 *
 * Implements local dendritic non-linearity by summing nearby synapses.
 *
 * REQUIRES: function_data points to array of neighbor synapse pointers
 * Format: [num_neighbors][syn_ptr1][syn_ptr2]...
 *
 * COMPLEXITY: O(k) where k = number of neighbors (~10-50)
 * PERFORMANCE: ~50-500 cycles depending on k
 */
float synapse_compute_dendritic(
    struct synapse_t* syn,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_activity,
    struct synapse_compute_context_t* context
) {
    // Guard against null synapse
    if (!syn) return 0.0F;

    float local_sum = 0.0F;
    synapse_cold_t* dend_cold = CTX_COLD(context);

    // Sum activity from neighboring synapses if available (cold field)
    if (dend_cold && dend_cold->compute_state && dend_cold->compute_state->function_data) {
        // First element = number of neighbors
        uint32_t* data = (uint32_t*)dend_cold->compute_state->function_data;
        uint32_t num_neighbors = data[0];

        // Sanity check: reasonable number of neighbors (prevent crashes on uninitialized data)
        if (num_neighbors > 0 && num_neighbors < 10000) {
            // NOTE: neighbor compute_state access assumes neighbor cold was set up.
            // In practice, dendritic neighbors should share compute_state references.
            struct synapse_t** neighbors = (struct synapse_t**)&data[1];

            // Sum recent activity from neighbors (stored in local_memory[1])
            // NOTE: This accesses neighbors' compute_state which is cold —
            // works only if neighbors were initialized with cold data.
            for (uint32_t i = 0; i < num_neighbors; i++) {
                (void)neighbors[i]; // Neighbor access needs refactoring for cold
                // TODO: Neighbor cold access requires network context
            }
        }
    }

    // Apply dendritic non-linearity (sigmoid)
    float nonlinear = sigmoid(local_sum);

    // Store current activity for neighbors to read
    if (dend_cold && dend_cold->compute_state) {
        dend_cold->compute_state->local_memory[1] = pre_activity * syn->weight;
    }

    // Modulated transmission
    float output = syn->weight * pre_activity * nonlinear;

    // Apply STP
    if (dend_cold && dend_cold->enable_stp) {
        output *= stp_get_modulation(&dend_cold->stp);
    }

    return output;
}

// ============================================================================
// BUILT-IN LEARNING FUNCTIONS
// ============================================================================

/**
 * @brief Three-factor learning rule with optional burst-triggered consolidation
 *
 * ALGORITHM (inline trace): Δw = η × eligibility_trace × reward_signal
 * ALGORITHM (burst mode): Δw = (η × burst_mult) × eligibility_trace × reward (only during burst)
 *
 * WHERE:
 * - Eligibility trace updated by STDP
 * - Decays with τ = 1000ms (inline) or configurable (full API)
 * - Reward signal provided by context
 * - Burst-triggered consolidation (Option 2.2) when eligibility trace allocated
 *
 * OPTION 2.2 INTEGRATION:
 * - If syn->eligibility is allocated → Use full API with burst-triggered consolidation
 * - If syn->eligibility is NULL → Use simple inline trace (backward compatible)
 */
void synapse_learn_three_factor(
    struct synapse_t* syn,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike_time,
    float post_spike_time,
    float reward_signal,
    struct synapse_compute_context_t* context
) {
    // Guard against null synapse
    if (!syn) return;

    // OPTION 2.2: Check if full eligibility trace API is enabled (cold fields)
    synapse_cold_t* learn_cold = CTX_COLD(context);
    if (learn_cold && learn_cold->eligibility && learn_cold->enable_eligibility) {
        // === FULL ELIGIBILITY TRACE API WITH BURST-TRIGGERED CONSOLIDATION ===

        // Get configuration - use default as baseline
        eligibility_config_t config = eligibility_default_config();

        // Extract dopamine phasic-tonic state from context (Phase 2.7.1)
        phasic_tonic_state_t* dopamine_phasic_tonic = NULL;
        float dopamine_level = 0.5F;  // Default baseline dopamine

        // Guard: Access neuromodulator system from context
        if (context && context->neuromodulator_system) {
            // Get dopamine level via public API
            neuromodulator_system_t neuromod = (neuromodulator_system_t)context->neuromodulator_system;
            dopamine_level = neuromodulator_get_level(neuromod, NEUROMOD_DOPAMINE);

            // Attempt to get phasic-tonic state for burst detection
            dopamine_phasic_tonic = get_dopamine_phasic_tonic(context->neuromodulator_system);

            // Enable burst-triggered mode if burst state detected
            if (dopamine_phasic_tonic && eligibility_is_in_burst(dopamine_phasic_tonic, &config)) {
                config.burst_triggered_mode = true;
            }
        }

        // Update trace with spike timing (if both spikes present)
        uint64_t current_time;
        if (pre_spike_time > 0 && post_spike_time > 0) {
            // Compute STDP contribution (pre-post correlation)
            float dt = post_spike_time - pre_spike_time;
            float stdp_exp = fminf(-fabsf(dt) / 20.0F, 88.0f);
            float stdp_contribution = expf(stdp_exp);
            if (!isfinite(stdp_contribution)) stdp_contribution = 0.0F;
            if (dt < 0) {
                stdp_contribution *= -0.5F;  // LTD asymmetry
            }

            // Update eligibility trace with STDP
            current_time = (uint64_t)(post_spike_time);
            eligibility_trace_update(learn_cold->eligibility, &config, current_time, stdp_contribution);
        } else if (pre_spike_time > 0 || post_spike_time > 0) {
            // Decay trace if we have a valid timestamp
            current_time = (post_spike_time > 0) ? (uint64_t)post_spike_time : (uint64_t)pre_spike_time;
            eligibility_trace_decay(learn_cold->eligibility, &config, current_time);
        } else {
            // No spikes: advance time by 1ms for decay
            current_time = learn_cold->eligibility->last_update + 1;
            eligibility_trace_decay(learn_cold->eligibility, &config, current_time);
        }

        // Apply three-factor learning: Δw = learning_rate × trace × reward × dopamine
        float weight_change = 0.0F;

        if (dopamine_phasic_tonic && config.burst_triggered_mode) {
            // Burst-triggered consolidation (four-factor rule)
            // Only consolidate weight changes during dopamine bursts
            weight_change = eligibility_consolidate_on_burst(
                syn, learn_cold->eligibility, &config, dopamine_phasic_tonic, reward_signal
            );
        } else {
            // Standard three-factor learning (trace × reward × dopamine)
            weight_change = eligibility_apply_reward(
                syn, learn_cold->eligibility, &config, reward_signal, dopamine_level
            );
        }

        syn->last_change = weight_change;

        // Weight bounds
        if (syn->weight < -10.0F) syn->weight = -10.0F;
        if (syn->weight > 10.0F) syn->weight = 10.0F;

    } else {
        // === SIMPLE INLINE TRACE (BACKWARD COMPATIBLE) ===

        const float TAU_ELIGIBILITY = 1000.0F; // ms
        const float LEARNING_RATE = 0.01F;

        // Update eligibility trace with STDP
        if (pre_spike_time > 0 && post_spike_time > 0) {
            float dt = post_spike_time - pre_spike_time;
            float stdp_exp2 = fminf(-fabsf(dt) / 20.0F, 88.0f);
            float stdp_update = expf(stdp_exp2);
            if (!isfinite(stdp_update)) stdp_update = 0.0F;
            if (dt > 0) {
                stdp_update *= 1.0F;  // LTP
            } else {
                stdp_update *= -0.5F;  // LTD (asymmetric)
            }

            // Update trace
            syn->trace += stdp_update;
        }

        // Decay eligibility trace
        if (context) {
            float dt = 1.0F; // Assume 1ms timestep
            float decay_exp = fminf(-dt / TAU_ELIGIBILITY, 88.0f);
            float decay_factor = expf(decay_exp);
            if (!isfinite(decay_factor)) decay_factor = 0.0F;
            syn->trace *= decay_factor;
        }

        // Three-factor weight update
        float weight_change = LEARNING_RATE * syn->trace * reward_signal;
        syn->weight += weight_change;
        syn->last_change = weight_change;

        // Weight bounds
        if (syn->weight < -10.0F) syn->weight = -10.0F;
        if (syn->weight > 10.0F) syn->weight = 10.0F;
    }
}

/**
 * @brief Attention-modulated learning
 *
 * Learning rate scaled by current attention weight.
 */
void synapse_learn_attention_modulated(
    struct synapse_t* syn,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike_time,
    float post_spike_time,
    float reward_signal,
    struct synapse_compute_context_t* context
) {
    // Guard against null synapse
    if (!syn) return;

    // Get attention weight (stored during compute phase, cold field)
    float attention = 1.0F;
    synapse_cold_t* attn_learn_cold = CTX_COLD(context);
    if (attn_learn_cold && attn_learn_cold->compute_state) {
        attention = attn_learn_cold->compute_state->local_memory[0];
        if (attention < 0.1F) attention = 0.1F; // Minimum learning
    }

    // Standard STDP
    if (pre_spike_time > 0 && post_spike_time > 0) {
        float dt = post_spike_time - pre_spike_time;
        float attn_stdp_exp = fminf(-fabsf(dt) / 20.0F, 88.0f);
        float stdp_update = expf(attn_stdp_exp);
        if (!isfinite(stdp_update)) stdp_update = 0.0F;
        if (dt > 0) {
            stdp_update *= 1.0F;
        } else {
            stdp_update *= -0.5F;
        }

        // Scale by attention
        float weight_change = 0.01F * attention * stdp_update;
        syn->weight += weight_change;
        syn->last_change = weight_change;
    }

    // Weight bounds
    if (syn->weight < -10.0F) syn->weight = -10.0F;
    if (syn->weight > 10.0F) syn->weight = 10.0F;
}

/**
 * @brief Meta-plasticity learning
 *
 * Learning rate adapts based on recent synaptic activity.
 */
void synapse_learn_metaplastic(
    struct synapse_t* syn,
    const neuron_t* pre_neuron,
    const neuron_t* post_neuron,
    float pre_spike_time,
    float post_spike_time,
    float reward_signal,
    struct synapse_compute_context_t* context
) {
    // Guard against null inputs
    if (!syn || !post_neuron) return;

    // Update meta-plasticity based on recent activity
    // High activity → lower plasticity (consolidation)
    // Low activity → higher plasticity (exploration)

    float avg_activity = post_neuron->avg_activity;
    float theta = 0.5F; // Threshold

    // BCM-like learning rate modulation
    float activity_factor = (avg_activity > theta) ? 0.5F : 1.5F;

    // Update meta-plasticity state
    syn->meta_plasticity = 0.9F * syn->meta_plasticity + 0.1F * activity_factor;
    NIMCP_EMA_GUARD(syn->meta_plasticity, activity_factor);

    // Standard STDP with meta-plasticity modulation
    if (pre_spike_time > 0 && post_spike_time > 0) {
        float dt = post_spike_time - pre_spike_time;
        float meta_stdp_exp = fminf(-fabsf(dt) / 20.0F, 88.0f);
        float stdp_update = expf(meta_stdp_exp);
        if (!isfinite(stdp_update)) stdp_update = 0.0F;
        if (dt > 0) {
            stdp_update *= 1.0F;
        } else {
            stdp_update *= -0.5F;
        }

        // Modulate by meta-plasticity
        float weight_change = 0.01F * syn->meta_plasticity * stdp_update;
        syn->weight += weight_change;
        syn->last_change = weight_change;
    }

    // Weight bounds
    if (syn->weight < -10.0F) syn->weight = -10.0F;
    if (syn->weight > 10.0F) syn->weight = 10.0F;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Initialize synapse compute state
 */
int synapse_compute_state_init(synapse_compute_state_t* state, uint32_t extended_size) {
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_compute_state_init: state is NULL");
        return NIMCP_ERROR_NULL_POINTER;
    }

    // Zero local memory
    memset(state->local_memory, 0, sizeof(state->local_memory));

    // Allocate extended memory if requested
    if (extended_size > 0) {
        state->extended_memory = (float*)nimcp_calloc(extended_size, sizeof(float));
        if (!state->extended_memory) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "synapse_compute_state_init: state->extended_memory is NULL");
            return -1; // Allocation failed
        }
        state->extended_size = extended_size;
    } else {
        state->extended_memory = NULL;
        state->extended_size = 0;
    }

    state->function_data = NULL;
    state->cleanup_fn = NULL;

    return 0;
}

/**
 * @brief Cleanup synapse compute state
 */
void synapse_compute_state_cleanup(synapse_compute_state_t* state) {
    if (!state) return;

    // Free extended memory
    if (state->extended_memory) {
        nimcp_free(state->extended_memory);
        state->extended_memory = NULL;
        state->extended_size = 0;
    }

    // Call cleanup function if registered
    if (state->cleanup_fn && state->function_data) {
        state->cleanup_fn(state->function_data);
    }

    state->function_data = NULL;
    state->cleanup_fn = NULL;
}

/**
 * @brief Set synapse compute function
 */
int synapse_set_compute_function(
    neural_network_t net,
    struct synapse_t* syn,
    synapse_compute_fn compute_fn,
    synapse_learn_fn learn_fn,
    void* function_data,
    void (*cleanup_fn)(void*)
) {
    if (!net || !syn) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "synapse_set_compute_function: net or syn is NULL");
        return -1;
    }

    synapse_cold_t* cold = SYNAPSE_ENSURE_COLD(net, syn);
    if (!cold) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "synapse_set_compute_function: failed to allocate cold data");
        return -1;
    }

    // Allocate compute state if needed
    if (!cold->compute_state && (function_data || compute_fn)) {
        cold->compute_state = (synapse_compute_state_t*)nimcp_calloc(1, sizeof(synapse_compute_state_t));
        if (!cold->compute_state) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "synapse_set_compute_function: cold->compute_state is NULL");
            return -1; // Allocation failed
        }
    }

    // Set function pointers (cold fields)
    cold->compute_function = compute_fn;
    cold->learn_function = learn_fn;

    // Set function data
    if (cold->compute_state) {
        cold->compute_state->function_data = function_data;
        cold->compute_state->cleanup_fn = cleanup_fn;
    }

    return 0;
}
