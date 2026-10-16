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
// nimcp_ephaptic_snn_bridge.h - Ephaptic to SNN Spike Timing Bridge
//=============================================================================
/**
 * @file nimcp_ephaptic_snn_bridge.h
 * @brief Bidirectional bridge between Ephaptic coupling and Spiking Neural Networks
 *
 * WHAT: Connects ephaptic field dynamics with SNN spike timing and synchronization,
 *       enabling field-mediated coordination of spike patterns.
 *
 * WHY:  Bridges the gap between:
 *       - Local field effects (ephaptic electric fields)
 *       - Spike timing in SNNs (precise temporal patterns)
 *       - Phase-locked neural assemblies
 *       The ephaptic field can bias spike timing, synchronize populations,
 *       and create phase-locked firing patterns without synaptic connections.
 *
 * HOW:  Two-way integration:
 *       1. Ephaptic -> SNN: Field polarization shifts spike thresholds
 *       2. Ephaptic -> SNN: Phase coherence gates spike synchronization
 *       3. SNN -> Ephaptic: Spike activity contributes to LFP/field
 *       4. Kuramoto coupling modulates SNN oscillatory dynamics
 *
 * BIOLOGICAL BASIS:
 * ```
 * EPHAPTIC COUPLING                      SNN SPIKE TIMING
 * ─────────────────────────────────────────────────────────────────────
 * Electric field strength (V/m)       -> Spike threshold modulation
 * Phase coherence (order parameter)   -> Population synchrony gating
 * Field polarization (mV)             -> Membrane potential bias
 * LFP phase                           -> Phase-locked spike timing
 * Kuramoto coupling K                 -> Oscillation frequency locking
 * Spiking activity                    <- LFP contribution (feedback)
 * ```
 *
 * KEY MECHANISMS:
 * - Field-induced threshold shift: E-field alters effective spike threshold
 * - Phase-locking: Neurons preferentially fire at specific LFP phases
 * - Coherence gating: High coherence enables rapid population sync
 * - Spatial decay: Field effects decrease with distance (r^-2 or exp decay)
 *
 * REFERENCES:
 * - Anastassiou et al. (2011) "Ephaptic coupling of cortical neurons"
 * - Schaefer et al. (2006) "Synchronization clusters in spiking networks"
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_EPHAPTIC_SNN_BRIDGE_H
#define NIMCP_EPHAPTIC_SNN_BRIDGE_H

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
#define EPHAPTIC_SNN_MODULE_NAME        "ephaptic_snn_bridge"

/** Maximum tracked neurons in bridge */
#define EPHAPTIC_SNN_MAX_NEURONS        4096

/** Maximum spike events per update cycle */
#define EPHAPTIC_SNN_MAX_SPIKES         1024

/** Default threshold modulation gain (mV per V/m) */
#define EPHAPTIC_SNN_DEFAULT_THRESHOLD_GAIN     0.5f

/** Default phase-lock window (radians) */
#define EPHAPTIC_SNN_DEFAULT_PHASE_WINDOW       0.52f  /* ~30 degrees */

/** Default coherence threshold for sync gating */
#define EPHAPTIC_SNN_DEFAULT_COHERENCE_GATE     0.6f

/** Default field contribution per spike (V/m) */
#define EPHAPTIC_SNN_DEFAULT_SPIKE_CONTRIBUTION 0.001f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Spike timing modulation mode
 */
typedef enum {
    EPHAPTIC_SNN_MOD_THRESHOLD = 0,   /**< Modulate spike threshold */
    EPHAPTIC_SNN_MOD_DELAY,           /**< Modulate spike delay */
    EPHAPTIC_SNN_MOD_PHASE_LOCK,      /**< Phase-lock to LFP */
    EPHAPTIC_SNN_MOD_COMBINED         /**< Combined modulation */
} ephaptic_snn_mod_mode_t;

/**
 * @brief Synchronization mode for population effects
 */
typedef enum {
    EPHAPTIC_SNN_SYNC_NONE = 0,       /**< No synchronization */
    EPHAPTIC_SNN_SYNC_KURAMOTO,       /**< Kuramoto-style phase coupling */
    EPHAPTIC_SNN_SYNC_COHERENCE_GATE, /**< Coherence-gated burst sync */
    EPHAPTIC_SNN_SYNC_PHASE_RESET     /**< Phase reset on coherence events */
} ephaptic_snn_sync_mode_t;

/**
 * @brief Field feedback mode from SNN to ephaptic
 */
typedef enum {
    EPHAPTIC_SNN_FEEDBACK_NONE = 0,   /**< No feedback to ephaptic */
    EPHAPTIC_SNN_FEEDBACK_LFP,        /**< Spikes contribute to LFP */
    EPHAPTIC_SNN_FEEDBACK_FIELD,      /**< Spikes contribute to E-field */
    EPHAPTIC_SNN_FEEDBACK_FULL        /**< Full bidirectional coupling */
} ephaptic_snn_feedback_mode_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for ephaptic-SNN bridge
 */
