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
// nimcp_hh_thalamic_bridge.h - Hodgkin-Huxley to Thalamic Attention Bridge
//=============================================================================
/**
 * @file nimcp_hh_thalamic_bridge.h
 * @brief Bridge between HH biophysics and thalamic attention gating
 *
 * WHAT: Bidirectional integration between Hodgkin-Huxley neuron dynamics and
 *       thalamic routing/attention gating, enabling biophysically-grounded
 *       attention-based signal routing.
 *
 * WHY:  The thalamus gates information flow based on attention and salience.
 *       HH neurons provide biophysical signals that inform attention:
 *       - Action potentials drive attention capture
 *       - Ion channel states encode signal reliability
 *       - Temperature affects processing speed
 *       - Population synchrony indicates salience
 *
 * HOW:  - Maps HH spike events to thalamic routing signals
 *       - Translates attention weights to HH conductance modulation
 *       - Uses burst/tonic mode distinction for routing priority
 *       - Integrates with thalamic reticular nucleus (TRN) gating
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * THALAMIC GATING MECHANISMS:
 * ---------------------------
 * The thalamus acts as a "gateway to cortex" (Sherman & Guillery, 2001):
 *
 * 1. First-Order Relay:
 *    - Sensory input from periphery to cortex
 *    - HH spikes encode sensory information
 *    - Attention modulates relay gain
 *
 * 2. Higher-Order Relay:
 *    - Cortico-cortical communication via thalamus
 *    - HH population synchrony determines routing
 *    - Top-down attention selects pathways
 *
 * 3. Thalamic Reticular Nucleus (TRN):
 *    - GABAergic inhibition gates thalamic relay
 *    - Attention shifts inhibit competing pathways
 *    - HH inhibitory neurons model TRN function
 *
 * HH FIRING MODES AND ATTENTION:
 * ------------------------------
 * 1. Tonic Mode (Awake, Attentive):
 *    - V_rest near -65 mV (normal HH)
 *    - Linear relay of information
 *    - Faithful transmission of spike timing
 *    - High attention engagement
 *
 * 2. Burst Mode (Drowsy, Low Attention):
 *    - V_rest hyperpolarized (-80 mV)
 *    - Low-threshold Ca2+ spike triggers burst
 *    - Non-linear detection mode
 *    - Attention capture (wake-up call)
 *
 * HH TO THALAMIC MAPPING:
 * -----------------------
 * - Spike rate: Routing priority
 * - Spike timing: Phase-locked attention
 * - Burst pattern: Salience/alerting signal
 * - Ion channel state: Signal reliability
 * - Population synchrony: Coherent attention
 *
 * THALAMIC TO HH EFFECTS:
 * -----------------------
 * - Attention weight: Modulate g_Na (gain control)
 * - Priority level: Modulate threshold
 * - TRN inhibition: Inject inhibitory current
 * - Routing mode: Set burst vs tonic
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

#ifndef NIMCP_HH_THALAMIC_BRIDGE_H
#define NIMCP_HH_THALAMIC_BRIDGE_H

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
#define HH_THALAMIC_MODULE_NAME         "hh_thalamic_bridge"

/** Maximum routable destinations */
#define HH_THALAMIC_MAX_DESTINATIONS    64

/** Maximum attention sources */
#define HH_THALAMIC_MAX_SOURCES         256

/** Default attention weight */
#define HH_THALAMIC_DEFAULT_ATTENTION   0.5f

/** Burst detection threshold (ISI in ms) */
#define HH_THALAMIC_BURST_ISI_THRESH    15.0f

/** Minimum burst spikes for detection */
#define HH_THALAMIC_MIN_BURST_SPIKES    2

/** Tonic mode resting potential (mV) */
#define HH_THALAMIC_TONIC_V_REST        (-65.0f)

/** Burst mode resting potential (mV) */
#define HH_THALAMIC_BURST_V_REST        (-80.0f)

/** TRN inhibition strength scale */
#define HH_THALAMIC_TRN_INHIBITION      2.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Thalamic relay mode
 */
