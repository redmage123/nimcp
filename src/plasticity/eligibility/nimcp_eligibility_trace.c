#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_eligibility_trace.c - Eligibility Trace Implementation
//=============================================================================
/**
 * @file nimcp_eligibility_trace.c
 * @brief Implementation of eligibility traces for temporal credit assignment
 *
 * WHAT: Exponentially decaying memory of synaptic activity
 * WHY: Bridge temporal gap between actions and rewards
 * HOW: e(t) = λ^Δt × e(t-1) + δ(spike)
 *
 * ALGORITHM:
 * ```
 * // Update trace with spike:
 * Δt = current_time - last_update
 * e = λ^Δt × e_old + spike_contribution
 *
 * // Apply reward (standard mode):
 * Δw = η × e × reward × dopamine
 *
 * // Apply reward (burst-triggered mode - Option 2.2):
 * if (in_burst):
 *     Δw = (η × burst_multiplier) × e × reward × dopamine
 * else:
 *     Δw = 0  // No consolidation outside bursts
 * ```
 *
 * PERFORMANCE:
 * - eligibility_trace_update: O(1) - single power computation
 * - eligibility_apply_reward: O(1) - arithmetic only
 * - eligibility_consolidate_on_burst: O(1) - arithmetic + burst check
 *
 * @author NIMCP Development Team
 * @date 2025-11-08
 * @version 2.7.0 Phase 4 + Option 2.2
 */

#include "plasticity/eligibility/nimcp_eligibility_trace.h"
#include "plasticity/neuromodulators/nimcp_phasic_tonic.h"  // For burst detection
#include "core/neuralnet/nimcp_neuralnet.h"  // For complete synapse_t definition
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "security/nimcp_security.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>

#define LOG_MODULE "plasticity_eligibility"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(eligibility_trace)

// Note: synapse_t and phasic_tonic_state_t forward-declared in header, full definitions needed here

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default eligibility trace configuration
 *
 * WHAT: Return RL-standard parameters
 * WHY: Based on Sutton & Barto (2018) recommendations
 * HOW: Static initialization with empirical values
 *
 * @return Default configuration structure
 */
eligibility_config_t eligibility_default_config(void) {
    eligibility_config_t config = {
        .decay_lambda = 0.95F,        // 95% decay per timestep (~20 step memory)
        .learning_rate = 0.001F,      // Modest learning rate
        .use_neuromodulation = true,  // Dopamine gating enabled
        .trace_threshold = 0.01F,     // Ignore traces < 1%

        // Option 2.2: Burst-triggered consolidation (disabled by default)
        .burst_triggered_mode = false, // Standard mode (consolidate anytime)
        .burst_lr_multiplier = 3.0F,   // 3x learning rate during bursts
        .min_burst_concentration = 0.3F // 30% dopamine = burst threshold
    };
    return config;
}

/**
 * @brief Initialize eligibility trace for synapse
 *
 * WHAT: Set trace to zero, record timestamp
 * WHY: Fresh start for learning
 * HOW: Direct field initialization
 *
 * @param trace Trace structure to initialize
 * @param current_time Current simulation time (ms)
 */
void eligibility_trace_init(eligibility_trace_t* trace, uint64_t current_time) {
    // Guard: NULL trace
    if (!trace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_trace_init: trace is NULL");
        return;
    }

    // WHAT: Initialize trace to zero
    // WHY: No eligibility at start
    // HOW: Direct assignment
    trace->trace = 0.0F;
    trace->last_update = current_time;
}

//=============================================================================
// Trace Update Functions
//=============================================================================

