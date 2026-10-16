/**
 * @file nimcp_lgss_stdp_guard.h
 * @brief Learning Guarded Safety System (LGSS) - STDP Guard
 * @version 1.0.0
 * @date 2026-01-16
 *
 * WHAT: Spike-Timing-Dependent Plasticity (STDP) specific guard
 * WHY: Ensure STDP learning remains within safe bounds and prevent
 *      exploitation of spike timing to induce unsafe weight changes
 * HOW: Validate spike pairs, enforce LTP/LTD limits, monitor spike rates
 *
 * STDP BACKGROUND:
 * STDP is a biological learning rule where synaptic strength changes based
 * on the relative timing of pre- and post-synaptic spikes:
 * - Pre before Post (positive dt): Long-Term Potentiation (LTP) - strengthen
 * - Post before Pre (negative dt): Long-Term Depression (LTD) - weaken
 *
 * SECURITY CONCERNS:
 * - Rapid spike trains could cause excessive weight changes
 * - Carefully timed spikes could target specific synapses
 * - Asymmetric STDP windows could be exploited
 * - Spike timing patterns could be injected to manipulate learning
 *
 * USAGE:
 * @code
 * stdp_guard_config_t config = stdp_guard_default_config();
 * stdp_guard_t* guard = stdp_guard_create(&config, base_plasticity_guard);
 *
 * // Process each spike pair through guard
 * stdp_spike_pair_t pair = {
 *     .synapse_id = synapse_id,
 *     .dt_ms = pre_time - post_time,
 *     .current_weight = weight
 * };
 *
 * stdp_update_result_t result;
 * if (stdp_guard_process_spike_pair(guard, &pair, &result) == 0) {
 *     // Apply result.adjusted_delta to weight
 * }
 * @endcode
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_LGSS_STDP_GUARD_H
#define NIMCP_LGSS_STDP_GUARD_H

#include "nimcp_lgss_plasticity_constraints.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS AND LIMITS
 * ============================================================================ */

/** Default STDP time window (milliseconds) */
#define STDP_DEFAULT_WINDOW_MS 50.0f

/** Default LTP time constant (tau_plus) */
#define STDP_DEFAULT_TAU_PLUS 20.0f

/** Default LTD time constant (tau_minus) */
#define STDP_DEFAULT_TAU_MINUS 20.0f

/** Default maximum LTP amplitude */
#define STDP_DEFAULT_MAX_LTP 0.01f

/** Default maximum LTD amplitude */
#define STDP_DEFAULT_MAX_LTD 0.01f

/** Default LTP/LTD asymmetry ratio */
#define STDP_DEFAULT_ASYMMETRY_RATIO 1.0f

/** Default max spike rate (Hz) for rate limiting */
#define STDP_DEFAULT_MAX_SPIKE_RATE 200.0f

/** Default spike rate window (seconds) */
#define STDP_DEFAULT_RATE_WINDOW_SEC 0.1f

/** Maximum tracked spike pairs per synapse */
#define STDP_MAX_TRACKED_PAIRS 64

/** Magic number for STDP guard validation */
#define LGSS_STDP_GUARD_MAGIC 0x53544450 /* 'STDP' */

/* ============================================================================
 * VIOLATION TYPES
 * ============================================================================ */

/**
 * WHAT: Types of STDP-specific constraint violations
 * WHY: Categorize STDP violations for logging and response
 * HOW: Bitmask flags for efficient tracking
 */
typedef enum {
    STDP_VIOLATION_NONE                 = 0,
    STDP_VIOLATION_LTP_LIMIT            = (1 << 0), /**< LTP exceeds limit */
    STDP_VIOLATION_LTD_LIMIT            = (1 << 1), /**< LTD exceeds limit */
    STDP_VIOLATION_SPIKE_RATE           = (1 << 2), /**< Spike rate too high */
    STDP_VIOLATION_WINDOW_EXCEEDED      = (1 << 3), /**< Outside STDP window */
    STDP_VIOLATION_ASYMMETRY            = (1 << 4), /**< Asymmetry violation */
    STDP_VIOLATION_CUMULATIVE           = (1 << 5), /**< Cumulative change limit */
    STDP_VIOLATION_SUSPICIOUS_TIMING    = (1 << 6), /**< Suspicious timing pattern */
    STDP_VIOLATION_BURST_DETECTED       = (1 << 7), /**< Excessive burst detected */
    STDP_VIOLATION_BASE_GUARD           = (1 << 8)  /**< Base plasticity guard violation */
} stdp_violation_t;