typedef enum {
    HH_THALAMIC_MODE_TONIC = 0,       /**< Tonic (awake, linear relay) */
    HH_THALAMIC_MODE_BURST,           /**< Burst (drowsy, detection) */
    HH_THALAMIC_MODE_TRANSITION       /**< Transitioning between modes */
} hh_thalamic_mode_t;

/**
 * @brief Signal routing priority
 */
typedef enum {
    HH_THALAMIC_PRIORITY_BACKGROUND = 0, /**< Background (low attention) */
    HH_THALAMIC_PRIORITY_NORMAL,         /**< Normal routing */
    HH_THALAMIC_PRIORITY_ELEVATED,       /**< Elevated (increased attention) */
    HH_THALAMIC_PRIORITY_HIGH,           /**< High (focused attention) */
    HH_THALAMIC_PRIORITY_CRITICAL        /**< Critical (salient, bypasses) */
} hh_thalamic_priority_t;

/**
 * @brief Attention modulation target
 */
typedef enum {
    HH_THALAMIC_MOD_GAIN = 0,         /**< Modulate relay gain (g_Na) */
    HH_THALAMIC_MOD_THRESHOLD,        /**< Modulate spike threshold */
    HH_THALAMIC_MOD_CURRENT,          /**< Inject modulatory current */
    HH_THALAMIC_MOD_MODE              /**< Switch burst/tonic mode */
} hh_thalamic_mod_target_t;

/**
 * @brief Attention source type
 */
typedef enum {
    HH_THALAMIC_ATTN_BOTTOM_UP = 0,   /**< Bottom-up (salience) */
    HH_THALAMIC_ATTN_TOP_DOWN,        /**< Top-down (goal-directed) */
    HH_THALAMIC_ATTN_ENDOGENOUS,      /**< Internally generated */
    HH_THALAMIC_ATTN_EXOGENOUS        /**< Externally triggered */
} hh_thalamic_attention_type_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief HH signal for thalamic routing
 */
typedef struct {
    uint32_t source_id;               /**< Source HH neuron/population ID */
    float spike_time_ms;              /**< Spike timestamp */
    float firing_rate_hz;             /**< Current firing rate */
    bool is_burst;                    /**< Part of burst pattern */
    uint32_t burst_spikes;            /**< Spikes in current burst */
    float burst_frequency_hz;         /**< Intra-burst frequency */
    float signal_reliability;         /**< Reliability from channel state */
    float population_synchrony;       /**< Population coherence [0,1] */
    hh_thalamic_mode_t current_mode;  /**< Current firing mode */
} hh_thalamic_signal_t;

/**
 * @brief Attention weight specification
 */
typedef struct {
    uint32_t source_id;               /**< Attention source (HH neuron) */
    uint32_t destination_id;          /**< Attention target */
    float weight;                     /**< Attention weight [0, 1] */
    hh_thalamic_attention_type_t type; /**< Attention type */
    hh_thalamic_priority_t priority;  /**< Routing priority */
    float decay_rate;                 /**< Weight decay rate (1/ms) */
    uint64_t last_update_ms;          /**< Last update time */
} hh_thalamic_attention_t;

/**
 * @brief TRN (Thalamic Reticular Nucleus) inhibition
 */
typedef struct {
    uint32_t source_id;               /**< TRN source */
    uint32_t target_relay;            /**< Target relay neuron */
    float inhibition_strength;        /**< Inhibition magnitude [0, 1] */
    float duration_ms;                /**< Inhibition duration */
    float onset_time_ms;              /**< Inhibition onset */
    bool active;                      /**< Currently active */
} hh_thalamic_trn_inhibition_t;

/**
 * @brief Routed signal with attention modulation
 */