typedef struct {
    /** Modulation parameters */
    ephaptic_snn_mod_mode_t mod_mode;     /**< Spike timing modulation mode */
    float threshold_gain;                  /**< Threshold shift per unit field (mV/(V/m)) */
    float delay_gain;                      /**< Delay shift per unit field (ms/(V/m)) */
    float phase_lock_window;               /**< Phase window for phase-locking (radians) */

    /** Synchronization parameters */
    ephaptic_snn_sync_mode_t sync_mode;   /**< Population synchronization mode */
    float coherence_gate_threshold;        /**< Coherence threshold for sync gating */
    float kuramoto_coupling;               /**< Kuramoto coupling strength K */
    float sync_burst_threshold;            /**< Threshold for synchronized burst detection */

    /** Feedback parameters */
    ephaptic_snn_feedback_mode_t feedback_mode; /**< Feedback mode */
    float spike_field_contribution;        /**< E-field contribution per spike */
    float spike_lfp_contribution;          /**< LFP contribution per spike */

    /** Spatial parameters */
    float field_decay_constant;            /**< Spatial decay constant (mm^-1) */
    bool enable_distance_weighting;        /**< Weight effects by distance */

    /** Update parameters */
    float update_interval_ms;              /**< Bridge update interval */
    bool enable_adaptive_coupling;         /**< Adapt coupling based on activity */
} ephaptic_snn_config_t;

/**
 * @brief Spike event from SNN for ephaptic processing
 */
typedef struct {
    uint32_t neuron_id;                    /**< Neuron identifier */
    float spike_time_ms;                   /**< Precise spike time */
    float position[3];                     /**< Neuron position (mm) */
    float membrane_voltage;                /**< Voltage at spike */
    float phase;                           /**< Phase at spike (radians) */
} ephaptic_snn_spike_t;

/**
 * @brief Threshold modulation result for a neuron
 */
typedef struct {
    uint32_t neuron_id;                    /**< Neuron identifier */
    float threshold_shift_mv;              /**< Threshold shift (mV) */
    float delay_shift_ms;                  /**< Delay shift (ms) */
    float phase_bias;                      /**< Phase bias (radians) */
    float field_at_neuron;                 /**< E-field magnitude at neuron */
    bool is_phase_locked;                  /**< Whether neuron is phase-locked */
} ephaptic_snn_modulation_t;

/**
 * @brief Synchronization event detected
 */
typedef struct {
    float event_time_ms;                   /**< Event timestamp */
    float coherence;                       /**< Coherence at event */
    float dominant_phase;                  /**< Dominant phase (radians) */
    uint32_t synchronized_count;           /**< Number of synchronized neurons */
    float burst_amplitude;                 /**< Burst amplitude (spikes/ms) */
    bool is_population_burst;              /**< Population burst detected */
} ephaptic_snn_sync_event_t;

/**
 * @brief Field feedback to ephaptic system
 */
typedef struct {
    float lfp_contribution;                /**< LFP contribution from spikes (mV) */
    float field_contribution[3];           /**< E-field contribution (V/m) */
    float spike_density;                   /**< Current spike density (spikes/ms) */
    float mean_spike_phase;                /**< Mean phase of recent spikes */
    uint32_t spike_count;                  /**< Spikes in window */
} ephaptic_snn_feedback_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t spikes_processed;             /**< Total spikes processed */
    uint64_t modulations_applied;          /**< Threshold modulations applied */
    uint64_t sync_events_detected;         /**< Synchronization events */
    uint64_t phase_lock_events;            /**< Phase-lock events */
    uint64_t feedback_updates;             /**< Feedback to ephaptic */
    float avg_threshold_shift;             /**< Average threshold shift (mV) */
    float avg_coherence;                   /**< Average coherence observed */
    float peak_synchrony;                  /**< Peak synchronization level */
    float last_update_ms;                  /**< Last update timestamp */
} ephaptic_snn_stats_t;

