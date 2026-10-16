//=============================================================================
// nimcp_physics_snn_bridge.h - Physics Layer to SNN/Plasticity Bridge
//=============================================================================
/**
 * @file nimcp_physics_snn_bridge.h
 * @brief Bidirectional bridge between HH biophysics and SNN/Plasticity systems
 *
 * WHAT: Connects Hodgkin-Huxley biophysics with Spiking Neural Networks and
 *       STDP plasticity mechanisms, enabling biophysically-grounded learning.
 *
 * WHY:  Bridges the gap between:
 *       - Biophysical realism (HH ion channels, action potentials)
 *       - Network-level computation (SNN population dynamics)
 *       - Synaptic plasticity (STDP, eligibility traces)
 *
 * HOW:  Two-way integration:
 *       1. HH → SNN: Spike trains with precise timing drive SNN computation
 *       2. SNN → HH: Network output modulates biophysical parameters
 *       3. HH timing → STDP: Biophysical spike times determine plasticity
 *       4. Temperature/ATP → learning rate modulation
 *
 * BIOLOGICAL BASIS:
 * ```
 * HH BIOPHYSICS                         SNN/PLASTICITY
 * ─────────────────────────────────────────────────────────────────
 * Action potentials (precise timing) → SNN input spikes
 * Ion channel states                 → Synaptic efficacy modulation
 * Temperature (Q10 effects)          → STDP time window scaling
 * ATP level                          → Learning rate gating
 * Population firing rate             → Network activity metric
 * Refractory period                  → Eligibility trace timing
 * ```
 *
 * STDP MODULATION BY PHYSICS:
 * - Temperature affects STDP window width (Q10 scaling)
 * - ATP depletion reduces LTP magnitude
 * - Ephaptic coherence gates eligibility conversion
 * - Thermodynamic efficiency affects consolidation
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_PHYSICS_SNN_BRIDGE_H
#define NIMCP_PHYSICS_SNN_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define PHYSICS_SNN_MODULE_NAME     "physics_snn_bridge"

/** Maximum tracked spike sources */
#define PHYSICS_SNN_MAX_SOURCES     256

/** Maximum STDP pairs per update */
#define PHYSICS_SNN_MAX_STDP_PAIRS  1024

/** Default STDP LTP window (ms) */
#define PHYSICS_SNN_LTP_WINDOW      20.0f

/** Default STDP LTD window (ms) */
#define PHYSICS_SNN_LTD_WINDOW      25.0f

/** Temperature reference for Q10 (K) */
#define PHYSICS_SNN_TEMP_REF        310.15f

/** Q10 factor for STDP window */
#define PHYSICS_SNN_STDP_Q10        2.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Spike encoding method for HH → SNN
 */
typedef enum {
    PHYSICS_SNN_ENCODE_PRECISE = 0,  /**< Precise spike times */
    PHYSICS_SNN_ENCODE_RATE,         /**< Rate coding */
    PHYSICS_SNN_ENCODE_BURST,        /**< Burst pattern detection */
    PHYSICS_SNN_ENCODE_PHASE         /**< Phase relative to LFP */
} physics_snn_encoding_t;

/**
 * @brief STDP rule type
 */
typedef enum {
    PHYSICS_SNN_STDP_CLASSICAL = 0,  /**< Classical asymmetric STDP */
    PHYSICS_SNN_STDP_SYMMETRIC,      /**< Symmetric (rate-based) */
    PHYSICS_SNN_STDP_TRIPLET,        /**< Triplet rule */
    PHYSICS_SNN_STDP_VOLTAGE         /**< Voltage-dependent */
} physics_snn_stdp_rule_t;

/**
 * @brief Physics modulation target
 */
typedef enum {
    PHYSICS_SNN_MOD_CONDUCTANCE = 0, /**< Modulate g_Na, g_K */
    PHYSICS_SNN_MOD_THRESHOLD,       /**< Modulate spike threshold */
    PHYSICS_SNN_MOD_TIME_CONST,      /**< Modulate tau_m */
    PHYSICS_SNN_MOD_REVERSAL         /**< Modulate reversal potentials */
} physics_snn_mod_target_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for physics-SNN bridge
 */
