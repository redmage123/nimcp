/**
 * @file nimcp_mirror_stdp.h
 * @brief Spike-Timing Dependent Plasticity for Mirror Neurons
 * @version 1.0.0
 * @date 2025-11-25
 *
 * WHAT: Biologically accurate STDP learning rules for mirror neuron associations
 * WHY:  Replace simple Hebbian learning with timing-dependent plasticity
 * HOW:  LTP when observation precedes execution, LTD when reversed
 *
 * Spike-Timing Dependent Plasticity (STDP) is a biological learning rule where:
 * - If presynaptic spike precedes postsynaptic spike (Δt > 0): LTP (strengthen)
 * - If postsynaptic spike precedes presynaptic spike (Δt < 0): LTD (weaken)
 *
 * For mirror neurons:
 * - Observation (pre) → Execution (post): Strengthen association (learn imitation)
 * - Execution (post) → Observation (pre): Weaken (avoid self-observation confusion)
 *
 * Biological Basis:
 * - Markram et al. (1997): Original STDP discovery
 * - Bi & Poo (1998): Asymmetric timing windows
 * - Caporale & Dan (2008): STDP in cortical circuits
 *
 * Key Features:
 * 1. Asymmetric timing window (LTP window wider than LTD)
 * 2. Triplet STDP rule (accounts for burst patterns)
 * 3. Metaplasticity (history-dependent threshold)
 * 4. Homeostatic scaling (prevent runaway potentiation)
 *
 * Integration Points:
 * - Mirror neuron observation pathway (presynaptic)
 * - Mirror neuron execution pathway (postsynaptic)
 * - Substrate layer dendritic spines
 * - Neuromodulator gating (dopamine, ACh)
 *
 * @see Phase 10.11.4 - Enhanced Mirror Neuron Learning
 */

#ifndef NIMCP_MIRROR_STDP_H
#define NIMCP_MIRROR_STDP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants and Defaults
//=============================================================================

/** @brief Default LTP time window (ms) - observation before execution */
#define NIMCP_STDP_LTP_WINDOW_MS        50.0f

/** @brief Default LTD time window (ms) - execution before observation */
#define NIMCP_STDP_LTD_WINDOW_MS        30.0f

/** @brief Default LTP amplitude */
#define NIMCP_STDP_A_PLUS               0.005f

/** @brief Default LTD amplitude (negative) */
#define NIMCP_STDP_A_MINUS              0.003f

/** @brief LTP time constant (tau+) in ms */
#define NIMCP_STDP_TAU_PLUS             20.0f

/** @brief LTD time constant (tau-) in ms */
#define NIMCP_STDP_TAU_MINUS            20.0f

/** @brief Maximum synaptic weight */
#define NIMCP_STDP_W_MAX                1.0f

/** @brief Minimum synaptic weight */
#define NIMCP_STDP_W_MIN                0.0f

/** @brief Homeostatic target rate (Hz) */
#define NIMCP_STDP_TARGET_RATE          5.0f

/** @brief Homeostatic time constant (ms) */
#define NIMCP_STDP_TAU_HOMEO            1000.0f

/** @brief Triplet rule slow time constant (ms) */
#define NIMCP_STDP_TAU_TRIPLET          100.0f

/** @brief Maximum spike events to track per synapse */
#define NIMCP_STDP_MAX_SPIKE_HISTORY    16

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief STDP configuration parameters
 *
 * WHAT: Configuration for STDP learning rules
 * WHY:  Allow customization of timing windows and learning rates
 */
typedef struct {
    // Timing windows
    float ltp_window_ms;          /**< LTP time window (default: 50ms) */
    float ltd_window_ms;          /**< LTD time window (default: 30ms) */

    // Amplitude parameters
    float A_plus;                 /**< LTP amplitude (default: 0.005) */
    float A_minus;                /**< LTD amplitude (default: 0.003) */

    // Time constants
    float tau_plus;               /**< LTP decay constant (default: 20ms) */
    float tau_minus;              /**< LTD decay constant (default: 20ms) */

    // Weight bounds
    float w_max;                  /**< Maximum weight (default: 1.0) */
    float w_min;                  /**< Minimum weight (default: 0.0) */

    // Homeostatic plasticity
    bool enable_homeostasis;      /**< Enable homeostatic scaling (default: true) */
    float target_rate;            /**< Target firing rate (Hz, default: 5.0) */
    float tau_homeostasis;        /**< Homeostasis time constant (ms, default: 1000) */

    // Triplet STDP
    bool enable_triplet;          /**< Enable triplet rule (default: true) */
    float tau_triplet;            /**< Triplet slow time constant (default: 100ms) */
    float A_triplet;              /**< Triplet amplitude multiplier (default: 0.1) */

    // Neuromodulator gating
    bool enable_dopamine_gating;  /**< Dopamine modulates STDP (default: true) */
    bool enable_ach_gating;       /**< Acetylcholine modulates STDP (default: true) */
    float dopamine_ltp_boost;     /**< DA boost to LTP (default: 2.0x) */
    float ach_attention_boost;    /**< ACh attention boost (default: 1.5x) */

    // Metaplasticity
    bool enable_metaplasticity;   /**< History-dependent threshold (default: true) */
    float meta_tau;               /**< Metaplasticity time constant (default: 10000ms) */
    float meta_threshold;         /**< Activity threshold for metaplasticity (default: 0.5) */

} mirror_stdp_config_t;