/* ============================================================================
 * DATA STRUCTURES
 * ============================================================================ */

/**
 * WHAT: A spike pair for STDP processing
 * WHY: Encapsulate spike timing information
 * HOW: Struct with synapse ID, timing, and context
 */
typedef struct {
    uint64_t synapse_id;              /**< Synapse identifier */
    float dt_ms;                      /**< Time delta: pre - post (ms) */
    float current_weight;             /**< Current synaptic weight */
    uint64_t pre_spike_time_us;       /**< Pre-synaptic spike time (us) */
    uint64_t post_spike_time_us;      /**< Post-synaptic spike time (us) */
    uint32_t neuron_pair_id;          /**< Optional: pre-post neuron pair ID */
    float eligibility_trace;          /**< Optional: eligibility trace value */
} stdp_spike_pair_t;

/**
 * WHAT: Result of STDP spike pair processing
 * WHY: Provide detailed information about STDP update
 * HOW: Struct with violation flags, weight delta, and details
 */
typedef struct {
    bool allowed;                     /**< Whether update is allowed */
    stdp_violation_t violations;      /**< STDP-specific violations */
    plasticity_violation_t base_violations; /**< Base guard violations */
    float original_delta;             /**< Delta before guard adjustment */
    float adjusted_delta;             /**< Delta after guard adjustment */
    float ltp_component;              /**< LTP contribution to delta */
    float ltd_component;              /**< LTD contribution to delta */
    bool was_clamped;                 /**< Whether delta was clamped */
    char reason[128];                 /**< Human-readable reason if blocked */
} stdp_update_result_t;

/**
 * WHAT: Configuration for STDP guard
 * WHY: Customize STDP safety bounds
 * HOW: Struct with STDP-specific parameters
 */
typedef struct {
    /* STDP window configuration */
    float stdp_window_ms;             /**< STDP time window (ms) */
    float tau_plus;                   /**< LTP time constant (ms) */
    float tau_minus;                  /**< LTD time constant (ms) */

    /* Amplitude limits */
    float max_ltp_amplitude;          /**< Maximum LTP per spike pair */
    float max_ltd_amplitude;          /**< Maximum LTD per spike pair */
    float asymmetry_ratio_min;        /**< Min allowed LTP/LTD ratio */
    float asymmetry_ratio_max;        /**< Max allowed LTP/LTD ratio */

    /* Cumulative limits */
    float max_cumulative_change;      /**< Max cumulative change per window */
    float cumulative_window_ms;       /**< Window for cumulative tracking */

    /* Spike rate limits */
    float max_pre_spike_rate;         /**< Max pre-synaptic spike rate (Hz) */
    float max_post_spike_rate;        /**< Max post-synaptic spike rate (Hz) */
    float spike_rate_window_sec;      /**< Window for rate calculation */

    /* Burst detection */
    bool enable_burst_detection;      /**< Detect suspicious bursts */
    uint32_t burst_threshold_count;   /**< Spikes to constitute burst */
    float burst_threshold_interval_ms; /**< Max interval for burst detection */

    /* Timing pattern detection */
    bool enable_timing_pattern_detection; /**< Detect suspicious patterns */
    float timing_regularity_threshold;    /**< Threshold for regularity */

    /* Integration with base guard */
    bool use_base_guard;              /**< Use base plasticity guard */
    bool clamp_to_limits;             /**< Clamp violations vs. reject */

    /* Logging */
    bool enable_violation_logging;    /**< Log violations */
    bool enable_statistics;           /**< Track detailed statistics */
} stdp_guard_config_t;

/**
 * WHAT: Statistics about STDP guard operation
 * WHY: Monitor STDP guard effectiveness
 * HOW: Counters and metrics for STDP-specific behavior
 */