/**
 * @brief Update eligibility trace with spike
 *
 * WHAT: Decay trace for elapsed time, add spike contribution
 * WHY: Mark synapse as recently active
 * HOW: e(t) = λ^Δt × e(t-1) + spike_contribution
 *
 * ALGORITHM:
 * 1. Compute Δt = current_time - last_update
 * 2. Decay: e × λ^Δt (exponential decay)
 * 3. Add spike: e + spike_contribution
 * 4. Update timestamp
 *
 * PERFORMANCE: O(1) - single pow() call
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
) {
    // Guard: NULL trace or config
    if (!trace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_trace_update: trace is NULL");
        return;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_trace_update: config is NULL");
        return;
    }

    // STEP 1: Compute time elapsed since last update
    // WHAT: Δt = current_time - last_update
    // WHY: Determine how much decay to apply
    // HOW: Unsigned subtraction with wraparound guard
    // P1 fix: Improved wraparound handling that allows decay to continue
    // WHY:  Original code set delta_t = 0 after wraparound, skipping decay entirely
    // HOW:  On wraparound, compute delta from 0 to current_time (partial wrap)
    uint64_t delta_t;
    if (current_time < trace->last_update) {
        /* Time counter wrapped around - use current_time as delta if small,
         * otherwise treat as reset scenario */
        if (current_time < 1000) {
            delta_t = current_time;  /* Small time since wrap point */
        } else {
            trace->last_update = current_time;
            delta_t = 0;  /* Large value after wrap - treat as reset */
        }
    } else {
        delta_t = current_time - trace->last_update;
    }

    // STEP 2: Apply exponential decay
    // WHAT: Decay trace by λ^Δt
    // WHY: Traces fade over time
    // HOW: Use powf() for exact exponential
    // OPTIMIZATION: Could use lookup table for common Δt values
    if (delta_t > 0) {
        float decay_factor = powf(config->decay_lambda, (float)delta_t);
        // NUMERICAL STABILITY: Early cutoff for large delta_t
        // WHAT: Zero out trace if decay becomes negligible
        // WHY: Avoid denormal floats and numerical errors
        // HOW: Threshold at 1e-6 (0.0001% of original value)
        if (decay_factor < 1e-6f) {
            trace->trace = 0.0f;
        } else {
            trace->trace *= decay_factor;
        }
    }

    // STEP 3: Add spike contribution with overflow protection
    // WHAT: Increment trace by spike amount with bounds checking
    // WHY: Spike marks synapse as eligible; prevent unbounded growth
    // HOW: Validate spike_contribution, add, then clamp
    // NUMERICAL STABILITY: Check for NaN/Inf in spike_contribution
    if (isnan(spike_contribution) || isinf(spike_contribution)) {
        spike_contribution = 0.0f;  /* Skip corrupted contribution */
    }

    /* Clamp spike contribution to reasonable range [-1, 1]
     * Negative contributions represent LTD (post-before-pre STDP) */
    if (spike_contribution < -1.0f) spike_contribution = -1.0f;
    if (spike_contribution > 1.0f) spike_contribution = 1.0f;

    trace->trace += spike_contribution;

    // STEP 4: Clamp trace to [-1, 1]
    // WHAT: Prevent trace from exceeding bounds
    // WHY: Traces represent eligibility for LTP (+) or LTD (-)
    // HOW: Explicit bounds checking for both limits
    if (trace->trace < -1.0F) {
        trace->trace = -1.0F;
    }
    if (trace->trace > 1.0F) {
        trace->trace = 1.0F;
    }

    /* Final NaN check to ensure numerical stability */
    if (isnan(trace->trace)) {
        trace->trace = 0.0F;
    }

    // STEP 5: Update timestamp
    // WHAT: Record this update time
    // WHY: Needed for next decay calculation
    // HOW: Direct assignment
    trace->last_update = current_time;
}

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
) {
    // Guard: NULL trace or config
    if (!trace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_trace_decay: trace is NULL");
        return;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_trace_decay: config is NULL");
        return;
    }

    // PARAMETER VALIDATION: Ensure decay_lambda is in valid range
    // WHAT: Validate decay_lambda is in (0, 1] range
    // WHY:  Invalid lambda values cause numerical instability
    // HOW:  Clamp or skip if out of range
    float decay_lambda = config->decay_lambda;
    if (isnan(decay_lambda) || decay_lambda <= 0.0f || decay_lambda > 1.0f) {
        /* Invalid decay parameter - skip decay to preserve trace */
        return;
    }

    // WHAT: Compute and apply exponential decay
    // WHY: Update trace to current time
    // HOW: Same as eligibility_trace_update but without spike contribution
    // P1 fix: Improved wraparound handling that allows decay to continue
    // WHY:  Original code skipped decay entirely on wraparound, causing trace
    //       accumulation without decay for potentially long periods
    // HOW:  On wraparound, compute delta from 0 to current_time (partial wrap)
    //       This maintains decay behavior while handling the discontinuity
    uint64_t delta_t;
    if (current_time < trace->last_update) {
        /* Time counter wrapped around - two options:
         * 1. If current_time is small, treat as fresh start (delta = current_time)
         * 2. If current_time is large, likely a reset occurred
         * Use 1 second threshold (1000ms) to distinguish */
        if (current_time < 1000) {
            /* Fresh start after wraparound - small delta since wrap point */
            delta_t = current_time;
        } else {
            /* Large current_time after wraparound - reset to current state */
            trace->last_update = current_time;
            delta_t = 0;
        }
    } else {
        delta_t = current_time - trace->last_update;
    }

    if (delta_t > 0) {
        float decay_factor = powf(decay_lambda, (float)delta_t);

        // NUMERICAL STABILITY: Validate decay_factor result
        if (isnan(decay_factor) || isinf(decay_factor)) {
            trace->trace = 0.0f;  /* Reset on numerical error */
            trace->last_update = current_time;
            return;
        }

        // NUMERICAL STABILITY: Early cutoff for large delta_t
        // WHAT: Zero out trace if decay becomes negligible
        // WHY: Avoid denormal floats and numerical errors
        // HOW: Threshold at 1e-6 (0.0001% of original value)
        if (decay_factor < 1e-6f) {
            trace->trace = 0.0f;
        } else {
            trace->trace *= decay_factor;
        }
        trace->last_update = current_time;
    }

    // OPTIMIZATION: If trace becomes negligible, zero it out
    // WHAT: Set very small traces (absolute value) to zero
    // WHY: Avoid denormal floating-point performance issues
    // HOW: Threshold at 0.0001 (0.01%) of magnitude
    if (fabsf(trace->trace) < 0.0001F) {
        trace->trace = 0.0F;
    }

    /* Final bounds enforcement - allow negative for LTD */
    if (trace->trace < -1.0F) trace->trace = -1.0F;
    if (trace->trace > 1.0F) trace->trace = 1.0F;
}

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
 * THREE-FACTOR RULE:
 * 1. Eligibility (e): Was synapse recently active?
 * 2. Reward (r): Was outcome good/bad?
 * 3. Dopamine (d): Should we learn from this?
 *
 * ALGORITHM:
 * ```
 * if (trace < threshold): return 0  // Skip negligible traces
 * if (use_neuromodulation):
 *     Δw = η × e × r × d
 * else:
 *     Δw = η × e × r
 * w_new = w_old + Δw
 * ```
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
) {
    // Guard: NULL synapse, trace, or config
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_apply_reward: synapse is NULL");
        return 0.0F;
    }
    if (!trace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_apply_reward: trace is NULL");
        return 0.0F;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_apply_reward: config is NULL");
        return 0.0F;
    }

    // OPTIMIZATION: Skip negligible traces
    // WHAT: Check if trace exceeds threshold (absolute value)
    // WHY: Avoid wasting computation on tiny weight changes
    // HOW: Compare absolute value to config.trace_threshold
    if (fabsf(trace->trace) < config->trace_threshold) {
        return 0.0F;
    }

    // STEP 1: Compute base weight change
    // WHAT: Δw = learning_rate × trace × reward
    // WHY: Scale by eligibility and reward magnitude
    // HOW: Simple multiplication
    float delta_w = config->learning_rate * trace->trace * reward;

    // STEP 2: Apply neuromodulation gating (if enabled)
    // WHAT: Multiply by dopamine level
    // WHY: Dopamine gates plasticity (3-factor rule)
    // HOW: Conditional multiplication
    if (config->use_neuromodulation) {
        delta_w *= dopamine_level;
    }

    // STEP 3: Update synapse weight
    // WHAT: Apply weight change
    // WHY: Implement learning
    // HOW: w_new = w_old + Δw
    // P1 fix: Check for NaN/Inf before weight update
    if (isnan(delta_w) || isinf(delta_w)) {
        delta_w = 0.0f;  /* Skip corrupted update */
    }
    synapse->weight += delta_w;

    // NOTE: Weight clamping is handled by the caller (synapse_learn_three_factor)
    // which uses [-10, 10] bounds. We only validate for NaN/Inf here.
    if (isnan(synapse->weight) || isinf(synapse->weight)) {
        synapse->weight = 0.5f;  /* Reset on numerical error */
    }

    return delta_w;
}

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
) {
    // Guard: NULL checks
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_update_and_learn: synapse is NULL");
        return 0.0F;
    }
    if (!trace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_update_and_learn: trace is NULL");
        return 0.0F;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_update_and_learn: config is NULL");
        return 0.0F;
    }

    // STEP 1: Decay trace to current time
    // WHAT: Update trace for elapsed time
    // WHY: Ensure trace reflects current eligibility
    // HOW: Call eligibility_trace_decay()
    eligibility_trace_decay(trace, config, current_time);

    // STEP 2: Apply reward-based learning
    // WHAT: Update weight based on trace and reward
    // WHY: Implement three-factor learning rule
    // HOW: Call eligibility_apply_reward()
    float delta_w = eligibility_apply_reward(
        synapse, trace, config, reward, dopamine_level
    );

    return delta_w;
}