/**
 * @brief Spike timing record
 *
 * WHAT: Records timing of observation/execution spikes
 * WHY:  Enable precise STDP calculations
 */
typedef struct {
    uint64_t timestamp_us;        /**< Spike time in microseconds */
    float strength;               /**< Spike strength (for rate coding) */
    bool is_observation;          /**< True if observation spike, false if execution */
} stdp_spike_t;

/**
 * @brief STDP synapse state
 *
 * WHAT: State for one mirror neuron synapse undergoing STDP
 * WHY:  Track all information needed for STDP learning
 */
typedef struct {
    // Synapse identification
    uint32_t synapse_id;          /**< Unique synapse ID */
    uint32_t action_id;           /**< Associated action */

    // Current weight
    float weight;                 /**< Current synaptic weight [0, 1] */
    float initial_weight;         /**< Weight at creation (for analysis) */

    // Eligibility traces (for triplet STDP)
    float r1;                     /**< Fast presynaptic trace */
    float r2;                     /**< Slow presynaptic trace (triplet) */
    float o1;                     /**< Fast postsynaptic trace */
    float o2;                     /**< Slow postsynaptic trace (triplet) */

    // Spike history
    stdp_spike_t obs_spikes[NIMCP_STDP_MAX_SPIKE_HISTORY];  /**< Observation spike history */
    stdp_spike_t exec_spikes[NIMCP_STDP_MAX_SPIKE_HISTORY]; /**< Execution spike history */
    uint8_t obs_spike_count;      /**< Number of stored observation spikes */
    uint8_t exec_spike_count;     /**< Number of stored execution spikes */

    // Rate tracking (for homeostasis)
    float avg_obs_rate;           /**< Average observation rate (Hz) */
    float avg_exec_rate;          /**< Average execution rate (Hz) */

    // Metaplasticity state
    float meta_state;             /**< Metaplastic threshold modifier */
    float activity_history;       /**< Integrated activity for metaplasticity */

    // Statistics
    uint32_t ltp_events;          /**< Count of LTP events */
    uint32_t ltd_events;          /**< Count of LTD events */
    float total_ltp;              /**< Cumulative LTP magnitude */
    float total_ltd;              /**< Cumulative LTD magnitude */
    uint64_t last_update_us;      /**< Last update timestamp */

} mirror_stdp_synapse_t;

/**
 * @brief STDP system state
 *
 * WHAT: Complete STDP system for mirror neurons
 * WHY:  Manage all STDP synapses and learning
 */
typedef struct mirror_stdp_system mirror_stdp_system_t;
typedef mirror_stdp_system_t* mirror_stdp_t;

/**
 * @brief STDP statistics
 *
 * WHAT: Runtime statistics for STDP learning
 * WHY:  Monitor learning progress and health
 */
typedef struct {
    uint32_t num_synapses;        /**< Total STDP synapses */
    uint32_t active_synapses;     /**< Synapses with recent activity */

    // Learning events
    uint32_t total_ltp_events;    /**< Total LTP events */
    uint32_t total_ltd_events;    /**< Total LTD events */
    float avg_ltp_magnitude;      /**< Average LTP change */
    float avg_ltd_magnitude;      /**< Average LTD change */

    // Weight distribution
    float mean_weight;            /**< Mean synaptic weight */
    float weight_variance;        /**< Weight variance */
    float min_weight;             /**< Minimum weight */
    float max_weight;             /**< Maximum weight */

    // Timing statistics
    float avg_delta_t_ltp;        /**< Average Δt for LTP events (ms) */
    float avg_delta_t_ltd;        /**< Average Δt for LTD events (ms) */

    // Rate statistics
    float mean_obs_rate;          /**< Mean observation rate (Hz) */
    float mean_exec_rate;         /**< Mean execution rate (Hz) */

    // Homeostasis
    uint32_t homeostatic_adjustments; /**< Number of homeostatic corrections */
    float homeostatic_scale_factor;   /**< Current global scale factor */

} mirror_stdp_stats_t;

//=============================================================================
// Lifecycle Management
//=============================================================================

/**
 * @brief Get default STDP configuration
 *
 * WHAT: Return sensible defaults for STDP learning
 * WHY:  Provide biological plausible starting point
 * HOW:  Return pre-configured struct based on literature values
 *
 * @return Default configuration
 */
