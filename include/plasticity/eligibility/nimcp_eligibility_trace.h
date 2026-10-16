//=============================================================================
// nimcp_eligibility_trace.h - Eligibility Traces for Temporal Credit Assignment
//=============================================================================
/**
 * @file nimcp_eligibility_trace.h
 * @brief Eligibility traces for bridging temporal gap in reinforcement learning
 *
 * WHAT: Exponentially decaying memory of synaptic activity
 * WHY: Solve distal reward problem (assign credit to past actions)
 * HOW: e(t) = λ × e(t-1) + δ(spike), then Δw = η × e × reward × dopamine
 *
 * BIOLOGICAL BASIS:
 *   - Izhikevich (2007): "Solving the distal reward problem through STDP"
 *   - Traces persist 100-1000ms after spike (λ = 0.9-0.99)
 *   - Dopamine gates trace-based learning (3-factor rule)
 *   - Frey & Morris (1997): "Tags and capture" - traces tag synapses for consolidation
 *
 * OPTION 2.2 ENHANCEMENT (Burst-Triggered Consolidation):
 *   - Traces accumulate during normal activity ("synaptic tags")
 *   - Weight changes occur ONLY during dopamine bursts ("capture")
 *   - Implements biologically-realistic credit assignment with temporal specificity
 *   - 3-factor + burst gating = 4-factor learning rule
 *
 * USE CASES:
 *   - Temporal difference learning: Q-learning, SARSA
 *   - Policy gradients: Actor-critic algorithms
 *   - Sequence learning: Credit assignment across time
 *   - Motor control: Delayed feedback (e.g., reaching task)
 *
 * @author NIMCP Development Team
 * @date 2025-11-08
 * @version 2.7.0 Phase 4 + Option 2.2
 */

#ifndef NIMCP_ELIGIBILITY_TRACE_H
#define NIMCP_ELIGIBILITY_TRACE_H

#include <stdint.h>
#include <stdbool.h>
#include "plasticity/neuromodulators/nimcp_phasic_tonic.h"  // For phasic_tonic_state_t (Option 2.2 integration)

// Forward declarations to avoid circular dependencies
typedef struct synapse_t synapse_t;

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Eligibility trace configuration
 *
 * WHAT: Parameters for trace decay and learning
 * WHY: Control temporal credit assignment window
 * HOW: Decay constant, learning rate, neuromodulation gating
 *
 * OPTION 2.2: Added burst-triggered consolidation mode
 */
typedef struct {
    float decay_lambda;       /**< Trace decay per timestep (default: 0.95) */
    float learning_rate;      /**< Base learning rate (default: 0.001) */
    bool use_neuromodulation; /**< Gate by dopamine (default: true) */
    float trace_threshold;    /**< Minimum trace for update (default: 0.01) */

    // Option 2.2: Burst-triggered consolidation
    bool burst_triggered_mode;/**< Only consolidate during dopamine bursts (default: false) */
    float burst_lr_multiplier;/**< Learning rate multiplier during bursts (default: 3.0) */
    float min_burst_concentration; /**< Min dopamine concentration to detect burst (default: 0.3) */
} eligibility_config_t;

/**
 * @brief Eligibility trace state for a synapse
 *
 * WHAT: Runtime eligibility trace value
 * WHY: Track synaptic eligibility for credit assignment
 * HOW: Decays exponentially, reset by spikes
 */
typedef struct eligibility_trace_t {
    float trace;              /**< Current trace value [0, 1] */
    uint64_t last_update;     /**< Last time trace was updated (ms) */
} eligibility_trace_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default eligibility trace configuration
 *
 * WHAT: Return sensible default parameters
 * WHY: Based on RL literature (Sutton & Barto 2018) + Option 2.2 enhancements
 * HOW: Static initialization
 *
 * DEFAULTS:
 * - λ = 0.95 (decay over ~20 timesteps, half-life ~14ms)
 * - η = 0.001 (modest learning rate)
 * - Use neuromodulation: true (dopamine gating)
 * - Trace threshold: 0.01 (ignore tiny traces)
 * - Burst-triggered mode: false (standard mode by default)
 * - Burst LR multiplier: 3.0 (3x learning during bursts)
 * - Min burst concentration: 0.3 (30% = ~6x baseline)
 *
 * @return Default configuration
 */
eligibility_config_t eligibility_default_config(void);

