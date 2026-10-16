/**
 * @file nimcp_bcm.c
 * @brief Implementation of BCM learning rule
 *
 * ARCHITECTURAL OVERVIEW:
 * - Strategy Pattern: Configurable threshold update strategies
 * - Factory Pattern: Preset parameter configurations
 * - Value Object: Immutable parameter structs
 *
 * PERFORMANCE OPTIMIZATIONS:
 * - Branchless computation in hot paths
 * - SIMD-friendly memory layout
 * - Inlined fast-path functions
 * - Cache-coherent data structures
 *
 * COMPLEXITY ANALYSIS:
 * - bcm_apply_rule: O(1) - constant time per synapse
 * - bcm_update_threshold: O(1) - single exponential update
 * - bcm_compute_stats: O(n) - linear scan over synapses
 *
 * DESIGN PRINCIPLES:
 * - Guard clauses for early returns (no nested ifs)
 * - Functions < 50 lines
 * - Explicit what/why comments
 * - TDD verified
 */

#include "plasticity/bcm/nimcp_bcm.h"
#include "plasticity/nimcp_plasticity_constants.h"
#include "plasticity/bcm/nimcp_bcm_sleep_bridge.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "security/nimcp_security.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>

#define LOG_MODULE "plasticity_bcm"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(bcm)

//=============================================================================
// Constants
//=============================================================================

#define BCM_EPSILON NIMCP_EPSILON_ADAM          // Prevent division by zero
#define BCM_WEIGHT_MIN 0.0f        // Minimum synaptic weight
#define BCM_WEIGHT_MAX 1.0f        // Maximum synaptic weight

//=============================================================================
// Inline Helper Functions
//=============================================================================

/**
 * @brief Compute BCM plasticity factor
 *
 * WHAT: φ(post, θ) = post × (post - θ)
 * WHY:  Core of BCM rule - supralinear in post-synaptic activity
 *
 * BIOLOGICAL:
 * - post > θ: φ > 0 → LTP
 * - post < θ: φ < 0 → LTD
 * - post = θ: φ = 0 → No change
 *
 * COMPLEXITY: O(1)
 * PERFORMANCE: ~3 CPU cycles
 */
static inline float bcm_plasticity_factor(float post_activity, float threshold) {
    /* WHAT: Compute deviation from threshold
     * WHY:  Determines direction of plasticity
     */
    float deviation = post_activity - threshold;

    /* WHAT: Multiply by post-synaptic activity
     * WHY:  Creates supralinear dependence (quadratic in post)
     */
    return post_activity * deviation;
}

//=============================================================================
// Core BCM Functions
//=============================================================================

bcm_synapse_t bcm_synapse_init(float initial_weight, float initial_threshold) {
    /* WHAT: Initialize BCM synapse with factory method (thread-safe version)
     * WHY:  Encapsulates initialization, ensures valid state with synchronization
     * COMPLEXITY: O(1)
     * THREAD SAFETY: Initializes spinlock for concurrent access protection
     */

    /* WHAT: Create synapse with zeroed memory
     * WHY:  Ensures all fields have defined values
     */
    bcm_synapse_t synapse = {0};

    /* WHAT: Clamp initial weight to valid range
     * WHY:  Prevent invalid initial states
     */
    synapse.weight = nimcp_clampf(initial_weight, BCM_WEIGHT_MIN, BCM_WEIGHT_MAX);

    /* WHAT: Set initial threshold
     * WHY:  Starting point for sliding threshold dynamics
     * NOTE: Threshold will adapt based on activity
     */
    synapse.threshold = initial_threshold;

    /* WHAT: Initialize average activity to zero
     * WHY:  Will be computed from actual activity
     */
    synapse.avg_post_activity = 0.0F;

    /* WHAT: Initialize eligibility to zero
     * WHY:  Used for delayed reward assignment
     */
    synapse.eligibility = 0.0F;

    /* WHAT: Initialize sleep state to awake
     * WHY:  Default to normal (awake) learning dynamics
     */
    synapse.current_sleep_state = SLEEP_STATE_AWAKE;

    /* WHAT: Initialize spinlock for thread-safe updates
     * WHY:  Allow concurrent access when synapse is shared across threads
     * WHEN NEEDED: Only if multiple threads update same synapse (rare)
     * OVERHEAD: Minimal (~4 bytes), no performance cost if not shared
     */
    nimcp_spinlock_init(&synapse.lock);

    return synapse;
}