mirror_stdp_config_t mirror_stdp_get_default_config(void);

/**
 * @brief Create STDP system
 *
 * WHAT: Initialize STDP learning system for mirror neurons
 * WHY:  Enable timing-dependent plasticity
 * HOW:  Allocate synapse storage, initialize parameters
 *
 * @param config Configuration (NULL = use defaults)
 * @param max_synapses Maximum number of synapses to manage
 * @return STDP system handle or NULL on error
 */
mirror_stdp_t mirror_stdp_create(const mirror_stdp_config_t* config, uint32_t max_synapses);

/**
 * @brief Destroy STDP system
 *
 * WHAT: Free all STDP resources
 * WHY:  Prevent memory leaks
 *
 * @param stdp STDP system to destroy (NULL-safe)
 */
void mirror_stdp_destroy(mirror_stdp_t stdp);

//=============================================================================
// Synapse Management
//=============================================================================

/**
 * @brief Create new STDP synapse
 *
 * WHAT: Add a new synapse to STDP management
 * WHY:  Enable STDP learning for this obs-exec connection
 *
 * @param stdp STDP system
 * @param action_id Action this synapse represents
 * @param initial_weight Initial synaptic weight (0-1)
 * @return Synapse ID or UINT32_MAX on error
 */
uint32_t mirror_stdp_create_synapse(mirror_stdp_t stdp, uint32_t action_id, float initial_weight);

/**
 * @brief Get synapse state
 *
 * WHAT: Query current state of a synapse
 * WHY:  Monitor learning progress
 *
 * @param stdp STDP system
 * @param synapse_id Synapse to query
 * @param out_synapse Output: synapse state
 * @return true on success, false if not found
 */
bool mirror_stdp_get_synapse(mirror_stdp_t stdp, uint32_t synapse_id, mirror_stdp_synapse_t* out_synapse);

/**
 * @brief Get synapse weight
 *
 * WHAT: Query current weight of a synapse
 * WHY:  Fast weight lookup for activation calculations
 *
 * @param stdp STDP system
 * @param synapse_id Synapse to query
 * @return Weight [0, 1] or -1 on error
 */
float mirror_stdp_get_weight(mirror_stdp_t stdp, uint32_t synapse_id);

/**
 * @brief Find synapse by action ID
 *
 * WHAT: Look up synapse for a specific action
 * WHY:  Map actions to their STDP synapses
 *
 * @param stdp STDP system
 * @param action_id Action to find
 * @return Synapse ID or UINT32_MAX if not found
 */
uint32_t mirror_stdp_find_synapse(mirror_stdp_t stdp, uint32_t action_id);

//=============================================================================
// Spike Processing
//=============================================================================

/**
 * @brief Record observation spike
 *
 * WHAT: Record presynaptic spike from observation pathway
 * WHY:  Trigger STDP update if execution spike is recent
 * HOW:  Store spike, compute Δt with recent exec spikes, apply STDP
 *
 * @param stdp STDP system
 * @param synapse_id Synapse receiving spike
 * @param timestamp_us Spike time in microseconds
 * @param strength Spike strength (typically 1.0)
 * @return Weight change applied (can be + or -)
 */
float mirror_stdp_observation_spike(mirror_stdp_t stdp, uint32_t synapse_id,
                                     uint64_t timestamp_us, float strength);

/**
 * @brief Record execution spike
 *
 * WHAT: Record postsynaptic spike from execution pathway
 * WHY:  Trigger STDP update if observation spike is recent
 * HOW:  Store spike, compute Δt with recent obs spikes, apply STDP
 *
 * @param stdp STDP system
 * @param synapse_id Synapse that spiked
 * @param timestamp_us Spike time in microseconds
 * @param strength Spike strength (typically 1.0)
 * @return Weight change applied (can be + or -)
 */
float mirror_stdp_execution_spike(mirror_stdp_t stdp, uint32_t synapse_id,
                                   uint64_t timestamp_us, float strength);

/**
 * @brief Compute STDP weight change
 *
 * WHAT: Calculate weight change for given spike timing
 * WHY:  Core STDP computation
 * HOW:  Apply asymmetric exponential timing rule
 *
 * Formula:
 *   Δt > 0 (obs before exec): Δw = A+ × exp(-Δt / τ+) × (w_max - w)
 *   Δt < 0 (exec before obs): Δw = -A- × exp(Δt / τ-) × (w - w_min)
 *
 * @param stdp STDP system
 * @param delta_t_ms Time difference (obs_time - exec_time) in ms
 * @param current_weight Current synaptic weight
 * @param dopamine_level Dopamine modulation (0-1, default 0.5)
 * @param ach_level Acetylcholine modulation (0-1, default 0.5)
 * @return Weight change (positive = LTP, negative = LTD)
 */