typedef struct {
    /* Spike pair statistics */
    uint64_t total_pairs_processed;   /**< Total spike pairs processed */
    uint64_t pairs_allowed;           /**< Pairs that passed guard */
    uint64_t pairs_blocked;           /**< Pairs blocked by guard */
    uint64_t pairs_clamped;           /**< Pairs with clamped delta */

    /* LTP/LTD statistics */
    uint64_t ltp_events;              /**< Total LTP events */
    uint64_t ltd_events;              /**< Total LTD events */
    float total_ltp_magnitude;        /**< Cumulative LTP magnitude */
    float total_ltd_magnitude;        /**< Cumulative LTD magnitude */
    float avg_ltp_magnitude;          /**< Average LTP per event */
    float avg_ltd_magnitude;          /**< Average LTD per event */
    float max_ltp_seen;               /**< Maximum LTP seen */
    float max_ltd_seen;               /**< Maximum LTD seen */

    /* Violation breakdown */
    uint64_t ltp_limit_violations;    /**< LTP limit violations */
    uint64_t ltd_limit_violations;    /**< LTD limit violations */
    uint64_t spike_rate_violations;   /**< Spike rate violations */
    uint64_t window_exceeded_violations; /**< Window exceeded */
    uint64_t asymmetry_violations;    /**< Asymmetry violations */
    uint64_t cumulative_violations;   /**< Cumulative change violations */
    uint64_t timing_pattern_violations; /**< Suspicious timing detections */
    uint64_t burst_detections;        /**< Burst detections */

    /* Timing statistics */
    float avg_dt_ms;                  /**< Average spike timing delta */
    float stddev_dt_ms;               /**< Standard deviation of delta */
    float avg_pre_spike_rate;         /**< Average pre spike rate */
    float avg_post_spike_rate;        /**< Average post spike rate */

    /* Performance */
    uint64_t guard_overhead_us;       /**< Total guard overhead */
    float avg_process_time_us;        /**< Average processing time */
} stdp_guard_stats_t;

/* ============================================================================
 * OPAQUE HANDLE
 * ============================================================================ */

/**
 * WHAT: Opaque handle to STDP guard
 * WHY: Encapsulation - hide internal implementation
 * HOW: Pimpl idiom - pointer to internal structure
 */
typedef struct stdp_guard_internal* stdp_guard_t;

/* ============================================================================
 * DEFAULT CONFIGURATION
 * ============================================================================ */

/**
 * WHAT: Get default STDP guard configuration
 * WHY: Provide sensible defaults for common use cases
 * HOW: Return pre-configured struct
 *
 * DEFAULT VALUES:
 * - stdp_window_ms: 50.0
 * - tau_plus: 20.0
 * - tau_minus: 20.0
 * - max_ltp_amplitude: 0.01
 * - max_ltd_amplitude: 0.01
 * - asymmetry_ratio_min: 0.5
 * - asymmetry_ratio_max: 2.0
 * - max_cumulative_change: 0.1
 * - max_pre_spike_rate: 200.0 Hz
 * - max_post_spike_rate: 200.0 Hz
 * - enable_burst_detection: true
 * - enable_timing_pattern_detection: true
 * - use_base_guard: true
 * - clamp_to_limits: true
 *
 * @return Default configuration
 */
stdp_guard_config_t stdp_guard_default_config(void);

/* ============================================================================
 * LIFECYCLE MANAGEMENT
 * ============================================================================ */

/**
 * WHAT: Create a new STDP guard
 * WHY: Initialize guard for constraining STDP updates
 * HOW: Allocate resources, setup spike rate tracking
 *
 * @param config STDP configuration (NULL for defaults)
 * @param base_guard Base plasticity guard for integration (can be NULL)
 * @return Guard handle, or NULL on error
 *
 * ERRORS:
 * - Returns NULL if memory allocation fails
 * - Returns NULL if configuration is invalid
 *
 * MEMORY: Caller must call stdp_guard_destroy() when done
 */
stdp_guard_t stdp_guard_create(
    const stdp_guard_config_t* config,
    plasticity_guard_t base_guard
);

/**
 * WHAT: Destroy STDP guard
 * WHY: Release all resources
 * HOW: Free internal structures
 *
 * @param guard Guard to destroy (NULL safe)
 *
 * NOTE: Does NOT destroy the base_guard - caller manages that separately
 */
