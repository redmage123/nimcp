/* ============================================================================
 * [TOMBSTONE] DEPRECATED — proposed design, never implemented.
 *
 * This header declares a bridge API whose .c implementation was never written.
 * Any code that #includes this file and calls its functions will fail at link.
 * Preserved as a design record only; do NOT add new uses.
 *
 * Status: FULL-STATUE in the 2026-04-24 consumer-bridge audit. Ghost-typedef
 * bridges like this describe cross-module couplings that were sketched but
 * never implemented.
 *
 * To revive: write the backing .c file, add it to the appropriate CMakeLists,
 * then remove this banner and validate with the `_update`/`_create` caller
 * chain ending somewhere in a hot path. See
 *   docs/claude/consumer-bridge-inventory-2026-04-24.md
 * for the full inventory + the middle-path rationale for why this is
 * tombstoned rather than deleted or implemented.
 * ========================================================================= */

//=============================================================================
// nimcp_hh_plasticity_bridge.h - Hodgkin-Huxley to STDP Plasticity Bridge
//=============================================================================
/**
 * @file nimcp_hh_plasticity_bridge.h
 * @brief Bridge between HH biophysics and STDP plasticity mechanisms
 *
 * WHAT: Integrates Hodgkin-Huxley precise spike timing with spike-timing
 *       dependent plasticity (STDP) for biophysically-grounded learning.
 *
 * WHY:  STDP learning depends critically on precise spike timing, which HH
 *       models provide naturally through ion channel dynamics. This bridge:
 *       - Uses HH action potential timing for STDP window computation
 *       - Modulates plasticity by HH biophysical state (temperature, channels)
 *       - Provides conductance-based STDP strength modulation
 *
 * HOW:  - Extracts spike times from HH threshold crossings
 *       - Computes STDP weight changes based on pre-post timing
 *       - Scales plasticity by temperature (Q10) and ion channel state
 *       - Supports multiple STDP rules (classical, triplet, voltage-dep)
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * HH SPIKE TIMING FOR STDP:
 * -------------------------
 * 1. Precise Timing from HH:
 *    - HH threshold crossing provides < 0.1 ms precision
 *    - Rising phase slope indicates timing reliability
 *    - Temperature affects timing via Q10 rate scaling
 *
 * 2. Ion Channel State Affects Plasticity:
 *    - Na+ channel availability: Recent spiking reduces LTP
 *    - K+ channel state: Afterhyperpolarization affects LTD window
 *    - Ca2+ influx: Backpropagating AP triggers dendritic plasticity
 *
 * 3. Refractory Period Constraints:
 *    - HH refractory period limits spike timing precision
 *    - Affects minimum ISI for STDP computation
 *
 * TEMPERATURE MODULATION OF STDP:
 * -------------------------------
 * - Q10 factor scales STDP time windows
 * - Higher temperature: Narrower STDP windows (faster kinetics)
 * - Lower temperature: Broader STDP windows (slower kinetics)
 * - Formula: tau_stdp(T) = tau_stdp(Tref) / Q10^((T-Tref)/10)
 *
 * CONDUCTANCE-BASED STDP MODULATION:
 * ----------------------------------
 * - High g_Na: Enhanced LTP (strong backprop AP)
 * - Reduced g_Na: Diminished LTP (weak dendritic signal)
 * - g_K state: Affects recovery and LTD timing
 * - g_Ca: Direct trigger for calcium-dependent plasticity
 *
 * STDP RULES SUPPORTED:
 * ---------------------
 * - Classical: Asymmetric exponential (Bi & Poo 1998)
 * - Symmetric: Rate-based approximation
 * - Triplet: Three-spike interactions (Pfister & Gerstner 2006)
 * - Voltage: Membrane voltage-dependent (Clopath et al. 2010)
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_HH_PLASTICITY_BRIDGE_H
#define NIMCP_HH_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define HH_PLASTICITY_MODULE_NAME       "hh_plasticity_bridge"

/** Maximum tracked synapses */
#define HH_PLASTICITY_MAX_SYNAPSES      65536

/** Maximum spike pairs per update */
#define HH_PLASTICITY_MAX_PAIRS         4096

/** Default STDP LTP time constant (ms) */
#define HH_PLASTICITY_TAU_LTP           20.0f

