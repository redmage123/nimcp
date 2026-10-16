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
// nimcp_hh_snn_bridge.h - Hodgkin-Huxley to SNN Spike Train Generation Bridge
//=============================================================================
/**
 * @file nimcp_hh_snn_bridge.h
 * @brief Bridge between HH biophysics and Spiking Neural Network systems
 *
 * WHAT: Converts Hodgkin-Huxley neuron dynamics into SNN-compatible spike trains
 *       with precise timing, enabling biophysically-grounded network computation.
 *
 * WHY:  HH models provide biophysical realism (ion channels, action potentials)
 *       but SNNs need discrete spike events for network-level processing.
 *       This bridge translates continuous HH dynamics into spike trains while
 *       preserving timing precision critical for neural coding.
 *
 * HOW:  - Monitors HH membrane voltage for threshold crossings
 *       - Extracts spike timing with sub-millisecond precision
 *       - Encodes spike metadata (amplitude, width, rate)
 *       - Supports multiple encoding schemes (precise, rate, burst, phase)
 *       - Provides bidirectional feedback from SNN to HH parameters
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * HH SPIKE PROPERTIES TO SNN:
 * ---------------------------
 * 1. Action Potential Timing:
 *    - HH threshold crossing (V > 0 mV) defines spike time
 *    - Rising phase slope indicates spike "sharpness"
 *    - Temperature (Q10) affects timing precision
 *
 * 2. Spike Shape Features:
 *    - Peak amplitude: Health of Na+ channels
 *    - Spike width: K+ channel kinetics
 *    - Afterhyperpolarization: Refractory dynamics
 *
 * 3. Firing Rate Encoding:
 *    - Inter-spike intervals (ISI) → instantaneous rate
 *    - ISI variability (CV) → coding precision
 *    - Burst detection → salience signaling
 *
 * SNN FEEDBACK TO HH:
 * -------------------
 * 1. Network Activity Modulation:
 *    - Population firing rate → HH conductance scaling
 *    - Network synchrony → ion channel modulation
 *    - SNN output → synaptic input currents
 *
 * ENCODING SCHEMES:
 * -----------------
 * - PRECISE: Exact spike times (< 0.1 ms resolution)
 * - RATE: Firing rate in Hz (temporal averaging)
 * - BURST: Burst pattern detection (ISI < burst threshold)
 * - PHASE: Spike phase relative to population oscillation
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_HH_SNN_BRIDGE_H
#define NIMCP_HH_SNN_BRIDGE_H

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
#define HH_SNN_MODULE_NAME              "hh_snn_bridge"

/** Maximum tracked HH neurons */
#define HH_SNN_MAX_NEURONS              1024

/** Maximum spikes per update cycle */
#define HH_SNN_MAX_SPIKES_PER_CYCLE     4096

/** Default spike detection threshold (mV) */
#define HH_SNN_DEFAULT_SPIKE_THRESH     0.0f

/** Default refractory period (ms) */
#define HH_SNN_DEFAULT_REFRACTORY_MS    2.0f

/** Burst detection ISI threshold (ms) */
#define HH_SNN_BURST_ISI_THRESHOLD      10.0f

/** Minimum burst spikes */
#define HH_SNN_MIN_BURST_SPIKES         3

/** Phase encoding bins */
#define HH_SNN_PHASE_BINS               32

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Spike encoding method for HH to SNN
 */
typedef enum {
    HH_SNN_ENCODE_PRECISE = 0,    /**< Precise spike times (< 0.1 ms) */
    HH_SNN_ENCODE_RATE,           /**< Rate coding (Hz) */
    HH_SNN_ENCODE_BURST,          /**< Burst pattern detection */
    HH_SNN_ENCODE_PHASE           /**< Phase relative to LFP/oscillation */
} hh_snn_encoding_t;

/**
 * @brief Spike shape classification
 */
typedef enum {
    HH_SNN_SPIKE_NORMAL = 0,      /**< Normal action potential */
    HH_SNN_SPIKE_BROAD,           /**< Broadened spike (K+ channel issue) */
    HH_SNN_SPIKE_NARROW,          /**< Narrow spike (fast-spiking interneuron) */
    HH_SNN_SPIKE_ATTENUATED,      /**< Reduced amplitude (Na+ fatigue) */
    HH_SNN_SPIKE_DOUBLET          /**< Doublet/burst initiation */
} hh_snn_spike_class_t;