//=============================================================================
// Burst-Triggered Consolidation (Option 2.2)
//=============================================================================

/**
 * @brief Check if currently in dopamine burst state
 *
 * WHAT: Detect if dopamine is in phasic burst mode
 * WHY: Determine whether to consolidate traces
 * HOW: Check phasic_tonic->in_burst_state or dopamine concentration threshold
 *
 * @param phasic_tonic Dopamine phasic-tonic state
 * @param config Configuration (for min_burst_concentration threshold)
 * @return true if in burst state
 */
bool eligibility_is_in_burst(
    const phasic_tonic_state_t* phasic_tonic,
    const eligibility_config_t* config
) {
    // Guard: NULL checks
    if (!phasic_tonic || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "eligibility_is_in_burst: invalid parameters");

            return false;
    }

    // DETECTION STRATEGY 1: Direct burst state flag (preferred)
    // WHAT: Check if system marked as in burst
    // WHY: Most accurate, directly from phasic-tonic system
    // HOW: Read in_burst_state field
    if (phasic_tonic->in_burst_state) {
        return true;
    }

    // DETECTION STRATEGY 2: Threshold on total concentration (fallback)
    // WHAT: High dopamine concentration implies burst
    // WHY: Fallback if burst state not explicitly tracked
    // HOW: Compare total_concentration to threshold
    // NOTE: Baseline ~0.05 normalized, burst ~0.8+, threshold 0.3 = 6x baseline
    if (phasic_tonic_get_total_concentration(phasic_tonic) >= config->min_burst_concentration) {
        return true;
    }

    // Neither condition met - not in burst
    return false;
}