/** Default STDP LTD time constant (ms) */
#define HH_PLASTICITY_TAU_LTD           25.0f

/** Default LTP amplitude */
#define HH_PLASTICITY_A_LTP             0.01f

/** Default LTD amplitude */
#define HH_PLASTICITY_A_LTD             0.012f

/** Reference temperature for STDP (Celsius) */
#define HH_PLASTICITY_TEMP_REF          37.0f

/** Q10 for STDP time constants */
#define HH_PLASTICITY_Q10_STDP          2.0f

/** Q10 for STDP amplitudes */
#define HH_PLASTICITY_Q10_AMP           1.5f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief STDP rule types
 */
typedef enum {
    HH_STDP_CLASSICAL = 0,        /**< Classical asymmetric exponential */
    HH_STDP_SYMMETRIC,            /**< Symmetric (rate-based) */
    HH_STDP_TRIPLET,              /**< Triplet rule (Pfister-Gerstner) */
    HH_STDP_VOLTAGE_DEPENDENT,    /**< Voltage-dependent (Clopath) */
    HH_STDP_CALCIUM_DEPENDENT     /**< Calcium-based (Shouval) */
} hh_stdp_rule_t;

/**
 * @brief Plasticity state for synapse
 */
typedef enum {
    HH_PLASTICITY_NORMAL = 0,     /**< Normal plasticity */
    HH_PLASTICITY_POTENTIATED,    /**< Recently potentiated */
    HH_PLASTICITY_DEPRESSED,      /**< Recently depressed */
    HH_PLASTICITY_SATURATED_HIGH, /**< At maximum weight */
    HH_PLASTICITY_SATURATED_LOW,  /**< At minimum weight */
    HH_PLASTICITY_BLOCKED         /**< Plasticity blocked (refractory) */
} hh_plasticity_state_t;

/**
 * @brief Channel modulation effect on plasticity
 */
typedef enum {
    HH_CHANNEL_MOD_NONE = 0,      /**< No modulation */
    HH_CHANNEL_MOD_ENHANCE_LTP,   /**< Enhance LTP (high Na+) */
    HH_CHANNEL_MOD_REDUCE_LTP,    /**< Reduce LTP (Na+ fatigue) */
    HH_CHANNEL_MOD_ENHANCE_LTD,   /**< Enhance LTD (high K+) */
    HH_CHANNEL_MOD_REDUCE_LTD     /**< Reduce LTD */
} hh_channel_mod_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Spike timing information from HH
 */
typedef struct {
    uint32_t neuron_id;           /**< HH neuron ID */
    float spike_time_ms;          /**< Precise spike time */
    float peak_voltage;           /**< Peak AP amplitude (mV) */
    float rise_slope;             /**< dV/dt at threshold */
    float temperature;            /**< Temperature at spike (C) */
    float phi_factor;             /**< Q10 factor */
    float g_na_fraction;          /**< Na+ conductance fraction [0,1] */
    float g_k_fraction;           /**< K+ conductance fraction [0,1] */
    float ca_concentration;       /**< Intracellular Ca2+ (uM) */
} hh_spike_timing_t;

/**
 * @brief STDP event from spike pair
 */
typedef struct {
    uint32_t synapse_id;          /**< Synapse identifier */
    uint32_t pre_neuron;          /**< Presynaptic neuron ID */
    uint32_t post_neuron;         /**< Postsynaptic neuron ID */
    float delta_t_ms;             /**< Timing difference (post - pre) */
    float weight_change;          /**< Computed weight change */
    float temp_scaling;           /**< Temperature scaling applied */
    float channel_modulation;     /**< Channel state modulation */
    hh_plasticity_state_t state;  /**< Resulting plasticity state */
    float effective_tau_ltp;      /**< Temperature-adjusted tau_LTP */
    float effective_tau_ltd;      /**< Temperature-adjusted tau_LTD */
} hh_stdp_event_t;

/**
 * @brief Triplet rule state (for triplet STDP)
 */
typedef struct {
    float r1;                     /**< Fast pre trace (for post-pre-post) */
    float r2;                     /**< Slow pre trace */
    float o1;                     /**< Fast post trace (for pre-post-pre) */
    float o2;                     /**< Slow post trace */
    float tau_plus;               /**< Fast trace time constant */
    float tau_minus;              /**< Fast trace time constant */
    float tau_x;                  /**< Slow pre trace time constant */
    float tau_y;                  /**< Slow post trace time constant */
} hh_triplet_state_t;