void bcm_update_threshold(bcm_synapse_t* synapse, float post_activity, float dt,
                         const bcm_params_t* params) {
    /* WHAT: Update sliding modification threshold (thread-safe)
     * WHY:  BCM threshold adapts to average post-synaptic activity²
     *
     * BIOLOGICAL: θ̇ = (post² - θ) / τ
     * - Threshold tracks E[post²]
     * - Self-stabilizing without explicit normalization
     *
     * COMPLEXITY: O(1)
     * THREAD SAFETY: Acquires spinlock (brief critical section ~30-50ns)
     */

    /* WHAT: Guard clause - validate inputs
     * WHY:  Prevent null pointer dereference
     */
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_update_threshold: synapse is NULL");
        return;
    }
    if (!params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_update_threshold: params is NULL");
        return;
    }

    /* WHAT: Acquire spinlock for atomic threshold update
     * WHY:  Prevent race conditions when multiple threads access same synapse
     * PERFORMANCE: ~10-20ns overhead (acceptable for per-synapse lock)
     * NOTE: Only contends if threads share synapses (rare in typical use)
     */
    nimcp_spinlock_lock(&synapse->lock);

    /* WHAT: Get sleep modulation factor for threshold
     * WHY:  Sleep state modulates threshold (e.g., NREM raises it for LTD bias)
     */
    float theta_factor = bcm_sleep_theta_for_state(synapse->current_sleep_state);

    /* WHAT: Compute target threshold (post-synaptic activity squared)
     * WHY:  Biological BCM uses quadratic dependence on activity
     * RATIONALE: E[post²] sets crossover point between LTP and LTD
     */
    float target_threshold = post_activity * post_activity * theta_factor;

    /* WHAT: Compute time constant for exponential decay
     * WHY:  Convert dt from ms to decay fraction
     * FORMULA: decay = 1 - exp(-dt/τ)
     * P1 fix: Guard against denormal floats from large dt/tau ratios
     *         exp(-x) becomes denormal for x > ~88, negligible for x > ~20
     */
    float tau = params->threshold_time_constant;
    /* Validate tau to prevent division issues */
    if (isnan(tau) || tau <= BCM_EPSILON) {
        tau = 1000.0F;  /* Default 1 second time constant */
    }
    float exp_arg = -dt / tau;
    /* Clamp to prevent denormal floats (exp(-20) ≈ 2e-9, sufficient precision) */
    if (exp_arg < -20.0F) exp_arg = -20.0F;
    float exp_result = expf(exp_arg);
    /* Validate exponential result and flush denormals */
    if (isnan(exp_result) || exp_result < NIMCP_DENORMAL_EXP_THRESHOLD) {
        exp_result = 0.0F;
    }
    float decay = 1.0F - exp_result;

    /* WHAT: Exponential moving average toward target
     * WHY:  Smooth threshold adaptation, prevents oscillations
     * FORMULA: θ_new = θ_old + decay × (target - θ_old)
     */
    synapse->threshold += decay * (target_threshold - synapse->threshold);

    /* WHAT: Clamp threshold to physiological range
     * WHY:  Prevent numerical instabilities and unrealistic values
     */
    synapse->threshold = nimcp_clampf(synapse->threshold, params->min_threshold,
                                 params->max_threshold);

    /* WHAT: Update running average of post-synaptic activity
     * WHY:  Used for statistics and monitoring
     * P1 fix: Same denormal protection as threshold decay
     */
    float activity_tau = params->activity_time_constant;
    if (isnan(activity_tau) || activity_tau <= BCM_EPSILON) {
        activity_tau = 100.0F;  /* Default 100ms time constant */
    }
    float activity_exp_arg = -dt / activity_tau;
    if (activity_exp_arg < -20.0F) activity_exp_arg = -20.0F;
    float activity_exp_result = expf(activity_exp_arg);
    if (isnan(activity_exp_result) || activity_exp_result < NIMCP_DENORMAL_EXP_THRESHOLD) {
        activity_exp_result = 0.0F;
    }
    float activity_decay = 1.0F - activity_exp_result;
    synapse->avg_post_activity += activity_decay * (post_activity - synapse->avg_post_activity);

    /* WHAT: Release spinlock
     * WHY:  Allow other threads to access synapse
     * PERFORMANCE: ~10-20ns
     */
    nimcp_spinlock_unlock(&synapse->lock);
}