typedef struct {
    uint32_t source_id;               /**< Original source */
    uint32_t destination_id;          /**< Routing destination */
    float signal_strength;            /**< Base signal strength */
    float attention_modulated;        /**< After attention modulation */
    float trn_gated;                  /**< After TRN gating */
    hh_thalamic_priority_t priority;  /**< Final priority */
    float latency_ms;                 /**< Routing latency */
    bool bypassed_queue;              /**< Bypassed normal queue */
} hh_thalamic_routed_signal_t;

/**
 * @brief Thalamic effects on HH parameters
 */
typedef struct {
    hh_thalamic_mod_target_t target;  /**< Modulation target */
    float gain_modulation;            /**< g_Na scaling [0.5, 2.0] */
    float threshold_shift_mv;         /**< Threshold adjustment (mV) */
    float current_injection;          /**< Modulatory current (uA/cm^2) */
    hh_thalamic_mode_t target_mode;   /**< Target firing mode */
    float mode_transition_rate;       /**< Mode transition speed */
} hh_thalamic_effects_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Mode settings */
    hh_thalamic_mode_t default_mode;  /**< Default firing mode */
    float tonic_v_rest;               /**< Tonic mode V_rest (mV) */
    float burst_v_rest;               /**< Burst mode V_rest (mV) */
    float mode_transition_tau_ms;     /**< Mode transition time constant */

    /* Burst detection */
    float burst_isi_threshold_ms;     /**< ISI threshold for burst */
    uint32_t min_burst_spikes;        /**< Minimum spikes for burst */

    /* Attention parameters */
    float default_attention_weight;   /**< Default attention weight */
    float attention_decay_rate;       /**< Attention weight decay */
    float min_attention_weight;       /**< Minimum attention (floor) */
    float max_attention_weight;       /**< Maximum attention (ceiling) */

    /* TRN parameters */
    bool enable_trn_gating;           /**< Enable TRN inhibition */
    float trn_inhibition_scale;       /**< TRN inhibition strength */
    float trn_decay_tau_ms;           /**< TRN inhibition decay */

    /* Routing parameters */
    bool enable_priority_routing;     /**< Use priority queues */
    bool enable_burst_priority;       /**< Bursts get high priority */
    float bypass_threshold;           /**< Attention for queue bypass */

    /* Feedback to HH */
    bool enable_hh_feedback;          /**< Enable attention modulation of HH */
    float gain_mod_sensitivity;       /**< Attention to gain scaling */
    float threshold_mod_max_mv;       /**< Maximum threshold shift */

    /* Update parameters */
    float update_interval_ms;         /**< Bridge update interval */
} hh_thalamic_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Signal routing */
    uint64_t signals_routed;          /**< Total signals routed */
    uint64_t signals_gated;           /**< Signals blocked by TRN */
    uint64_t signals_bypassed;        /**< Signals bypassing queue */
    uint64_t burst_signals;           /**< Burst-mode signals */
    uint64_t tonic_signals;           /**< Tonic-mode signals */

    /* Attention */
    float avg_attention_weight;       /**< Average attention weight */
    float max_attention_observed;     /**< Peak attention observed */
    uint64_t attention_updates;       /**< Attention weight updates */
    uint64_t priority_elevations;     /**< Priority increases */

    /* Mode transitions */
    uint64_t mode_transitions;        /**< Burst/tonic transitions */
    uint64_t bursts_detected;         /**< Bursts detected */
    float avg_burst_spikes;           /**< Average spikes per burst */

    /* TRN activity */
    uint64_t trn_inhibitions;         /**< TRN inhibition events */
    float avg_trn_strength;           /**< Average TRN strength */

    /* Performance */
    float avg_routing_latency_us;     /**< Average routing latency */
    float last_update_ms;             /**< Last update timestamp */
} hh_thalamic_stats_t;