/**
 * @brief Consolidate eligibility traces to weight changes during dopamine burst
 *
 * WHAT: Convert accumulated traces to weight changes only during bursts
 * WHY: Implements "tags and capture" mechanism (Frey & Morris 1997)
 * HOW: Check burst state, apply amplified learning if in burst
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
) {
    // Guard: NULL checks
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_consolidate_on_burst: synapse is NULL");
        return 0.0F;
    }
    if (!trace) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_consolidate_on_burst: trace is NULL");
        return 0.0F;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "eligibility_consolidate_on_burst: config is NULL");
        return 0.0F;
    }

    // OPTIMIZATION: Skip negligible traces (use absolute value to catch negative traces)
    if (fabsf(trace->trace) < config->trace_threshold) {
        return 0.0F;
    }

    // MODE CHECK: Are we in burst-triggered mode?
    if (config->burst_triggered_mode) {
        // BURST-TRIGGERED MODE (Option 2.2)
        // WHAT: Only consolidate during dopamine bursts
        // WHY: Implement synaptic tagging and capture mechanism
        // HOW: Check burst state before allowing consolidation

        if (!phasic_tonic || !eligibility_is_in_burst(phasic_tonic, config)) {
            // NOT in burst - traces remain as "tags", no consolidation
            return 0.0F;
        }

        // IN BURST - consolidate with amplified learning rate
        // WHAT: Apply burst_lr_multiplier to learning rate
        // WHY: Bursts provide protein synthesis signal for LTP
        // HOW: learning_rate × burst_lr_multiplier × trace × reward × dopamine
        float burst_lr = config->learning_rate * config->burst_lr_multiplier;
        float dopamine_concentration = phasic_tonic_get_total_concentration(phasic_tonic);

        float delta_w = burst_lr * trace->trace * reward;

        // Apply neuromodulation gating (dopamine concentration)
        if (config->use_neuromodulation) {
            delta_w *= dopamine_concentration;
        }

        // Update synapse weight (P1 fix: validate before update)
        if (isnan(delta_w) || isinf(delta_w)) {
            delta_w = 0.0f;
        }
        synapse->weight += delta_w;

        // BUG-16 fix: Use same [-10, 10] bounds as standard mode (caller uses these
        // bounds in synapse_learn_three_factor). Previous [0, 1] was inconsistent
        // and prevented inhibitory weights in burst-triggered mode.
        float new_weight = fminf(fmaxf(synapse->weight, -10.0f), 10.0f);
        synapse->weight = isnan(new_weight) ? 0.5f : new_weight;

        return delta_w;

    } else {
        // STANDARD MODE (Original behavior)
        // WHAT: Consolidate anytime, using dopamine concentration for gating
        // WHY: Backward compatibility with existing code
        // HOW: Same as eligibility_apply_reward()

        float dopamine_concentration = phasic_tonic ?
            phasic_tonic_get_total_concentration(phasic_tonic) : 1.0F;  // Default to 1.0 if no phasic-tonic

        float delta_w = config->learning_rate * trace->trace * reward;

        if (config->use_neuromodulation) {
            delta_w *= dopamine_concentration;
        }

        // P1 fix: validate before update
        if (isnan(delta_w) || isinf(delta_w)) {
            delta_w = 0.0f;
        }
        synapse->weight += delta_w;

        // NOTE: Weight clamping is handled by the caller (synapse_learn_three_factor)
        // which uses [-10, 10] bounds. We only validate for NaN/Inf here.
        if (isnan(synapse->weight) || isinf(synapse->weight)) {
            synapse->weight = 0.5f;  /* Reset on numerical error */
        }

        return delta_w;
    }
}