void bcm_apply_rule(bcm_synapse_t* synapse, float pre_activity, float post_activity,
                   float dt, const bcm_params_t* params) {
    /* WHAT: Apply BCM plasticity rule to update synaptic weight (thread-safe)
     * WHY:  Implement sliding threshold LTP/LTD with concurrent access protection
     *
     * BIOLOGICAL: Δw = η × post × (post - θ) × pre
     * - Supralinear in post-synaptic activity
     * - Hebbian-like (requires pre and post co-activity)
     * - Self-stabilizing via sliding threshold
     *
     * COMPLEXITY: O(1)
     * PERFORMANCE: ~10 CPU cycles (branchless hot path) + spinlock overhead
     * THREAD SAFETY: Acquires spinlock (brief critical section ~50-80ns)
     */

    /* WHAT: Guard clause - validate inputs
     * WHY:  Prevent null pointer dereference
     */
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_apply_rule: synapse is NULL");
        return;
    }
    if (!params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_apply_rule: params is NULL");
        return;
    }

    /* WHAT: Acquire spinlock for atomic weight update
     * WHY:  Prevent race conditions when multiple threads access same synapse
     * PERFORMANCE: ~10-20ns overhead (acceptable for per-synapse lock)
     * NOTE: Only contends if threads share synapses (rare in typical use)
     */
    nimcp_spinlock_lock(&synapse->lock);

    /* WHAT: Get sleep modulation factor for learning rate
     * WHY:  Sleep state modulates learning rate (e.g., reduced during sleep)
     */
    float lr_factor = bcm_sleep_lr_for_state(synapse->current_sleep_state);

    /* WHAT: Compute BCM plasticity factor φ(post, θ)
     * WHY:  Determines direction and magnitude of weight change
     * FORMULA: φ = post × (post - θ)
     */
    float plasticity_factor = bcm_plasticity_factor(post_activity, synapse->threshold);

    /* WHAT: Compute weight change with sleep modulation
     * WHY:  BCM rule with Hebbian pre-synaptic gating and sleep modulation
     * FORMULA: Δw = η × lr_factor × φ(post, θ) × pre × dt
     * NOTE: dt scaling makes rule time-step independent
     */
    float delta_w = params->learning_rate * lr_factor * plasticity_factor * pre_activity * dt;

    /* WHAT: Apply weight update
     * WHY:  Integrate weight change into current weight
     */
    float new_weight = synapse->weight + delta_w;

    /* P0 fix: Validate numerical stability before weight update
     * WHY:  NaN/Inf can propagate from inputs, causing weight corruption
     * HOW:  Skip update if delta_w or new_weight is invalid
     */
    if (isnan(new_weight) || isinf(new_weight)) {
        nimcp_spinlock_unlock(&synapse->lock);
        return;  /* Skip this update to protect weight integrity */
    }

    /* WHAT: Clamp weight to valid physiological range
     * WHY:  Weights must stay in [0, 1] for biological realism
     * NOTE: Branchless clamp for performance
     */
    synapse->weight = nimcp_clampf(new_weight, BCM_WEIGHT_MIN, BCM_WEIGHT_MAX);

    /* WHAT: Update eligibility trace for delayed reward
     * WHY:  Allows credit assignment when reward is delayed
     * FORMULA: e_new = e_old × decay + Δw
     */
    float eligibility_decay = 0.95F;  // Fast decay (~20ms time constant)
    synapse->eligibility = synapse->eligibility * eligibility_decay + delta_w;

    /* WHAT: Clamp eligibility to reasonable bounds [-2, 2]
     * WHY:  Prevent unbounded accumulation of eligibility
     * HOW:  Use clamp_f helper function
     * BIOLOGICAL: Eligibility traces represent recent plasticity, should be bounded
     */
    synapse->eligibility = nimcp_clampf(synapse->eligibility, -2.0F, 2.0F);

    /* WHAT: Release spinlock
     * WHY:  Allow other threads to access synapse
     * PERFORMANCE: ~10-20ns
     */
    nimcp_spinlock_unlock(&synapse->lock);
}