/**
 * @brief Voltage-dependent STDP state
 */
typedef struct {
    float u_bar_plus;             /**< Low-pass filtered post voltage (fast) */
    float u_bar_minus;            /**< Low-pass filtered post voltage (slow) */
    float x_trace;                /**< Presynaptic trace */
    float theta_minus;            /**< LTD voltage threshold */
    float theta_plus;             /**< LTP voltage threshold */
    float tau_v_minus;            /**< Slow voltage filter tau */
    float tau_v_plus;             /**< Fast voltage filter tau */
} hh_voltage_stdp_state_t;

/**
 * @brief Synapse plasticity tracking
 */
typedef struct {
    uint32_t synapse_id;          /**< Synapse identifier */
    uint32_t pre_neuron;          /**< Presynaptic neuron */
    uint32_t post_neuron;         /**< Postsynaptic neuron */
    float weight;                 /**< Current synaptic weight */
    float weight_min;             /**< Minimum weight */
    float weight_max;             /**< Maximum weight */
    float last_pre_spike_ms;      /**< Last presynaptic spike time */
    float last_post_spike_ms;     /**< Last postsynaptic spike time */
    float eligibility_trace;      /**< Eligibility trace for 3-factor */
    hh_plasticity_state_t state;  /**< Current plasticity state */

    /* Rule-specific state */
    hh_triplet_state_t triplet;   /**< Triplet rule state */
    hh_voltage_stdp_state_t voltage; /**< Voltage-dep state */
} hh_synapse_plasticity_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* STDP rule selection */
    hh_stdp_rule_t rule;          /**< STDP rule type */

    /* Classical STDP parameters */
    float tau_ltp_ms;             /**< LTP time constant */
    float tau_ltd_ms;             /**< LTD time constant */
    float a_ltp;                  /**< LTP amplitude */
    float a_ltd;                  /**< LTD amplitude */

    /* Temperature modulation */
    bool enable_temp_scaling;     /**< Scale STDP by temperature */
    float temp_reference;         /**< Reference temperature (C) */
    float q10_tau;                /**< Q10 for time constants */
    float q10_amplitude;          /**< Q10 for amplitudes */

    /* Ion channel modulation */
    bool enable_channel_modulation; /**< Modulate by channel state */
    float na_ltp_threshold;       /**< Na+ threshold for LTP boost */
    float ca_ltp_threshold;       /**< Ca2+ threshold for LTP */

    /* Weight constraints */
    float weight_min;             /**< Global minimum weight */
    float weight_max;             /**< Global maximum weight */
    bool enable_soft_bounds;      /**< Use soft bounds (multiplicative) */

    /* Eligibility traces */
    bool enable_eligibility;      /**< Enable eligibility traces */
    float eligibility_tau_ms;     /**< Eligibility decay time constant */

    /* Update parameters */
    float update_interval_ms;     /**< Bridge update interval */
    bool batch_updates;           /**< Batch plasticity updates */
} hh_plasticity_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Event counts */
    uint64_t spike_pairs_processed;    /**< Total spike pairs processed */
    uint64_t ltp_events;               /**< LTP events (positive delta_w) */
    uint64_t ltd_events;               /**< LTD events (negative delta_w) */

    /* Weight change statistics */
    float total_ltp;                   /**< Cumulative LTP */
    float total_ltd;                   /**< Cumulative LTD */
    float net_weight_change;           /**< Net weight change (LTP - LTD) */
    float avg_weight_change;           /**< Average weight change magnitude */
    float max_weight_change;           /**< Maximum single weight change */

    /* Temperature effects */
    float avg_temp_scaling;            /**< Average temperature scaling */
    float avg_effective_tau_ltp;       /**< Average effective tau_LTP */
    float avg_effective_tau_ltd;       /**< Average effective tau_LTD */

    /* Channel modulation */
    uint64_t channel_enhanced_events;  /**< Events enhanced by channels */
    uint64_t channel_reduced_events;   /**< Events reduced by channels */
    float avg_channel_modulation;      /**< Average channel modulation */

    /* Saturation */
    uint64_t saturated_high;           /**< Synapses at max weight */
    uint64_t saturated_low;            /**< Synapses at min weight */

    /* Performance */
    float last_update_ms;              /**< Last update timestamp */
    float processing_latency_us;       /**< Processing latency */
} hh_plasticity_stats_t;