/**
 * @brief Batch consolidate multiple synapses during dopamine burst
 *
 * WHAT: Apply burst-triggered consolidation to array of synapses
 * WHY: Efficient bulk processing during burst events
 * HOW: Single burst check, then iterate synapses
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
) {
    // Guard: NULL checks and bounds
    if (!synapses || !traces || !config || num_synapses <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "eligibility_consolidate_batch: invalid parameters");

            return 0;
    }

    // OPTIMIZATION: Check burst state once (not per synapse)
    // WHAT: Single burst check for entire batch
    // WHY: Avoid redundant burst checks for each synapse
    // HOW: Call eligibility_is_in_burst() once
    bool in_burst = (phasic_tonic != NULL) &&
                    eligibility_is_in_burst(phasic_tonic, config);

    // Early exit if burst-triggered mode but not in burst
    if (config->burst_triggered_mode && !in_burst) {
        return 0;  // No consolidation outside bursts
    }

    // BATCH PROCESSING: Apply consolidation to all synapses
    int num_consolidated = 0;

    // Phase 8: Heartbeat at start of batch processing
    eligibility_trace_heartbeat("batch_consolidation", 0.0f);

    for (int i = 0; i < num_synapses; i++) {
        // Call consolidate_on_burst for each synapse
        float delta_w = eligibility_consolidate_on_burst(
            &synapses[i],
            &traces[i],
            config,
            phasic_tonic,
            reward
        );

        // Count synapses with significant weight changes
        // Use same threshold as config->trace_threshold for consistency
        if (fabsf(delta_w) > 0.0F) {  // Any non-zero weight change
            num_consolidated++;
        }

        // Phase 8: Periodic heartbeat for large batches
        if ((i & 0xFF) == 0 && num_synapses > 256) {
            eligibility_trace_heartbeat("batch_consolidation",
                                        (float)(i + 1) / (float)num_synapses);
        }
    }

    return num_consolidated;
}

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
 * @return Trace value [0, 1], or 0.0 if trace is NULL
 */
float eligibility_get_trace(const eligibility_trace_t* trace) {
    return trace ? trace->trace : 0.0F;
}

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
) {
    // Guard: NULL checks
    if (!trace || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,

                "eligibility_is_significant: invalid parameters");

            return false;
    }

    // WHAT: Compare absolute trace value to threshold
    // WHY: Determine if trace is worth processing (traces can be negative for LTD)
    // HOW: Use fabsf to catch significant negative traces too
    return fabsf(trace->trace) > config->trace_threshold;
}