void stdp_guard_destroy(stdp_guard_t guard);

/**
 * WHAT: Reset STDP guard state
 * WHY: Clear accumulated state without recreating
 * HOW: Reset spike rate tracking, cumulative counters
 *
 * @param guard Guard to reset
 * @return 0 on success, error code on failure
 */
int stdp_guard_reset(stdp_guard_t guard);

/* ============================================================================
 * SPIKE PAIR PROCESSING
 * ============================================================================ */

/**
 * WHAT: Process a single spike pair through STDP guard
 * WHY: Validate and constrain STDP updates
 * HOW: Check all STDP constraints, compute safe delta
 *
 * @param guard STDP guard
 * @param pair Spike pair to process
 * @param result Output: processing result with adjusted delta
 * @return 0 on success, error code on failure
 *
 * ALGORITHM:
 * 1. Calculate raw STDP delta from dt_ms
 * 2. Check spike rate limits
 * 3. Check LTP/LTD amplitude limits
 * 4. Check cumulative change limits
 * 5. Detect suspicious timing patterns
 * 6. Check against base plasticity guard (if configured)
 * 7. Clamp or reject based on configuration
 */
int stdp_guard_process_spike_pair(
    stdp_guard_t guard,
    const stdp_spike_pair_t* pair,
    stdp_update_result_t* result
);

/**
 * WHAT: Process batch of spike pairs
 * WHY: Efficiently process multiple spike pairs
 * HOW: Validate all pairs, return individual results
 *
 * @param guard STDP guard
 * @param pairs Array of spike pairs
 * @param results Array of results (same size as pairs)
 * @param count Number of spike pairs
 * @return Number of pairs successfully processed (allowed)
 */
uint32_t stdp_guard_process_batch(
    stdp_guard_t guard,
    const stdp_spike_pair_t* pairs,
    stdp_update_result_t* results,
    uint32_t count
);

/**
 * WHAT: Compute STDP delta without applying constraints
 * WHY: Get raw STDP delta for comparison
 * HOW: Apply STDP function without guard checks
 *
 * @param guard STDP guard (for STDP parameters)
 * @param dt_ms Spike timing delta (pre - post)
 * @param current_weight Current synaptic weight
 * @return Raw STDP delta (unconstrained)
 */
float stdp_guard_compute_raw_delta(
    stdp_guard_t guard,
    float dt_ms,
    float current_weight
);

/* ============================================================================
 * SPIKE RATE MONITORING
 * ============================================================================ */

/**
 * WHAT: Record a pre-synaptic spike
 * WHY: Track spike rates for rate limiting
 * HOW: Add to pre-spike sliding window
 *
 * @param guard STDP guard
 * @param synapse_id Synapse identifier
 * @param spike_time_us Spike time (microseconds)
 * @return 0 on success, error code on failure
 */
int stdp_guard_record_pre_spike(
    stdp_guard_t guard,
    uint64_t synapse_id,
    uint64_t spike_time_us
);

/**
 * WHAT: Record a post-synaptic spike
 * WHY: Track spike rates for rate limiting
 * HOW: Add to post-spike sliding window
 *
 * @param guard STDP guard
 * @param synapse_id Synapse identifier
 * @param spike_time_us Spike time (microseconds)
 * @return 0 on success, error code on failure
 */
int stdp_guard_record_post_spike(
    stdp_guard_t guard,
    uint64_t synapse_id,
    uint64_t spike_time_us
);

/**
 * WHAT: Get current spike rate for a synapse
 * WHY: Query spike rate for monitoring
 * HOW: Calculate from sliding window
 *
 * @param guard STDP guard
 * @param synapse_id Synapse identifier
 * @param pre_rate_out Output: pre-synaptic rate (Hz)
 * @param post_rate_out Output: post-synaptic rate (Hz)
 * @return 0 on success, error code on failure
 */
int stdp_guard_get_spike_rate(
    stdp_guard_t guard,
    uint64_t synapse_id,
    float* pre_rate_out,
    float* post_rate_out
);