/**
 * @brief Initialize eligibility trace for synapse
 *
 * WHAT: Set trace to zero, record timestamp
 * WHY: Fresh start for learning
 * HOW: Direct initialization
 *
 * @param trace Trace structure to initialize
 * @param current_time Current simulation time (ms)
 */
void eligibility_trace_init(eligibility_trace_t* trace, uint64_t current_time);

//=============================================================================
// Trace Update Functions
//=============================================================================

/**
 * @brief Update eligibility trace with spike
 *
 * WHAT: Decay trace and add spike contribution
 * WHY: Mark synapse as recently active
 * HOW: e(t) = λ^Δt × e(t-1) + δ(spike)
 *
 * ALGORITHM:
 * ```
 * Δt = current_time - last_update
 * e = λ^Δt × e_old + spike_contribution
 * ```
 *
 * PERFORMANCE: O(1) - single power computation
 *
 * @param trace Eligibility trace
 * @param config Configuration
 * @param current_time Current time (ms)
 * @param spike_contribution Spike contribution (typically 1.0)
 */
void eligibility_trace_update(
    eligibility_trace_t* trace,
    const eligibility_config_t* config,
    uint64_t current_time,
    float spike_contribution
);

/**
 * @brief Decay eligibility trace without spike
 *
 * WHAT: Apply exponential decay for elapsed time
 * WHY: Traces naturally decay over time
 * HOW: e(t) = λ^Δt × e(t-1)
 *
 * USE CASE: Call every timestep to decay all traces
 *
 * @param trace Eligibility trace
 * @param config Configuration
 * @param current_time Current time (ms)
 */
void eligibility_trace_decay(
    eligibility_trace_t* trace,
    const eligibility_config_t* config,
    uint64_t current_time
);

//=============================================================================
// Learning Functions
//=============================================================================

/**
 * @brief Apply trace-based weight update
 *
 * WHAT: Update synapse weight using eligibility trace and reward
 * WHY: Assign credit to recently active synapses
 * HOW: Δw = η × e × reward × dopamine
 *
 * ALGORITHM:
 * ```
 * if (use_neuromodulation):
 *     Δw = learning_rate × trace × reward × dopamine
 * else:
 *     Δw = learning_rate × trace × reward
 * w_new = w_old + Δw
 * ```
 *
 * THREE-FACTOR RULE:
 * 1. Trace (eligibility): Was synapse recently active?
 * 2. Reward: Was outcome good/bad?
 * 3. Dopamine: Should we learn from this?
 *
 * PERFORMANCE: O(1) - simple arithmetic
 *
 * @param synapse Synapse to update
 * @param trace Eligibility trace for this synapse
 * @param config Configuration
 * @param reward Reward signal [-1, 1]
 * @param dopamine_level Dopamine concentration [0, 1]
 * @return Weight change (Δw)
 */
float eligibility_apply_reward(
    synapse_t* synapse,
    const eligibility_trace_t* trace,
    const eligibility_config_t* config,
    float reward,
    float dopamine_level
);

/**
 * @brief Combined trace update and learning
 *
 * WHAT: Decay trace, then apply reward-based learning
 * WHY: Convenience function for typical use case
 * HOW: Call eligibility_trace_decay(), then eligibility_apply_reward()
 *
 * TYPICAL USAGE:
 * ```
 * // Each timestep:
 * for (each synapse with trace):
 *     eligibility_update_and_learn(
 *         synapse, trace, config,
 *         current_time, reward, dopamine
 *     );
 * ```
 *
 * @param synapse Synapse to update
 * @param trace Eligibility trace
 * @param config Configuration
 * @param current_time Current time (ms)
 * @param reward Reward signal
 * @param dopamine_level Dopamine concentration
 * @return Weight change
 */
float eligibility_update_and_learn(
    synapse_t* synapse,
    eligibility_trace_t* trace,
    const eligibility_config_t* config,
    uint64_t current_time,
    float reward,
    float dopamine_level
);

//=============================================================================
// Burst-Triggered Consolidation (Option 2.2)
//=============================================================================