typedef struct {
    /** Encoding parameters */
    physics_snn_encoding_t encoding;     /**< Spike encoding method */
    float spike_threshold_mv;            /**< Threshold for spike detection */
    float refractory_ms;                 /**< Refractory period */

    /** STDP parameters */
    physics_snn_stdp_rule_t stdp_rule;   /**< STDP rule type */
    float ltp_window_ms;                 /**< LTP time window */
    float ltd_window_ms;                 /**< LTD time window */
    float ltp_amplitude;                 /**< Max LTP magnitude */
    float ltd_amplitude;                 /**< Max LTD magnitude */

    /** Physics modulation of STDP */
    bool enable_temp_scaling;            /**< Temperature scales STDP window */
    bool enable_atp_gating;              /**< ATP gates LTP */
    float atp_ltp_threshold;             /**< ATP threshold for LTP */
    float temp_q10;                      /**< Q10 for STDP window */

    /** Eligibility traces */
    bool enable_eligibility;             /**< Use eligibility traces */
    float eligibility_decay_ms;          /**< Trace decay time constant */
    float coherence_gate_threshold;      /**< Ephaptic coherence for gating */

    /** SNN → HH modulation */
    bool enable_feedback;                /**< Enable SNN → HH feedback */
    physics_snn_mod_target_t mod_target; /**< What to modulate */
    float mod_strength;                  /**< Modulation strength */

    /** Update intervals */
    float update_interval_ms;            /**< Bridge update interval */
} physics_snn_config_t;

/**
 * @brief Spike event from HH neuron
 */
typedef struct {
    uint32_t source_id;                  /**< HH neuron ID */
    float spike_time_ms;                 /**< Precise spike time */
    float membrane_voltage;              /**< Voltage at spike */
    float temperature;                   /**< Temperature at spike */
    float atp_level;                     /**< ATP level at spike */
} physics_snn_spike_t;

/**
 * @brief STDP update event
 */
typedef struct {
    uint32_t pre_id;                     /**< Presynaptic neuron */
    uint32_t post_id;                    /**< Postsynaptic neuron */
    float dt_ms;                         /**< Time difference (post - pre) */
    float weight_change;                 /**< Computed weight change */
    float temperature_factor;            /**< Temperature scaling applied */
    float atp_factor;                    /**< ATP gating applied */
} physics_snn_stdp_event_t;

/**
 * @brief Eligibility trace state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Synapse identifier */
    float trace_value;                   /**< Current trace magnitude */
    float last_update_ms;                /**< Last update time */
    bool converted;                      /**< Whether trace was converted */
} physics_snn_eligibility_t;

/**
 * @brief Modulation signal from SNN to HH
 */
typedef struct {
    physics_snn_mod_target_t target;     /**< Modulation target */
    float g_na_factor;                   /**< Na conductance factor */
    float g_k_factor;                    /**< K conductance factor */
    float threshold_shift;               /**< Threshold shift (mV) */
    float tau_factor;                    /**< Time constant factor */
} physics_snn_modulation_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t spikes_encoded;             /**< Total spikes encoded */
    uint64_t stdp_updates;               /**< STDP weight updates */
    uint64_t eligibility_conversions;    /**< Traces converted to weights */
    uint64_t feedback_events;            /**< SNN → HH modulations */
    float avg_ltp_magnitude;             /**< Average LTP change */
    float avg_ltd_magnitude;             /**< Average LTD change */
    float total_weight_change;           /**< Net weight change */
    float last_update_ms;                /**< Last update timestamp */
} physics_snn_stats_t;