void bcm_apply_rule_modulated(bcm_synapse_t* synapse, float pre_activity,
                             float post_activity, float dt, const bcm_params_t* params,
                             float neuromodulator_level) {
    /* WHAT: Apply BCM rule with neuromodulator gating
     * WHY:  Reward signals should gate cortical plasticity
     *
     * BIOLOGICAL:
     * - Dopamine gates LTP in cortex and striatum
     * - No dopamine → minimal plasticity
     * - High dopamine → enhanced plasticity
     *
     * COMPLEXITY: O(1)
     */

    /* WHAT: Guard clause - validate inputs
     * WHY:  Prevent null pointer dereference
     */
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_apply_rule_modulated: synapse is NULL");
        return;
    }
    if (!params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_apply_rule_modulated: params is NULL");
        return;
    }

    /* WHAT: Clamp neuromodulator to valid range
     * WHY:  Ensure modulation factor is in [0, 1]
     */
    float modulation = nimcp_clampf(neuromodulator_level, 0.0F, 1.0F);

    /* WHAT: Acquire spinlock for atomic weight update
     * WHY:  Prevent race conditions when multiple threads access same synapse
     * PERFORMANCE: ~10-20ns overhead (acceptable for per-synapse lock)
     * NOTE: Only contends if threads share synapses (rare in typical use)
     */
    nimcp_spinlock_lock(&synapse->lock);

    /* WHAT: Get sleep modulation factor for learning rate
     * WHY:  Sleep state modulates learning rate (e.g., reduced during sleep)
     */
    float lr_factor = bcm_sleep_lr_for_state(synapse->current_sleep_state);

    /* WHAT: Compute BCM plasticity factor
     * WHY:  Same as standard BCM rule
     */
    float plasticity_factor = bcm_plasticity_factor(post_activity, synapse->threshold);

    /* WHAT: Compute modulated weight change with sleep modulation
     * WHY:  Neuromodulator and sleep both scale learning rate
     * FORMULA: Δw = η × lr_factor × modulation × φ(post, θ) × pre × dt
     * BIOLOGICAL: DA concentration and sleep state both scale plasticity magnitude
     */
    float delta_w = params->learning_rate * lr_factor * modulation * plasticity_factor * pre_activity * dt;

    /* WHAT: Apply weight update with clamping
     * WHY:  Same as standard BCM, but modulated magnitude
     */
    float new_weight = synapse->weight + delta_w;

    /* P0 fix: Validate numerical stability before weight update
     * WHY:  NaN/Inf can propagate from inputs, causing weight corruption
     * HOW:  Skip update if new_weight is invalid
     */
    if (isnan(new_weight) || isinf(new_weight)) {
        nimcp_spinlock_unlock(&synapse->lock);
        return;  /* Skip this update to protect weight integrity */
    }

    synapse->weight = nimcp_clampf(new_weight, BCM_WEIGHT_MIN, BCM_WEIGHT_MAX);

    /* WHAT: Update eligibility trace
     * WHY:  Track recent plasticity for delayed reward
     */
    float eligibility_decay = 0.95F;
    synapse->eligibility = synapse->eligibility * eligibility_decay + delta_w;

    /* WHAT: Clamp eligibility to reasonable bounds [-2, 2]
     * WHY:  Prevent unbounded accumulation of eligibility
     * HOW:  Use clamp_f helper function
     * BIOLOGICAL: Eligibility traces represent recent plasticity, should be bounded
     */
    synapse->eligibility = nimcp_clampf(synapse->eligibility, -2.0F, 2.0F);

    /* WHAT: Release spinlock
     * WHY:  Allow other threads to access synapse
     * PERFORMANCE: ~10-20ns
     */
    nimcp_spinlock_unlock(&synapse->lock);
}