/** Opaque bridge handle */
typedef struct hh_thalamic_bridge_struct hh_thalamic_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Initialize configuration with balanced defaults
 * WHY:  Easy creation with biologically-motivated parameters
 * HOW:  Set tonic mode, moderate attention, TRN enabled
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_thalamic_default_config(hh_thalamic_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create HH-thalamic bridge
 *
 * WHAT: Initialize bridge for attention-gated routing
 * WHY:  Enable biophysically-grounded attention
 * HOW:  Allocate routing tables, initialize attention weights
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT hh_thalamic_bridge_t* hh_thalamic_bridge_create(
    const hh_thalamic_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
NIMCP_EXPORT void hh_thalamic_bridge_destroy(hh_thalamic_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_thalamic_bridge_reset(hh_thalamic_bridge_t* bridge);

//=============================================================================
// Signal Routing API (HH to Thalamic)
//=============================================================================

/**
 * @brief Route HH signal through thalamic gating
 *
 * WHAT: Apply attention gating to HH signal
 * WHY:  Filter/amplify signals based on attention
 * HOW:  Modulate by attention weight, apply TRN gating
 *
 * @param bridge Bridge handle
 * @param signal Input HH signal
 * @param routed_out Output routed signal
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_thalamic_route_signal(
    hh_thalamic_bridge_t* bridge,
    const hh_thalamic_signal_t* signal,
    hh_thalamic_routed_signal_t* routed_out
);

/**
 * @brief Process HH spike for routing
 *
 * WHAT: Convert HH spike to routed signal
 * WHY:  Spikes are primary routing events
 * HOW:  Detect burst/tonic, apply attention, route
 *
 * @param bridge Bridge handle
 * @param source_id Source HH neuron
 * @param spike_time_ms Spike timestamp
 * @param firing_rate_hz Current firing rate
 * @param routed_out Output routed signal
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_thalamic_process_spike(
    hh_thalamic_bridge_t* bridge,
    uint32_t source_id,
    float spike_time_ms,
    float firing_rate_hz,
    hh_thalamic_routed_signal_t* routed_out
);

/**
 * @brief Detect firing mode from HH activity
 *
 * WHAT: Determine tonic vs burst mode
 * WHY:  Mode affects routing priority
 * HOW:  Analyze ISI pattern, V_rest level
 *
 * @param bridge Bridge handle
 * @param source_id Source neuron
 * @param isi_ms Current inter-spike interval
 * @param v_rest Current resting potential
 * @param mode_out Output detected mode
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_thalamic_detect_mode(
    hh_thalamic_bridge_t* bridge,
    uint32_t source_id,
    float isi_ms,
    float v_rest,
    hh_thalamic_mode_t* mode_out
);

//=============================================================================
// Attention Management API
//=============================================================================

/**
 * @brief Set attention weight for source-destination pair
 *
 * WHAT: Update attention weight for routing
 * WHY:  Attention modulates signal relay gain
 * HOW:  Store weight, apply to future signals
 *
 * @param bridge Bridge handle
 * @param source_id Source neuron/population
 * @param destination_id Destination module
 * @param weight Attention weight [0, 1]
 * @param type Attention type (top-down, bottom-up, etc.)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_thalamic_set_attention(
    hh_thalamic_bridge_t* bridge,
    uint32_t source_id,
    uint32_t destination_id,
    float weight,
    hh_thalamic_attention_type_t type
);

/**
 * @brief Get attention weight
 *
 * @param bridge Bridge handle
 * @param source_id Source neuron
 * @param destination_id Destination module
 * @param attention_out Output attention specification
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_thalamic_get_attention(
    const hh_thalamic_bridge_t* bridge,
    uint32_t source_id,
    uint32_t destination_id,
    hh_thalamic_attention_t* attention_out
);

/**
 * @brief Shift attention to new focus
 *
 * WHAT: Redirect attention to new target
 * WHY:  Implement attention shifts
 * HOW:  Boost target, suppress competitors
 *
 * @param bridge Bridge handle
 * @param focus_source New attention focus source
 * @param focus_destination New focus destination
 * @param shift_strength Strength of shift [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_thalamic_shift_attention(
    hh_thalamic_bridge_t* bridge,
    uint32_t focus_source,
    uint32_t focus_destination,
    float shift_strength
);

/**
 * @brief Apply bottom-up salience from HH activity
 *
 * WHAT: Update attention based on HH salience signals
 * WHY:  Bottom-up attention capture from spikes
 * HOW:  Burst activity increases attention weight
 *
 * @param bridge Bridge handle
 * @param source_id Source neuron
 * @param salience Salience magnitude [0, 1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_thalamic_apply_salience(
    hh_thalamic_bridge_t* bridge,
    uint32_t source_id,
    float salience
);

//=============================================================================
// TRN Gating API
//=============================================================================

/**
 * @brief Apply TRN inhibition
 *
 * WHAT: Activate TRN inhibition on relay
 * WHY:  Gate competing pathways
 * HOW:  Reduce relay gain for target
 *
 * @param bridge Bridge handle
 * @param inhibition TRN inhibition specification
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_thalamic_apply_trn_inhibition(
    hh_thalamic_bridge_t* bridge,
    const hh_thalamic_trn_inhibition_t* inhibition
);

/**
 * @brief Release TRN inhibition
 *
 * @param bridge Bridge handle
 * @param target_relay Relay to release
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_thalamic_release_trn_inhibition(
    hh_thalamic_bridge_t* bridge,
    uint32_t target_relay
);

/**
 * @brief Get TRN inhibition state
 *
 * @param bridge Bridge handle
 * @param target_relay Relay to query
 * @param inhibition_out Output inhibition state
 * @return 0 on success, -1 if no inhibition
 */
NIMCP_EXPORT int hh_thalamic_get_trn_state(
    const hh_thalamic_bridge_t* bridge,
    uint32_t target_relay,
    hh_thalamic_trn_inhibition_t* inhibition_out
);

//=============================================================================
// Feedback API (Thalamic to HH)
//=============================================================================

/**
 * @brief Compute thalamic effects on HH
 *
 * WHAT: Generate HH modulation from attention state
 * WHY:  Attention affects neural excitability
 * HOW:  Map attention to gain/threshold changes
 *
 * @param bridge Bridge handle
 * @param source_id HH neuron to modulate
 * @param effects_out Output HH effects
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_thalamic_compute_effects(
    hh_thalamic_bridge_t* bridge,
    uint32_t source_id,
    hh_thalamic_effects_t* effects_out
);

/**
 * @brief Set firing mode for HH neuron
 *
 * WHAT: Transition HH neuron to target mode
 * WHY:  Implement tonic/burst mode switching
 * HOW:  Adjust V_rest and channel states
 *
 * @param bridge Bridge handle
 * @param neuron_id Target neuron
 * @param mode Target firing mode
 * @param transition_time_ms Transition duration
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_thalamic_set_mode(
    hh_thalamic_bridge_t* bridge,
    uint32_t neuron_id,
    hh_thalamic_mode_t mode,
    float transition_time_ms
);

/**
 * @brief Get current mode for neuron
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron to query
 * @param mode_out Output current mode
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_thalamic_get_mode(
    const hh_thalamic_bridge_t* bridge,
    uint32_t neuron_id,
    hh_thalamic_mode_t* mode_out
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic bridge housekeeping
 * WHY:  Decay attention, process TRN, update modes
 * HOW:  Time-based state transitions
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_thalamic_bridge_update(
    hh_thalamic_bridge_t* bridge,
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
NIMCP_EXPORT int hh_thalamic_get_stats(
    const hh_thalamic_bridge_t* bridge,
    hh_thalamic_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_thalamic_reset_stats(hh_thalamic_bridge_t* bridge);

/**
 * @brief Get mode name string
 *
 * @param mode Thalamic mode
 * @return Static string name
 */
NIMCP_EXPORT const char* hh_thalamic_mode_name(hh_thalamic_mode_t mode);

/**
 * @brief Print bridge summary
 *
 * @param bridge Bridge handle (NULL safe)
 */
NIMCP_EXPORT void hh_thalamic_print_summary(const hh_thalamic_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HH_THALAMIC_BRIDGE_H */