/** Opaque bridge handle */
typedef struct hh_plasticity_bridge_struct hh_plasticity_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Initialize configuration with biologically-motivated defaults
 * WHY:  Easy creation with standard STDP parameters
 * HOW:  Set classical rule, standard time constants, enable temp scaling
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_plasticity_default_config(hh_plasticity_config_t* config);

/**
 * @brief Get configuration for specific STDP rule
 *
 * @param config Configuration to initialize
 * @param rule STDP rule type
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_plasticity_config_for_rule(
    hh_plasticity_config_t* config,
    hh_stdp_rule_t rule
);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create HH-plasticity bridge
 *
 * WHAT: Initialize bridge for HH-based STDP computation
 * WHY:  Enable biophysically-grounded synaptic plasticity
 * HOW:  Allocate synapse tracking, initialize STDP state
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT hh_plasticity_bridge_t* hh_plasticity_bridge_create(
    const hh_plasticity_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
NIMCP_EXPORT void hh_plasticity_bridge_destroy(hh_plasticity_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_plasticity_bridge_reset(hh_plasticity_bridge_t* bridge);

//=============================================================================
// Synapse Management API
//=============================================================================

/**
 * @brief Register synapse for plasticity tracking
 *
 * WHAT: Add synapse to bridge for STDP computation
 * WHY:  Track spike timing for specific synapses
 * HOW:  Initialize synapse state, set initial weight
 *
 * @param bridge Bridge handle
 * @param synapse_id Unique synapse identifier
 * @param pre_neuron Presynaptic HH neuron ID
 * @param post_neuron Postsynaptic HH neuron ID
 * @param initial_weight Initial synaptic weight
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_plasticity_register_synapse(
    hh_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    uint32_t pre_neuron,
    uint32_t post_neuron,
    float initial_weight
);

/**
 * @brief Unregister synapse
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse to remove
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_plasticity_unregister_synapse(
    hh_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get synapse plasticity state
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse to query
 * @param state Output synapse state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_plasticity_get_synapse(
    const hh_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    hh_synapse_plasticity_t* state
);

//=============================================================================
// Spike Processing API
//=============================================================================

/**
 * @brief Process presynaptic spike
 *
 * WHAT: Record presynaptic spike for STDP computation
 * WHY:  Pre spike timing needed for STDP window
 * HOW:  Store spike time, compute LTD if post spike recent
 *
 * @param bridge Bridge handle
 * @param spike Spike timing from HH neuron
 * @param events_out Output STDP events (for affected synapses)
 * @param max_events Maximum events to return
 * @return Number of STDP events generated, -1 on error
 */
NIMCP_EXPORT int hh_plasticity_pre_spike(
    hh_plasticity_bridge_t* bridge,
    const hh_spike_timing_t* spike,
    hh_stdp_event_t* events_out,
    uint32_t max_events
);

/**
 * @brief Process postsynaptic spike
 *
 * WHAT: Record postsynaptic spike for STDP computation
 * WHY:  Post spike timing needed for LTP computation
 * HOW:  Store spike time, compute LTP if pre spike recent
 *
 * @param bridge Bridge handle
 * @param spike Spike timing from HH neuron
 * @param events_out Output STDP events (for affected synapses)
 * @param max_events Maximum events to return
 * @return Number of STDP events generated, -1 on error
 */
NIMCP_EXPORT int hh_plasticity_post_spike(
    hh_plasticity_bridge_t* bridge,
    const hh_spike_timing_t* spike,
    hh_stdp_event_t* events_out,
    uint32_t max_events
);

/**
 * @brief Compute STDP for spike pair
 *
 * WHAT: Calculate weight change for pre-post spike pair
 * WHY:  Core STDP computation with HH modulation
 * HOW:  Apply rule, scale by temperature and channel state
 *
 * @param bridge Bridge handle
 * @param pre_spike Presynaptic spike timing
 * @param post_spike Postsynaptic spike timing
 * @param synapse_id Synapse for weight update
 * @param event_out Output STDP event
 * @return Weight change value, NaN on error
 */