/** Opaque bridge handle */
typedef struct ephaptic_snn_bridge_struct ephaptic_snn_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible default configuration values
 * WHY:  Provide biologically plausible starting parameters
 * HOW:  Based on experimental literature values
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_snn_default_config(ephaptic_snn_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create ephaptic-SNN bridge
 *
 * WHAT: Allocates and initializes bridge instance
 * WHY:  Enable ephaptic-SNN coordination
 * HOW:  Allocates internal buffers, sets up modulation tables
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT ephaptic_snn_bridge_t* ephaptic_snn_bridge_create(
    const ephaptic_snn_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
NIMCP_EXPORT void ephaptic_snn_bridge_destroy(ephaptic_snn_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_snn_bridge_reset(ephaptic_snn_bridge_t* bridge);

//=============================================================================
// Ephaptic -> SNN Modulation API
//=============================================================================

/**
 * @brief Compute spike threshold modulation for neuron
 *
 * WHAT: Calculates field-induced threshold shift
 * WHY:  Ephaptic fields bias spike timing via threshold
 * HOW:  threshold_shift = field_strength * susceptibility * gain
 *
 * BIOLOGICAL: Extracellular fields of 1-5 mV/mm can shift spike
 * thresholds by several millivolts, advancing or delaying spikes.
 *
 * @param bridge         Bridge handle
 * @param neuron_id      Neuron identifier
 * @param position       Neuron position (mm)
 * @param field_strength Current E-field (V/m)
 * @param lfp_phase      Current LFP phase (radians)
 * @param modulation     Output modulation result
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_snn_compute_modulation(
    ephaptic_snn_bridge_t* bridge,
    uint32_t neuron_id,
    const float position[3],
    const float field_strength[3],
    float lfp_phase,
    ephaptic_snn_modulation_t* modulation
);

/**
 * @brief Apply phase-locking to spike timing
 *
 * WHAT: Adjusts spike time to align with LFP phase
 * WHY:  Phase-locked firing is a key ephaptic mechanism
 * HOW:  Shifts spike time to nearest preferred phase window
 *
 * @param bridge           Bridge handle
 * @param intended_time_ms Intended spike time
 * @param neuron_phase     Current neuron phase
 * @param lfp_phase        Current LFP phase
 * @param adjusted_time_ms Output: adjusted spike time
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_snn_apply_phase_lock(
    ephaptic_snn_bridge_t* bridge,
    float intended_time_ms,
    float neuron_phase,
    float lfp_phase,
    float* adjusted_time_ms
);

/**
 * @brief Check if coherence gates synchronization
 *
 * WHAT: Determines if population is coherent enough for sync effects
 * WHY:  High coherence enables rapid population coordination
 * HOW:  Compares current coherence to gate threshold
 *
 * @param bridge    Bridge handle
 * @param coherence Current phase coherence [0,1]
 * @return true if synchronization is gated (enabled)
 */
NIMCP_EXPORT bool ephaptic_snn_coherence_gates_sync(
    const ephaptic_snn_bridge_t* bridge,
    float coherence
);

//=============================================================================
// SNN -> Ephaptic Feedback API
//=============================================================================

/**
 * @brief Register spike for ephaptic feedback
 *
 * WHAT: Records spike for field/LFP contribution
 * WHY:  SNN activity contributes to ephaptic fields
 * HOW:  Stores spike in buffer for integration
 *
 * @param bridge Bridge handle
 * @param spike  Spike event data
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_snn_register_spike(
    ephaptic_snn_bridge_t* bridge,
    const ephaptic_snn_spike_t* spike
);

/**
 * @brief Compute feedback from recent spikes
 *
 * WHAT: Aggregates spike contributions to field/LFP
 * WHY:  Provides ephaptic system with spike-driven input
 * HOW:  Sums contributions with distance weighting
 *
 * @param bridge       Bridge handle
 * @param window_ms    Time window for spike integration
 * @param feedback     Output feedback structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_snn_compute_feedback(
    ephaptic_snn_bridge_t* bridge,
    float window_ms,
    ephaptic_snn_feedback_t* feedback
);

/**
 * @brief Clear spike buffer
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_snn_clear_spikes(ephaptic_snn_bridge_t* bridge);

//=============================================================================
// Synchronization Detection API
//=============================================================================

/**
 * @brief Detect synchronization events
 *
 * WHAT: Identifies population synchronization events
 * WHY:  Sync events indicate coherent ephaptic effects
 * HOW:  Monitors coherence, phase clustering, burst patterns
 *
 * @param bridge    Bridge handle
 * @param coherence Current phase coherence
 * @param event     Output sync event (if detected)
 * @return 1 if event detected, 0 if not, -1 on error
 */
NIMCP_EXPORT int ephaptic_snn_detect_sync_event(
    ephaptic_snn_bridge_t* bridge,
    float coherence,
    ephaptic_snn_sync_event_t* event
);

/**
 * @brief Get current synchronization state
 *
 * @param bridge            Bridge handle
 * @param synchronized_pct  Output: percentage synchronized
 * @param mean_phase        Output: mean population phase
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_snn_get_sync_state(
    const ephaptic_snn_bridge_t* bridge,
    float* synchronized_pct,
    float* mean_phase
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Process pending events, update statistics
 * HOW:  Called during simulation step
 *
 * @param bridge     Bridge handle
 * @param dt_ms      Time step in milliseconds
 * @param coherence  Current ephaptic coherence
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_snn_update(
    ephaptic_snn_bridge_t* bridge,
    float dt_ms,
    float coherence
);

/**
 * @brief Set Kuramoto coupling parameter
 *
 * @param bridge  Bridge handle
 * @param k       Kuramoto coupling strength
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_snn_set_kuramoto_coupling(
    ephaptic_snn_bridge_t* bridge,
    float k
);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats  Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_snn_get_stats(
    const ephaptic_snn_bridge_t* bridge,
    ephaptic_snn_stats_t* stats
);

/**
 * @brief Get current modulation strength
 *
 * @param bridge    Bridge handle
 * @param strength  Output: modulation strength [0,1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_snn_get_modulation_strength(
    const ephaptic_snn_bridge_t* bridge,
    float* strength
);

/**
 * @brief Check if bridge is active
 *
 * @param bridge Bridge handle
 * @return true if bridge is active and processing
 */
NIMCP_EXPORT bool ephaptic_snn_is_active(const ephaptic_snn_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EPHAPTIC_SNN_BRIDGE_H */