float mirror_stdp_compute_delta_w(mirror_stdp_t stdp, float delta_t_ms, float current_weight,
                                   float dopamine_level, float ach_level);

//=============================================================================
// Trace and Eligibility
//=============================================================================

/**
 * @brief Update eligibility traces
 *
 * WHAT: Decay eligibility traces for all synapses
 * WHY:  Traces track recent activity for triplet STDP
 * HOW:  Exponential decay with time constants
 *
 * @param stdp STDP system
 * @param dt_ms Time step in milliseconds
 */
void mirror_stdp_update_traces(mirror_stdp_t stdp, float dt_ms);

/**
 * @brief Get observation eligibility trace
 *
 * WHAT: Query current observation eligibility for synapse
 * WHY:  Used for triplet STDP computation
 *
 * @param stdp STDP system
 * @param synapse_id Synapse to query
 * @return Eligibility trace value [0, 1]
 */
float mirror_stdp_get_obs_trace(mirror_stdp_t stdp, uint32_t synapse_id);

/**
 * @brief Get execution eligibility trace
 *
 * WHAT: Query current execution eligibility for synapse
 * WHY:  Used for triplet STDP computation
 *
 * @param stdp STDP system
 * @param synapse_id Synapse to query
 * @return Eligibility trace value [0, 1]
 */
float mirror_stdp_get_exec_trace(mirror_stdp_t stdp, uint32_t synapse_id);

//=============================================================================
// Homeostatic Plasticity
//=============================================================================

/**
 * @brief Apply homeostatic scaling
 *
 * WHAT: Scale weights to maintain target activity
 * WHY:  Prevent runaway potentiation/depression
 * HOW:  Multiplicative scaling based on rate deviation
 *
 * @param stdp STDP system
 * @param dt_ms Time step for rate calculation
 */
void mirror_stdp_apply_homeostasis(mirror_stdp_t stdp, float dt_ms);

/**
 * @brief Update metaplasticity state
 *
 * WHAT: Adjust STDP thresholds based on activity history
 * WHY:  BCM-like sliding threshold prevents saturation
 * HOW:  Track integrated activity, shift LTP/LTD threshold
 *
 * @param stdp STDP system
 * @param dt_ms Time step
 */
void mirror_stdp_update_metaplasticity(mirror_stdp_t stdp, float dt_ms);

//=============================================================================
// Neuromodulator Integration
//=============================================================================

/**
 * @brief Set dopamine level for STDP modulation
 *
 * WHAT: Update dopamine modulation level
 * WHY:  Dopamine gates reward-dependent plasticity
 * HOW:  Scale LTP amplitude by dopamine level
 *
 * @param stdp STDP system
 * @param level Dopamine level (0-1)
 */
void mirror_stdp_set_dopamine(mirror_stdp_t stdp, float level);

/**
 * @brief Set acetylcholine level for STDP modulation
 *
 * WHAT: Update ACh modulation level
 * WHY:  ACh enhances attention-dependent learning
 * HOW:  Scale overall STDP by attention level
 *
 * @param stdp STDP system
 * @param level ACh level (0-1)
 */
void mirror_stdp_set_acetylcholine(mirror_stdp_t stdp, float level);

//=============================================================================
// Statistics and Analysis
//=============================================================================

/**
 * @brief Get STDP statistics
 *
 * WHAT: Retrieve comprehensive STDP statistics
 * WHY:  Monitor learning health and progress
 *
 * @param stdp STDP system
 * @param stats Output: statistics
 * @return true on success
 */
bool mirror_stdp_get_stats(mirror_stdp_t stdp, mirror_stdp_stats_t* stats);

/**
 * @brief Get weight histogram
 *
 * WHAT: Compute histogram of synaptic weights
 * WHY:  Analyze weight distribution
 *
 * @param stdp STDP system
 * @param bins Output array for bin counts
 * @param num_bins Number of bins (recommended: 10-20)
 * @return true on success
 */
bool mirror_stdp_get_weight_histogram(mirror_stdp_t stdp, uint32_t* bins, uint32_t num_bins);

/**
 * @brief Step STDP simulation
 *
 * WHAT: Advance STDP system by one timestep
 * WHY:  Update all time-dependent processes
 * HOW:  Decay traces, apply homeostasis, update metaplasticity
 *
 * @param stdp STDP system
 * @param dt_ms Time step in milliseconds
 */
void mirror_stdp_step(mirror_stdp_t stdp, float dt_ms);

/**
 * @brief Reset STDP statistics
 *
 * WHAT: Clear accumulated statistics
 * WHY:  Start fresh measurement period
 *
 * @param stdp STDP system
 */
void mirror_stdp_reset_stats(mirror_stdp_t stdp);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MIRROR_STDP_H