//=============================================================================
// Factory Methods for Parameter Presets
//=============================================================================

bcm_params_t bcm_params_cortical(void) {
    /* WHAT: Factory method for cortical BCM parameters
     * WHY:  Provides biologically plausible defaults
     *
     * BIOLOGICAL: Based on visual cortex measurements
     * - Learning rate: moderate (0.01)
     * - Threshold τ: slow adaptation (~1 second)
     * - Activity τ: moderate averaging (~100 ms)
     *
     * COMPLEXITY: O(1)
     */

    bcm_params_t params;

    /* WHAT: Set learning rate for adult cortex
     * WHY:  Adult cortex has moderate plasticity
     */
    params.learning_rate = 0.01F;

    /* WHAT: Set threshold adaptation time constant
     * WHY:  Threshold should adapt slowly relative to activity
     * BIOLOGICAL: ~1 second timescale in cortex
     */
    params.threshold_time_constant = 1000.0F;  // ms

    /* WHAT: Set activity averaging time constant
     * WHY:  Average over multiple spikes for stability
     * BIOLOGICAL: ~100 ms integration window
     */
    params.activity_time_constant = 100.0F;  // ms

    /* WHAT: Set minimum threshold
     * WHY:  Prevent over-depression (all weights → 0)
     */
    params.min_threshold = 0.1F;

    /* WHAT: Set maximum threshold
     * WHY:  Prevent runaway potentiation
     */
    params.max_threshold = 10.0F;

    /* WHAT: Enable quantum threshold optimization by default
     * WHY:  Quantum annealing finds better thresholds globally
     */
    params.enable_quantum_bcm = true;

    return params;
}

bcm_params_t bcm_params_critical_period(void) {
    /* WHAT: Factory method for critical period learning
     * WHY:  Developmental plasticity is higher than adult
     *
     * BIOLOGICAL: Models early postnatal development
     * - High learning rate (10x adult)
     * - Faster threshold adaptation
     * - Wider threshold range
     *
     * COMPLEXITY: O(1)
     */

    bcm_params_t params;

    /* WHAT: High learning rate for critical period
     * WHY:  Rapid learning during development
     * BIOLOGICAL: Critical periods have elevated plasticity
     */
    params.learning_rate = 0.1F;  // 10x adult

    /* WHAT: Faster threshold adaptation
     * WHY:  Development requires rapid tuning
     */
    params.threshold_time_constant = 500.0F;  // ms (2x faster)

    /* WHAT: Faster activity averaging
     * WHY:  Developmental plasticity responds to shorter timescales
     */
    params.activity_time_constant = 50.0F;  // ms

    /* WHAT: Lower minimum threshold
     * WHY:  Allow stronger depression during refinement
     */
    params.min_threshold = 0.05F;

    /* WHAT: Higher maximum threshold
     * WHY:  Allow stronger potentiation during development
     */
    params.max_threshold = 20.0F;

    /* WHAT: Enable quantum threshold optimization by default
     * WHY:  Quantum annealing finds better thresholds globally
     */
    params.enable_quantum_bcm = true;

    return params;
}

bcm_params_t bcm_params_mature(void) {
    /* WHAT: Factory method for mature/adult learning
     * WHY:  Adult cortex has lower plasticity than critical period
     *
     * BIOLOGICAL: Models adult cortical plasticity
     * - Low learning rate (stable)
     * - Slow threshold adaptation
     * - Narrow threshold range
     *
     * COMPLEXITY: O(1)
     */

    bcm_params_t params;

    /* WHAT: Low learning rate for mature brain
     * WHY:  Stability over plasticity in adults
     * BIOLOGICAL: Adult cortex resists change
     */
    params.learning_rate = 0.001F;  // 10x lower than developmental

    /* WHAT: Slow threshold adaptation
     * WHY:  Mature brain has stable activity statistics
     */
    params.threshold_time_constant = 2000.0F;  // ms (very slow)

    /* WHAT: Slow activity averaging
     * WHY:  Integrate over longer timescales for stability
     */
    params.activity_time_constant = 200.0F;  // ms

    /* WHAT: Narrow threshold range
     * WHY:  Prevent large weight fluctuations in mature brain
     */
    params.min_threshold = 0.2F;
    params.max_threshold = 5.0F;

    /* WHAT: Enable quantum threshold optimization by default
     * WHY:  Quantum annealing finds better thresholds globally
     */
    params.enable_quantum_bcm = true;

    return params;
}