/** Opaque bridge handle */
typedef struct physics_snn_bridge_struct physics_snn_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_snn_default_config(physics_snn_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create physics-SNN bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT physics_snn_bridge_t* physics_snn_bridge_create(
    const physics_snn_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void physics_snn_bridge_destroy(physics_snn_bridge_t* bridge);

//=============================================================================
// Spike Input API (HH → SNN)
//=============================================================================

/**
 * @brief Register a spike from HH neuron
 *
 * WHAT: Records spike event for SNN processing
 * WHY:  Enables precise spike timing for STDP
 * HOW:  Stores spike in circular buffer with metadata
 *
 * @param bridge Bridge handle
 * @param spike Spike event data
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_snn_register_spike(
    physics_snn_bridge_t* bridge,
    const physics_snn_spike_t* spike
);

/**
 * @brief Encode HH population activity for SNN input
 *
 * WHAT: Converts HH spike trains to SNN input currents
 * WHY:  Bridges biophysics to network computation
 * HOW:  Uses configured encoding method (precise, rate, burst, phase)
 *
 * @param bridge Bridge handle
 * @param output_currents Output array for SNN input currents
 * @param num_outputs Size of output array
 * @param window_ms Time window for encoding
 * @return Number of active inputs, -1 on error
 */
NIMCP_EXPORT int physics_snn_encode_spikes(
    physics_snn_bridge_t* bridge,
    float* output_currents,
    uint32_t num_outputs,
    float window_ms
);

//=============================================================================
// STDP API
//=============================================================================

/**
 * @brief Compute STDP weight change for spike pair
 *
 * WHAT: Calculates weight change based on spike timing
 * WHY:  Implements biophysically-grounded plasticity
 * HOW:  Applies STDP rule with temperature/ATP modulation
 *
 * @param bridge Bridge handle
 * @param pre_spike Presynaptic spike
 * @param post_spike Postsynaptic spike
 * @param event Output STDP event (optional)
 * @return Computed weight change
 */
NIMCP_EXPORT float physics_snn_compute_stdp(
    physics_snn_bridge_t* bridge,
    const physics_snn_spike_t* pre_spike,
    const physics_snn_spike_t* post_spike,
    physics_snn_stdp_event_t* event
);

/**
 * @brief Process all pending STDP updates
 *
 * WHAT: Applies STDP to all recent spike pairs
 * WHY:  Batch processing of plasticity updates
 * HOW:  Iterates through spike buffer, computes pairwise STDP
 *
 * @param bridge Bridge handle
 * @param events Output array for STDP events (optional)
 * @param max_events Maximum events to return
 * @return Number of STDP updates applied
 */
NIMCP_EXPORT int physics_snn_process_stdp(
    physics_snn_bridge_t* bridge,
    physics_snn_stdp_event_t* events,
    uint32_t max_events
);

/**
 * @brief Set temperature for STDP window scaling
 *
 * @param bridge Bridge handle
 * @param temperature_k Temperature in Kelvin
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_snn_set_temperature(
    physics_snn_bridge_t* bridge,
    float temperature_k
);

/**
 * @brief Set ATP level for LTP gating
 *
 * @param bridge Bridge handle
 * @param atp_level ATP level (0.0-1.0)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_snn_set_atp(
    physics_snn_bridge_t* bridge,
    float atp_level
);

//=============================================================================
// Eligibility Trace API
//=============================================================================

/**
 * @brief Update eligibility trace for synapse
 *
 * WHAT: Accumulates eligibility based on spike activity
 * WHY:  Enables three-factor learning rules
 * HOW:  Exponential decay with spike-driven increments
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse identifier
 * @param increment Trace increment
 * @return Current trace value
 */
NIMCP_EXPORT float physics_snn_update_eligibility(
    physics_snn_bridge_t* bridge,
    uint32_t synapse_id,
    float increment
);

/**
 * @brief Convert eligibility traces to weight changes
 *
 * WHAT: Applies accumulated eligibility as weight updates
 * WHY:  Three-factor rule: eligibility × modulator → weight
 * HOW:  Gated by ephaptic coherence (if enabled)
 *
 * @param bridge Bridge handle
 * @param coherence Current ephaptic coherence (0.0-1.0)
 * @return Number of traces converted
 */
NIMCP_EXPORT int physics_snn_convert_eligibility(
    physics_snn_bridge_t* bridge,
    float coherence
);

/**
 * @brief Decay all eligibility traces
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_snn_decay_eligibility(
    physics_snn_bridge_t* bridge,
    float dt_ms
);

//=============================================================================
// Feedback API (SNN → HH)
//=============================================================================

/**
 * @brief Receive modulation signal from SNN
 *
 * WHAT: Receives network-level feedback for HH modulation
 * WHY:  Enables top-down network effects on biophysics
 * HOW:  Stores modulation for application during HH update
 *
 * @param bridge Bridge handle
 * @param modulation Modulation signal
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_snn_receive_modulation(
    physics_snn_bridge_t* bridge,
    const physics_snn_modulation_t* modulation
);

/**
 * @brief Get current modulation for HH neurons
 *
 * WHAT: Retrieves accumulated modulation signal
 * WHY:  For application to HH parameters
 * HOW:  Returns smoothed/accumulated modulation state
 *
 * @param bridge Bridge handle
 * @param modulation Output modulation
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_snn_get_modulation(
    physics_snn_bridge_t* bridge,
    physics_snn_modulation_t* modulation
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Decay traces, process pending updates, collect stats
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_snn_update(
    physics_snn_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_snn_reset(physics_snn_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_snn_get_stats(
    const physics_snn_bridge_t* bridge,
    physics_snn_stats_t* stats
);

/**
 * @brief Get current STDP window widths (temperature-adjusted)
 *
 * @param bridge Bridge handle
 * @param ltp_window_ms Output LTP window
 * @param ltd_window_ms Output LTD window
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int physics_snn_get_stdp_windows(
    const physics_snn_bridge_t* bridge,
    float* ltp_window_ms,
    float* ltd_window_ms
);

/**
 * @brief Check if learning is gated (ATP below threshold)
 *
 * @param bridge Bridge handle
 * @return true if learning is gated (ATP low)
 */
NIMCP_EXPORT bool physics_snn_is_learning_gated(
    const physics_snn_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PHYSICS_SNN_BRIDGE_H */