/**
 * @brief Consolidate eligibility traces to weight changes during dopamine burst
 *
 * WHAT: Convert accumulated traces to weight changes only during bursts
 * WHY: Implements "tags and capture" mechanism (Frey & Morris 1997)
 * HOW: Check burst state, apply amplified learning if in burst
 *
 * ALGORITHM:
 * ```
 * if (burst_triggered_mode):
 *     if (NOT in_dopamine_burst):
 *         return 0  // No consolidation outside bursts
 *     else:
 *         Δw = (learning_rate × burst_lr_multiplier) × trace × reward × dopamine
 * else:
 *     Δw = learning_rate × trace × reward × dopamine  // Standard mode
 * ```
 *
 * FOUR-FACTOR LEARNING RULE:
 * 1. Eligibility (trace): Was synapse recently active? (local)
 * 2. Reward: Was outcome good/bad? (global)
 * 3. Dopamine: Neuromodulator concentration (global)
 * 4. Burst state: Is dopamine in burst mode? (temporal gating)
 *
 * BIOLOGICAL JUSTIFICATION:
 * - Protein synthesis required for long-term potentiation
 * - Dopamine bursts trigger mRNA translation
 * - Without burst, traces remain as "tags" awaiting consolidation
 *
 * PERFORMANCE: O(1) - simple conditional and arithmetic
 *
 * @param synapse Synapse to potentially update
 * @param trace Eligibility trace for this synapse
 * @param config Configuration (includes burst_triggered_mode flag)
 * @param phasic_tonic Dopamine phasic-tonic state (for burst detection)
 * @param reward Reward signal [-1, 1]
 * @return Weight change (Δw), 0 if not in burst and burst_triggered_mode enabled
 */
float eligibility_consolidate_on_burst(
    synapse_t* synapse,
    const eligibility_trace_t* trace,
    const eligibility_config_t* config,
    const phasic_tonic_state_t* phasic_tonic,
    float reward
);

/**
 * @brief Check if currently in dopamine burst state
 *
 * WHAT: Detect if dopamine is in phasic burst mode
 * WHY: Determine whether to consolidate traces
 * HOW: Check phasic_tonic->in_burst_state or dopamine concentration threshold
 *
 * DETECTION STRATEGIES:
 * 1. Direct: Check phasic_tonic->in_burst_state flag (preferred)
 * 2. Threshold: dopamine_concentration > min_burst_concentration (fallback)
 *
 * @param phasic_tonic Dopamine phasic-tonic state
 * @param config Configuration (for min_burst_concentration threshold)
 * @return true if in burst state
 */
bool eligibility_is_in_burst(
    const phasic_tonic_state_t* phasic_tonic,
    const eligibility_config_t* config
);

/**
 * @brief Batch consolidate multiple synapses during dopamine burst
 *
 * WHAT: Apply burst-triggered consolidation to array of synapses
 * WHY: Efficient bulk processing during burst events
 * HOW: Single burst check, then iterate synapses
 *
 * TYPICAL USAGE:
 * ```
 * // When dopamine burst detected:
 * if (eligibility_is_in_burst(phasic_tonic, config)) {
 *     int num_consolidated = eligibility_consolidate_batch(
 *         synapses, traces, num_synapses,
 *         config, phasic_tonic, reward
 *     );
 * }
 * ```
 *
 * PERFORMANCE: O(n) where n = num_synapses, but with single burst check overhead
 *
 * @param synapses Array of synapses
 * @param traces Array of eligibility traces (parallel to synapses)
 * @param num_synapses Number of synapses to process
 * @param config Configuration
 * @param phasic_tonic Dopamine phasic-tonic state
 * @param reward Reward signal (same for all synapses)
 * @return Number of synapses with significant weight changes
 */
int eligibility_consolidate_batch(
    synapse_t* synapses,
    const eligibility_trace_t* traces,
    int num_synapses,
    const eligibility_config_t* config,
    const phasic_tonic_state_t* phasic_tonic,
    float reward
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current trace value
 *
 * WHAT: Return trace strength
 * WHY: Query eligibility for debugging/analysis
 * HOW: Direct field access
 *
 * @param trace Eligibility trace
 * @return Trace value [0, 1]
 */
float eligibility_get_trace(const eligibility_trace_t* trace);

/**
 * @brief Check if trace is significant
 *
 * WHAT: Test if trace exceeds threshold
 * WHY: Skip updates for negligible traces (performance)
 * HOW: Compare to config.trace_threshold
 *
 * @param trace Eligibility trace
 * @param config Configuration
 * @return true if trace > threshold
 */
bool eligibility_is_significant(
    const eligibility_trace_t* trace,
    const eligibility_config_t* config
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_ELIGIBILITY_TRACE_H