/* ============================================================================
 * TIMING PATTERN DETECTION
 * ============================================================================ */

/**
 * WHAT: Check for suspicious timing regularity
 * WHY: Detect artificially regular spike timing patterns
 * HOW: Analyze inter-spike interval variance
 *
 * @param guard STDP guard
 * @param synapse_id Synapse to check
 * @param is_suspicious Output: true if suspicious pattern detected
 * @return 0 on success, error code on failure
 */
int stdp_guard_check_timing_regularity(
    stdp_guard_t guard,
    uint64_t synapse_id,
    bool* is_suspicious
);

/**
 * WHAT: Detect burst activity
 * WHY: Identify excessive spike bursts that could cause rapid weight changes
 * HOW: Count spikes within burst detection window
 *
 * @param guard STDP guard
 * @param synapse_id Synapse to check
 * @param is_burst Output: true if burst detected
 * @param burst_count Output: number of spikes in burst (can be NULL)
 * @return 0 on success, error code on failure
 */
int stdp_guard_detect_burst(
    stdp_guard_t guard,
    uint64_t synapse_id,
    bool* is_burst,
    uint32_t* burst_count
);

/* ============================================================================
 * CUMULATIVE TRACKING
 * ============================================================================ */

/**
 * WHAT: Get cumulative weight change for a synapse
 * WHY: Track total STDP-induced change over time
 * HOW: Return accumulated change within tracking window
 *
 * @param guard STDP guard
 * @param synapse_id Synapse identifier
 * @param cumulative_out Output: cumulative change
 * @return 0 on success, error code on failure
 */
int stdp_guard_get_cumulative_change(
    stdp_guard_t guard,
    uint64_t synapse_id,
    float* cumulative_out
);

/**
 * WHAT: Reset cumulative tracking for a synapse
 * WHY: Clear accumulated change tracking
 * HOW: Reset synapse-specific counters
 *
 * @param guard STDP guard
 * @param synapse_id Synapse identifier
 * @return 0 on success, error code on failure
 */
int stdp_guard_reset_cumulative(
    stdp_guard_t guard,
    uint64_t synapse_id
);

/* ============================================================================
 * STATISTICS AND MONITORING
 * ============================================================================ */

/**
 * WHAT: Get STDP guard statistics
 * WHY: Monitor guard effectiveness
 * HOW: Copy accumulated statistics
 *
 * @param guard STDP guard
 * @param stats Output: statistics structure
 * @return 0 on success, error code on failure
 */
int stdp_guard_get_stats(
    stdp_guard_t guard,
    stdp_guard_stats_t* stats
);

/**
 * WHAT: Reset STDP guard statistics
 * WHY: Clear statistics for fresh measurement
 * HOW: Zero all counters
 *
 * @param guard STDP guard
 * @return 0 on success, error code on failure
 */
int stdp_guard_reset_stats(stdp_guard_t guard);

/**
 * WHAT: Get LTP/LTD ratio for monitoring
 * WHY: Track asymmetry in potentiation vs depression
 * HOW: Calculate from accumulated statistics
 *
 * @param guard STDP guard
 * @param ratio_out Output: LTP/LTD ratio
 * @return 0 on success, error code on failure
 */
int stdp_guard_get_ltp_ltd_ratio(
    stdp_guard_t guard,
    float* ratio_out
);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * WHAT: Get string name for STDP violation type
 * WHY: Human-readable violation identification
 * HOW: Lookup table
 *
 * @param violation Violation type (single flag)
 * @return String name (never NULL)
 */
const char* stdp_violation_name(stdp_violation_t violation);

/**
 * WHAT: Format STDP violation flags as string
 * WHY: Human-readable multi-violation description
 * HOW: Concatenate flag names
 *
 * @param violations Violation bitmask
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of characters written
 */
int stdp_violations_to_string(
    stdp_violation_t violations,
    char* buffer,
    size_t buffer_size
);

/**
 * WHAT: Print STDP guard summary to stdout
 * WHY: Debug and diagnostic output
 * HOW: Format and print guard state
 *
 * @param guard STDP guard (NULL safe)
 */
void stdp_guard_print_summary(stdp_guard_t guard);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_STDP_GUARD_H */