NIMCP_EXPORT float hh_plasticity_compute_stdp(
    hh_plasticity_bridge_t* bridge,
    const hh_spike_timing_t* pre_spike,
    const hh_spike_timing_t* post_spike,
    uint32_t synapse_id,
    hh_stdp_event_t* event_out
);

//=============================================================================
// Temperature Modulation API
//=============================================================================

/**
 * @brief Set current temperature for STDP scaling
 *
 * WHAT: Update temperature for Q10 modulation
 * WHY:  Temperature affects STDP time windows and amplitudes
 * HOW:  Store temperature, recompute scaling factors
 *
 * @param bridge Bridge handle
 * @param temperature Current temperature (Celsius)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_plasticity_set_temperature(
    hh_plasticity_bridge_t* bridge,
    float temperature
);

/**
 * @brief Get temperature-scaled STDP parameters
 *
 * WHAT: Query effective STDP parameters at current temperature
 * WHY:  Inspect temperature effects on plasticity
 * HOW:  Return scaled tau and amplitude values
 *
 * @param bridge Bridge handle
 * @param tau_ltp_out Output effective tau_LTP
 * @param tau_ltd_out Output effective tau_LTD
 * @param a_ltp_out Output effective A_LTP
 * @param a_ltd_out Output effective A_LTD
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_plasticity_get_temp_params(
    const hh_plasticity_bridge_t* bridge,
    float* tau_ltp_out,
    float* tau_ltd_out,
    float* a_ltp_out,
    float* a_ltd_out
);

//=============================================================================
// Weight Update API
//=============================================================================

/**
 * @brief Apply pending weight updates
 *
 * WHAT: Apply accumulated weight changes to synapses
 * WHY:  Batch weight updates for efficiency
 * HOW:  Apply all pending changes, enforce bounds
 *
 * @param bridge Bridge handle
 * @return Number of synapses updated, -1 on error
 */
NIMCP_EXPORT int hh_plasticity_apply_updates(hh_plasticity_bridge_t* bridge);

/**
 * @brief Get weight for synapse
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse to query
 * @param weight Output weight value
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_plasticity_get_weight(
    const hh_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float* weight
);

/**
 * @brief Set weight for synapse (override plasticity)
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse to modify
 * @param weight New weight value
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_plasticity_set_weight(
    hh_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float weight
);

//=============================================================================
// Eligibility Trace API
//=============================================================================

/**
 * @brief Update eligibility trace for synapse
 *
 * WHAT: Increment eligibility based on STDP event
 * WHY:  Three-factor learning requires eligibility traces
 * HOW:  Store trace, decay over time
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse to update
 * @param stdp_magnitude STDP contribution to trace
 * @return Current trace value, -1 on error
 */
NIMCP_EXPORT float hh_plasticity_update_eligibility(
    hh_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float stdp_magnitude
);

/**
 * @brief Convert eligibility to weight change
 *
 * WHAT: Apply modulator to convert eligibility to learning
 * WHY:  Three-factor rule: eligibility x modulator = delta_w
 * HOW:  Multiply trace by modulator, apply to weight
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse to update
 * @param modulator Neuromodulatory signal (e.g., dopamine)
 * @return Weight change applied
 */
NIMCP_EXPORT float hh_plasticity_convert_eligibility(
    hh_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float modulator
);

/**
 * @brief Decay all eligibility traces
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step for decay
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_plasticity_decay_eligibility(
    hh_plasticity_bridge_t* bridge,
    float dt_ms
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic bridge housekeeping
 * WHY:  Decay traces, apply pending updates
 * HOW:  Process time-dependent state changes
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_plasticity_bridge_update(
    hh_plasticity_bridge_t* bridge,
    float dt_ms
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_plasticity_get_stats(
    const hh_plasticity_bridge_t* bridge,
    hh_plasticity_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_plasticity_reset_stats(hh_plasticity_bridge_t* bridge);

/**
 * @brief Print bridge summary
 *
 * @param bridge Bridge handle (NULL safe)
 */
NIMCP_EXPORT void hh_plasticity_print_summary(const hh_plasticity_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HH_PLASTICITY_BRIDGE_H */