/**
 * @brief Feedback modulation target
 */
typedef enum {
    HH_SNN_FEEDBACK_CONDUCTANCE = 0, /**< Modulate g_Na, g_K */
    HH_SNN_FEEDBACK_THRESHOLD,       /**< Modulate spike threshold */
    HH_SNN_FEEDBACK_CURRENT,         /**< Inject synaptic current */
    HH_SNN_FEEDBACK_TEMPERATURE      /**< Adjust temperature factor */
} hh_snn_feedback_target_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Spike event extracted from HH neuron
 */
typedef struct {
    uint32_t neuron_id;           /**< Source HH neuron ID */
    float spike_time_ms;          /**< Precise spike timestamp */
    float peak_voltage;           /**< Peak membrane voltage (mV) */
    float spike_width_ms;         /**< Spike width at half-max */
    float rise_slope;             /**< dV/dt at threshold (mV/ms) */
    float isi_ms;                 /**< Inter-spike interval from last */
    float instantaneous_rate_hz;  /**< 1000/ISI rate estimate */
    hh_snn_spike_class_t classification; /**< Spike shape class */
    float temperature;            /**< Temperature at spike time */
    float phi_factor;             /**< Q10 factor at spike time */
    bool is_burst_spike;          /**< Part of burst pattern */
    uint32_t burst_index;         /**< Position in burst (0-based) */
} hh_snn_spike_event_t;

/**
 * @brief Burst detection state
 */
typedef struct {
    bool in_burst;                /**< Currently in burst */
    uint32_t burst_count;         /**< Spikes in current burst */
    float burst_start_ms;         /**< Burst onset time */
    float last_isi_ms;            /**< Last inter-spike interval */
    float mean_burst_isi_ms;      /**< Mean ISI within burst */
    float burst_rate_hz;          /**< Instantaneous burst rate */
} hh_snn_burst_state_t;

/**
 * @brief Rate coding state
 */
typedef struct {
    float window_ms;              /**< Averaging window */
    uint32_t spike_count;         /**< Spikes in window */
    float mean_rate_hz;           /**< Mean firing rate */
    float rate_variance;          /**< Rate variance (CV^2) */
    float last_update_ms;         /**< Last rate update time */
} hh_snn_rate_state_t;

/**
 * @brief Phase coding state
 */
typedef struct {
    float reference_period_ms;    /**< Reference oscillation period */
    float current_phase;          /**< Current phase [0, 2*pi] */
    float* phase_histogram;       /**< Phase distribution histogram */
    float preferred_phase;        /**< Preferred firing phase */
    float phase_locking_value;    /**< Phase coherence [0, 1] */
} hh_snn_phase_state_t;

/**
 * @brief SNN feedback signal to HH
 */
typedef struct {
    hh_snn_feedback_target_t target; /**< Modulation target */
    float g_na_scale;             /**< Na+ conductance scaling [0.5, 2.0] */
    float g_k_scale;              /**< K+ conductance scaling [0.5, 2.0] */
    float threshold_shift_mv;     /**< Threshold adjustment (mV) */
    float current_injection;      /**< Synaptic current (uA/cm^2) */
    float temperature_delta;      /**< Temperature adjustment (C) */
} hh_snn_feedback_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Spike detection */
    float spike_threshold_mv;     /**< Voltage threshold for spike detection */
    float refractory_ms;          /**< Refractory period */
    float min_spike_amplitude_mv; /**< Minimum spike amplitude */

    /* Encoding parameters */
    hh_snn_encoding_t encoding;   /**< Encoding method */
    float rate_window_ms;         /**< Window for rate coding */
    float burst_isi_threshold_ms; /**< ISI threshold for burst detection */
    uint32_t min_burst_spikes;    /**< Minimum spikes for burst */
    float phase_period_ms;        /**< Reference period for phase coding */

    /* Feedback parameters */
    bool enable_feedback;         /**< Enable SNN to HH feedback */
    hh_snn_feedback_target_t feedback_target; /**< Feedback target */
    float feedback_strength;      /**< Feedback scaling [0, 1] */

    /* Output options */
    bool output_spike_shape;      /**< Include spike shape features */
    bool output_burst_info;       /**< Include burst detection info */
    bool output_rate_estimate;    /**< Include rate estimates */
    bool output_phase_info;       /**< Include phase coding info */

    /* Update interval */
    float update_interval_ms;     /**< Bridge update interval */
} hh_snn_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Spike counts */
    uint64_t total_spikes_detected;    /**< Total spikes detected */
    uint64_t spikes_encoded;           /**< Spikes encoded for SNN */
    uint64_t spikes_filtered;          /**< Spikes filtered (below threshold) */

    /* Burst statistics */
    uint64_t bursts_detected;          /**< Total bursts detected */
    float mean_burst_duration_ms;      /**< Mean burst duration */
    float mean_spikes_per_burst;       /**< Mean spikes per burst */

    /* Rate statistics */
    float population_mean_rate_hz;     /**< Population mean firing rate */
    float population_rate_variance;    /**< Population rate variance */

    /* Timing */
    float mean_spike_width_ms;         /**< Mean spike width */
    float mean_isi_ms;                 /**< Mean inter-spike interval */
    float spike_timing_precision_ms;   /**< Timing precision estimate */

    /* Feedback */
    uint64_t feedback_events;          /**< SNN feedback events processed */
    float avg_feedback_strength;       /**< Average feedback strength applied */

    /* Performance */
    float last_update_ms;              /**< Last update timestamp */
    float processing_latency_us;       /**< Processing latency */
} hh_snn_stats_t;