//=============================================================================
// Statistics and Analysis
//=============================================================================

bool bcm_compute_stats(const bcm_synapse_t* synapses, uint32_t num_synapses,
                      bcm_stats_t* stats) {
    /* WHAT: Compute aggregate statistics over synapse population
     * WHY:  Monitor learning progress, detect instabilities
     *
     * COMPLEXITY: O(n) where n = num_synapses
     * PERFORMANCE: ~2n cache misses (linear scan)
     */

    /* WHAT: Guard clause - validate inputs
     * WHY:  Prevent null pointer dereference
     */
    if (!synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_compute_stats: synapses is NULL");
        return false;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_compute_stats: stats is NULL");
        return false;
    }
    if (num_synapses == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bcm_compute_stats: num_synapses is 0");
        return false;
    }

    /* WHAT: Zero out statistics structure
     * WHY:  Clean slate for accumulation
     */
    memset(stats, 0, sizeof(bcm_stats_t));

    /* Phase 8: Heartbeat at start of stats computation */
    bcm_heartbeat("bcm_compute_stats", 0.0f);

    /* WHAT: Accumulate sums for mean computation
     * WHY:  Single pass for efficiency
     * P2 fix: Use double for accumulators to prevent precision loss
     *         with large synapse counts (float has 24-bit mantissa,
     *         double has 53-bit mantissa - handles ~10^15 synapses accurately)
     */
    double sum_weight = 0.0;
    double sum_threshold = 0.0;
    double sum_weight_squared = 0.0;

    for (uint32_t i = 0; i < num_synapses; i++) {
        const bcm_synapse_t* syn = &synapses[i];

        sum_weight += (double)syn->weight;
        sum_threshold += (double)syn->threshold;
        sum_weight_squared += (double)(syn->weight * syn->weight);

        /* WHAT: Count LTP vs LTD events (approximate)
         * WHY:  Balance indicates learning dynamics
         * NOTE: Eligibility > 0 suggests recent LTP, < 0 suggests LTD
         */
        if (syn->eligibility > 0.0F) {
            stats->ltp_events++;
        } else if (syn->eligibility < 0.0F) {
            stats->ltd_events++;
        }

        /* Phase 8: Periodic heartbeat for large synapse counts */
        if ((i & 0xFF) == 0 && num_synapses > 256) {
            bcm_heartbeat("bcm_compute_stats", (float)(i + 1) / (float)num_synapses);
        }
    }

    /* WHAT: Compute averages
     * WHY:  Mean weight and threshold indicate population state
     * P2 fix: Cast back to float after division for storage
     */
    stats->avg_weight = (float)(sum_weight / (double)num_synapses);
    stats->avg_threshold = (float)(sum_threshold / (double)num_synapses);

    /* WHAT: Compute weight variance
     * WHY:  Variance indicates weight distribution spread
     * FORMULA: Var(w) = E[w²] - E[w]²
     * P2 fix: Use double for intermediate calculation to preserve precision
     */
    double mean_squared = (double)stats->avg_weight * (double)stats->avg_weight;
    double mean_of_squares = sum_weight_squared / (double)num_synapses;
    stats->weight_variance = (float)(mean_of_squares - mean_squared);

    /* WHAT: Record total update count (per synapse basis)
     * WHY:  Indicates total plasticity activity
     */
    stats->total_updates = num_synapses;

    return true;
}

//=============================================================================
// Sleep Integration
//=============================================================================

void bcm_set_sleep_state(bcm_synapse_t* synapse, sleep_state_t state) {
    /* WHAT: Update sleep state for synapse
     * WHY:  Sleep state modulates threshold and learning rate
     * HOW:  Set current_sleep_state field, will be applied during next update
     */
    if (!synapse) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_set_sleep_state: synapse is NULL");
        return;
    }
    synapse->current_sleep_state = state;
}

//=============================================================================
// Quantum Bridge Integration
//=============================================================================

#define NIMCP_BCM_QUANTUM_BRIDGE_IMPLEMENTATION
#include "plasticity/bcm/nimcp_bcm_quantum_bridge.h"

void* bcm_extract_quantum_stats(const bcm_synapse_t* synapses, uint32_t num_synapses,
                                float avg_post_activity) {
    /* WHAT: Extract BCM activity statistics for quantum optimization
     * WHY:  Provide input to quantum threshold optimization
     * HOW:  Compute aggregated metrics from synapse population
     *
     * COMPLEXITY: O(n) where n = num_synapses
     */

    /* WHAT: Guard clause - validate inputs */
    if (!synapses) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_extract_quantum_stats: synapses is NULL");
        return NULL;
    }
    if (num_synapses == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "bcm_extract_quantum_stats: num_synapses is 0");
        return NULL;
    }

    /* WHAT: Allocate statistics structure */
    bcm_activity_stats_t* stats = (bcm_activity_stats_t*)nimcp_malloc(sizeof(bcm_activity_stats_t));
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bcm_extract_quantum_stats: failed to allocate bcm_activity_stats_t");
        return NULL;
    }

    /* WHAT: Compute BCM statistics */
    bcm_stats_t bcm_stats;
    if (!bcm_compute_stats(synapses, num_synapses, &bcm_stats)) {
        nimcp_free(stats);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_set_sleep_state: bcm_compute_stats is NULL");
        return NULL;
    }

    /* WHAT: Populate activity statistics */
    stats->avg_weight = bcm_stats.avg_weight;
    stats->weight_variance = bcm_stats.weight_variance;
    stats->avg_post_activity = avg_post_activity;

    /* WHAT: Compute selectivity index
     * WHY:  Measure of feature selectivity (winner-take-all)
     * HOW:  Ratio of active to total synapses weighted by variance
     */
    uint32_t num_active = 0;
    const float active_threshold = 0.1F;  /* Weights > 0.1 considered active */
    for (uint32_t i = 0; i < num_synapses; i++) {
        if (synapses[i].weight > active_threshold) {
            num_active++;
        }
    }
    stats->num_active_synapses = num_active;

    /* WHAT: Compute selectivity index
     * WHY:  High variance + low sparsity = selective
     * FORMULA: selectivity = variance × (1 - sparsity)
     */
    float sparsity = (float)num_active / (float)num_synapses;
    stats->selectivity_index = bcm_stats.weight_variance * (1.0F - sparsity);

    return (void*)stats;
}

//=============================================================================
// KG Reader Self-Awareness Integration
//=============================================================================

/**
 * @brief Query self-knowledge from knowledge graph
 *
 * WHAT: Allow BCM module to introspect its own capabilities and connections
 * WHY:  Self-awareness enables adaptive behavior and system introspection
 * HOW:  Query KG for BCM_Module entity and its relations
 *
 * COMPLEXITY: O(n) where n = number of observations/relations
 *
 * @param kg Knowledge graph reader handle
 * @return 1 if self-knowledge found, 0 if not found or error
 */
int bcm_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bcm_query_self_knowledge: kg is NULL");
        return 0;
    }

    /* Query our own entity from the knowledge graph */
    const kg_entity_t* self = kg_reader_get_entity(kg, "BCM_Module");
    if (self) {
        /* Module now knows its own capabilities from KG */
        for (uint32_t i = 0; i < self->num_observations; i++) {
            LOG_DEBUG("BCM self-knowledge: %s", self->observations[i]);
        }
    }

    /* Query connections to understand integration points */
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "BCM_Module");
    if (connections) {
        LOG_DEBUG("BCM has %u outgoing connections", connections->count);
        kg_relation_list_destroy(connections);
    }

    /* Query incoming connections */
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "BCM_Module");
    if (incoming) {
        LOG_DEBUG("BCM has %u incoming connections", incoming->count);
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}