/** Opaque bridge handle */
typedef struct hh_snn_bridge_struct hh_snn_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Initialize configuration with sensible defaults
 * WHY:  Easy bridge creation with biologically-motivated parameters
 * HOW:  Set spike threshold, encoding method, feedback options
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_snn_default_config(hh_snn_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create HH-SNN bridge
 *
 * WHAT: Initialize bridge for HH to SNN spike conversion
 * WHY:  Enable biophysically-grounded SNN computation
 * HOW:  Allocate buffers, initialize state tracking
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT hh_snn_bridge_t* hh_snn_bridge_create(
    const hh_snn_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
NIMCP_EXPORT void hh_snn_bridge_destroy(hh_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * WHAT: Clear spike history, reset encoding state
 * WHY:  Fresh start for new simulation
 * HOW:  Zero counters, clear buffers
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_snn_bridge_reset(hh_snn_bridge_t* bridge);

//=============================================================================
// Spike Detection API (HH to SNN)
//=============================================================================

/**
 * @brief Process HH neuron and detect spikes
 *
 * WHAT: Monitor HH neuron voltage for spike events
 * WHY:  Extract spike timing for SNN processing
 * HOW:  Check threshold crossing, extract spike features
 *
 * @param bridge Bridge handle
 * @param neuron_id HH neuron identifier
 * @param voltage Current membrane voltage (mV)
 * @param prev_voltage Previous voltage (mV)
 * @param time_ms Current simulation time
 * @param temperature Current temperature (C)
 * @param spike_out Output spike event (if detected)
 * @return 1 if spike detected, 0 if no spike, -1 on error
 */
NIMCP_EXPORT int hh_snn_detect_spike(
    hh_snn_bridge_t* bridge,
    uint32_t neuron_id,
    float voltage,
    float prev_voltage,
    float time_ms,
    float temperature,
    hh_snn_spike_event_t* spike_out
);

/**
 * @brief Process HH population for spike trains
 *
 * WHAT: Batch process entire HH population
 * WHY:  Efficient population-level spike detection
 * HOW:  Iterate neurons, collect spikes, update statistics
 *
 * @param bridge Bridge handle
 * @param voltages Array of current voltages (per neuron)
 * @param prev_voltages Array of previous voltages
 * @param num_neurons Number of neurons
 * @param time_ms Current simulation time
 * @param temperature Population temperature (C)
 * @param spikes_out Output spike array (caller allocates)
 * @param max_spikes Maximum spikes to return
 * @return Number of spikes detected, -1 on error
 */
NIMCP_EXPORT int hh_snn_process_population(
    hh_snn_bridge_t* bridge,
    const float* voltages,
    const float* prev_voltages,
    uint32_t num_neurons,
    float time_ms,
    float temperature,
    hh_snn_spike_event_t* spikes_out,
    uint32_t max_spikes
);

//=============================================================================
// Encoding API
//=============================================================================

/**
 * @brief Encode spikes for SNN input
 *
 * WHAT: Convert spike events to SNN-compatible format
 * WHY:  Different SNNs need different spike representations
 * HOW:  Apply configured encoding (precise, rate, burst, phase)
 *
 * @param bridge Bridge handle
 * @param spikes Input spike events
 * @param num_spikes Number of spikes
 * @param snn_input Output SNN input array
 * @param num_outputs Size of output array
 * @param window_ms Time window for encoding
 * @return Number of encoded outputs, -1 on error
 */
NIMCP_EXPORT int hh_snn_encode_spikes(
    hh_snn_bridge_t* bridge,
    const hh_snn_spike_event_t* spikes,
    uint32_t num_spikes,
    float* snn_input,
    uint32_t num_outputs,
    float window_ms
);

/**
 * @brief Get rate encoding for neuron
 *
 * WHAT: Compute firing rate for specified neuron
 * WHY:  Rate coding for SNN input
 * HOW:  Average spikes over window
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron to query
 * @param rate_hz Output firing rate (Hz)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_snn_get_rate(
    const hh_snn_bridge_t* bridge,
    uint32_t neuron_id,
    float* rate_hz
);

/**
 * @brief Get burst state for neuron
 *
 * WHAT: Query burst detection state
 * WHY:  Burst coding carries salience information
 * HOW:  Return current burst tracking state
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron to query
 * @param burst_state Output burst state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_snn_get_burst_state(
    const hh_snn_bridge_t* bridge,
    uint32_t neuron_id,
    hh_snn_burst_state_t* burst_state
);

/**
 * @brief Get phase coding state for neuron
 *
 * WHAT: Query phase coding information
 * WHY:  Phase coding for temporal coordination
 * HOW:  Return phase histogram and preferred phase
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron to query
 * @param phase_state Output phase state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_snn_get_phase_state(
    const hh_snn_bridge_t* bridge,
    uint32_t neuron_id,
    hh_snn_phase_state_t* phase_state
);

//=============================================================================
// Feedback API (SNN to HH)
//=============================================================================

/**
 * @brief Apply feedback from SNN to HH parameters
 *
 * WHAT: Modulate HH parameters based on SNN activity
 * WHY:  Network-level effects on biophysics
 * HOW:  Apply scaling to conductances, threshold, or current
 *
 * @param bridge Bridge handle
 * @param feedback Feedback signal to apply
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_snn_apply_feedback(
    hh_snn_bridge_t* bridge,
    const hh_snn_feedback_t* feedback
);

/**
 * @brief Get accumulated feedback for HH neuron
 *
 * WHAT: Retrieve feedback signal for applying to HH
 * WHY:  Query accumulated feedback for neuron update
 * HOW:  Return stored feedback parameters
 *
 * @param bridge Bridge handle
 * @param neuron_id Target neuron
 * @param feedback Output feedback signal
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_snn_get_feedback(
    const hh_snn_bridge_t* bridge,
    uint32_t neuron_id,
    hh_snn_feedback_t* feedback
);

/**
 * @brief Set reference oscillation for phase coding
 *
 * WHAT: Update reference signal for phase encoding
 * WHY:  Phase coding needs reference (LFP, theta, etc.)
 * HOW:  Store period and update phase tracker
 *
 * @param bridge Bridge handle
 * @param period_ms Reference oscillation period
 * @param current_phase Current phase [0, 2*pi]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_snn_set_reference_oscillation(
    hh_snn_bridge_t* bridge,
    float period_ms,
    float current_phase
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic bridge update for housekeeping
 * WHY:  Decay rate estimates, update statistics
 * HOW:  Process pending events, update encodings
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_snn_bridge_update(
    hh_snn_bridge_t* bridge,
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
NIMCP_EXPORT int hh_snn_get_stats(
    const hh_snn_bridge_t* bridge,
    hh_snn_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_snn_reset_stats(hh_snn_bridge_t* bridge);

/**
 * @brief Get population synchrony measure
 *
 * WHAT: Compute population-level synchrony
 * WHY:  Assess coherence of HH population for SNN
 * HOW:  Calculate spike timing correlation
 *
 * @param bridge Bridge handle
 * @param window_ms Time window for analysis
 * @param synchrony Output synchrony [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_snn_get_population_synchrony(
    const hh_snn_bridge_t* bridge,
    float window_ms,
    float* synchrony
);

/**
 * @brief Print bridge summary
 *
 * @param bridge Bridge handle (NULL safe)
 */
NIMCP_EXPORT void hh_snn_print_summary(const hh_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HH_SNN_BRIDGE_H */