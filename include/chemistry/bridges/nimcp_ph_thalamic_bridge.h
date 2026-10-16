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
// nimcp_ph_thalamic_bridge.h - pH Dynamics to Thalamic Attention Gating Bridge
//=============================================================================
/**
 * @file nimcp_ph_thalamic_bridge.h
 * @brief Bridge connecting pH dynamics with thalamic attention gating systems
 *
 * WHAT: Bidirectional bridge between pH dynamics and thalamic circuits for
 *       modeling pH-dependent attention and sensory gating modulation.
 *
 * WHY:  pH significantly affects thalamic function and attention:
 *       - Thalamic T-type Ca2+ channels are pH-sensitive
 *       - Acidosis shifts thalamus toward burst firing (drowsiness)
 *       - Alkalosis promotes tonic firing (alertness)
 *       - Reticular nucleus inhibition is pH-modulated
 *       - Thalamocortical relay fidelity depends on pH
 *
 * HOW:  Two-way integration:
 *       1. pH -> Thalamus: Modulate T-channel gating, relay fidelity
 *       2. Thalamus -> pH: Arousal state affects metabolic demand
 *       3. T-channel kinetics: pH shifts activation/inactivation curves
 *       4. Attentional gating: pH affects TRN inhibition strength
 *
 * BIOLOGICAL BASIS:
 * ```
 * pH DYNAMICS                              THALAMIC EFFECTS
 * ---------------------------------------------------------------
 * Extracellular acidosis (pH < 7.3)     -> T-channel shift to burst
 * Extracellular alkalosis (pH > 7.5)    -> T-channel shift to tonic
 * ASIC activation                       -> Transient depolarization
 * CO2 elevation (hypercapnia)           -> Drowsiness via thalamus
 * Intracellular acidosis                -> Reduced relay fidelity
 * Proton-gated channels                 -> TRN modulation
 * ```
 *
 * T-TYPE CALCIUM CHANNEL pH SENSITIVITY:
 * - Acidosis shifts activation to more depolarized voltages
 * - Acidosis accelerates inactivation
 * - Net effect: reduced window current, more burst-prone
 * - pH 7.0 reduces T-current by ~30%
 *
 * ATTENTIONAL IMPLICATIONS:
 * - Burst mode: poor sensory relay, drowsiness, sleep spindles
 * - Tonic mode: faithful relay, alertness, attention
 * - TRN inhibition: attention filtering via lateral inhibition
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_PH_THALAMIC_BRIDGE_H
#define NIMCP_PH_THALAMIC_BRIDGE_H

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
#define PH_THALAMIC_MODULE_NAME         "ph_thalamic_bridge"

/** Maximum thalamic nuclei */
#define PH_THALAMIC_MAX_NUCLEI          32

/** Maximum relay channels */
#define PH_THALAMIC_MAX_RELAYS          128

/** Normal extracellular pH */
#define PH_THALAMIC_NORMAL_PH           7.4f

/** T-channel activation shift per 0.1 pH (mV) */
#define PH_THALAMIC_T_ACTIVATION_SHIFT  2.5f

/** T-channel inactivation shift per 0.1 pH (mV) */
#define PH_THALAMIC_T_INACT_SHIFT       3.0f

/** T-current reduction at pH 7.0 (fraction) */
#define PH_THALAMIC_T_REDUCTION_7_0     0.30f

/** pH threshold for burst mode transition */
#define PH_THALAMIC_BURST_THRESHOLD     7.2f

/** pH threshold for enhanced tonic mode */
#define PH_THALAMIC_TONIC_THRESHOLD     7.5f

/** TRN inhibition pH sensitivity */
#define PH_THALAMIC_TRN_PH_SENS         0.15f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Thalamic nucleus types
 */
typedef enum {
    PH_THALAMIC_NUCLEUS_LGN = 0,         /**< Lateral geniculate (visual) */
    PH_THALAMIC_NUCLEUS_MGN,             /**< Medial geniculate (auditory) */
    PH_THALAMIC_NUCLEUS_VPL,             /**< Ventral posterolateral (somatic) */
    PH_THALAMIC_NUCLEUS_VPM,             /**< Ventral posteromedial (face) */
    PH_THALAMIC_NUCLEUS_VA,              /**< Ventral anterior (motor) */
    PH_THALAMIC_NUCLEUS_MD,              /**< Mediodorsal (prefrontal) */
    PH_THALAMIC_NUCLEUS_PULVINAR,        /**< Pulvinar (attention) */
    PH_THALAMIC_NUCLEUS_TRN,             /**< Thalamic reticular nucleus */
    PH_THALAMIC_NUCLEUS_COUNT
} ph_thalamic_nucleus_t;

/**
 * @brief Thalamic firing mode
 */
typedef enum {
    PH_THALAMIC_MODE_TONIC = 0,          /**< Tonic (relay) mode */
    PH_THALAMIC_MODE_BURST,              /**< Burst mode */
    PH_THALAMIC_MODE_MIXED,              /**< Mixed tonic/burst */
    PH_THALAMIC_MODE_SUPPRESSED          /**< Activity suppressed */
} ph_thalamic_mode_t;

/**
 * @brief Attention gating state
 */
typedef enum {
    PH_THALAMIC_GATE_OPEN = 0,           /**< Full relay (attending) */
    PH_THALAMIC_GATE_PARTIAL,            /**< Partial relay */
    PH_THALAMIC_GATE_FILTERED,           /**< TRN filtering active */
    PH_THALAMIC_GATE_CLOSED              /**< Blocked (not attending) */
} ph_thalamic_gate_state_t;

/**
 * @brief pH modulation target in thalamus
 */
typedef enum {
    PH_THALAMIC_MOD_T_CHANNEL = 0,       /**< T-type Ca2+ channels */
    PH_THALAMIC_MOD_H_CURRENT,           /**< HCN/Ih current */
    PH_THALAMIC_MOD_GABA_A,              /**< GABA-A inhibition */
    PH_THALAMIC_MOD_NMDA,                /**< NMDA receptors */
    PH_THALAMIC_MOD_TRN_INHIBITION,      /**< TRN inhibitory strength */
    PH_THALAMIC_MOD_COUNT
} ph_thalamic_mod_target_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Bridge configuration
 */
typedef struct {
    /** pH monitoring parameters */
    bool monitor_extracellular;          /**< Monitor extracellular pH */
    float ph_sample_interval_ms;         /**< pH sampling interval */

    /** T-channel modulation */
    bool enable_t_channel_mod;           /**< Enable T-channel pH effects */
    float t_activation_shift_rate;       /**< mV shift per pH unit */
    float t_inactivation_shift_rate;     /**< mV shift per pH unit */

    /** Firing mode parameters */
    float burst_threshold_ph;            /**< pH below which burst promoted */
    float tonic_threshold_ph;            /**< pH above which tonic promoted */

    /** TRN parameters */
    bool enable_trn_modulation;          /**< Enable TRN pH effects */
    float trn_ph_sensitivity;            /**< TRN inhibition pH scaling */

    /** Attention gating parameters */
    bool enable_attention_gating;        /**< Enable attention modulation */
    float gate_threshold;                /**< Gating threshold */

    /** Arousal feedback */
    bool enable_arousal_feedback;        /**< Enable thalamus->pH feedback */
    float arousal_metabolic_factor;      /**< Arousal acid production */

    /** Update parameters */
    float update_interval_ms;            /**< Bridge update interval */
} ph_thalamic_config_t;

/**
 * @brief T-type calcium channel state
 */
typedef struct {
    float v_activation_half;             /**< Activation V1/2 (mV) */
    float v_inactivation_half;           /**< Inactivation V1/2 (mV) */
    float activation_slope;              /**< Activation slope */
    float inactivation_slope;            /**< Inactivation slope */
    float tau_activation_ms;             /**< Activation time constant */
    float tau_inactivation_ms;           /**< Inactivation time constant */
    float g_max;                         /**< Maximum conductance (nS) */
    float current_amplitude;             /**< Current T-current */
    float window_current;                /**< Window current magnitude */
    float ph_shift;                      /**< pH-induced shift (mV) */
} ph_thalamic_t_channel_t;

/**
 * @brief Thalamic nucleus state
 */
typedef struct {
    ph_thalamic_nucleus_t type;          /**< Nucleus type */
    char name[32];                       /**< Nucleus name */
    ph_thalamic_mode_t firing_mode;      /**< Current firing mode */
    float relay_fidelity;                /**< Relay fidelity (0-1) */
    float burst_probability;             /**< Burst probability */
    float tonic_fraction;                /**< Fraction in tonic mode */
    float local_ph;                      /**< Local pH at nucleus */
    ph_thalamic_t_channel_t t_channel;   /**< T-channel state */
    bool is_trn;                         /**< Is this TRN */
    float trn_inhibition_strength;       /**< TRN inhibitory output */
} ph_thalamic_nucleus_state_t;

/**
 * @brief Attention gating state
 */
typedef struct {
    ph_thalamic_gate_state_t state;      /**< Current gating state */
    float gate_level;                    /**< Gate open level (0-1) */
    float trn_activity;                  /**< TRN inhibitory activity */
    float attention_signal;              /**< Top-down attention */
    float ph_modulation;                 /**< pH effect on gating */
    uint32_t attended_channel;           /**< Currently attended channel */
    float timestamp_ms;                  /**< Timestamp */
} ph_thalamic_gate_state_info_t;

/**
 * @brief Modulation output for thalamic systems
 */
typedef struct {
    /** T-channel modulation */
    float t_activation_shift_mv;         /**< T-channel activation shift */
    float t_inactivation_shift_mv;       /**< T-channel inactivation shift */
    float t_current_factor;              /**< T-current scaling (0-1) */

    /** Mode probabilities */
    float burst_mode_probability;        /**< P(burst mode) */
    float tonic_mode_probability;        /**< P(tonic mode) */

    /** Relay modulation */
    float relay_fidelity_factor;         /**< Relay fidelity scaling */
    float signal_to_noise_factor;        /**< SNR modulation */

    /** TRN modulation */
    float trn_inhibition_factor;         /**< TRN inhibition strength */

    /** Attention gating */
    float attention_gate_factor;         /**< Attention gate opening */
    float arousal_level;                 /**< Implied arousal level */
} ph_thalamic_modulation_t;

/**
 * @brief Arousal feedback to pH
 */
typedef struct {
    float arousal_level;                 /**< Current arousal (0-1) */
    float metabolic_demand;              /**< Metabolic demand from arousal */
    float acid_production;               /**< Acid produced by thalamus */
    float attention_load;                /**< Attentional processing load */
    float timestamp_ms;                  /**< Timestamp */
} ph_thalamic_arousal_feedback_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t ph_updates;                 /**< pH state updates */
    uint64_t modulations_computed;       /**< Modulation computations */
    uint64_t mode_transitions;           /**< Firing mode transitions */
    uint64_t gate_changes;               /**< Attention gate changes */
    float avg_relay_fidelity;            /**< Average relay fidelity */
    float avg_burst_probability;         /**< Average burst probability */
    float time_in_burst_mode_ms;         /**< Time in burst mode */
    float time_in_tonic_mode_ms;         /**< Time in tonic mode */
    float avg_t_current_factor;          /**< Average T-current factor */
    float last_update_ms;                /**< Last update timestamp */
} ph_thalamic_stats_t;

/** Opaque bridge handle */
typedef struct ph_thalamic_bridge_struct ph_thalamic_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_thalamic_default_config(ph_thalamic_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create pH-thalamic bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT ph_thalamic_bridge_t* ph_thalamic_bridge_create(
    const ph_thalamic_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void ph_thalamic_bridge_destroy(ph_thalamic_bridge_t* bridge);

//=============================================================================
// pH Input API (pH Dynamics -> Thalamus)
//=============================================================================

/**
 * @brief Update pH state from pH dynamics module
 *
 * WHAT: Receives current pH values from pH dynamics
 * WHY:  pH state determines thalamic modulation
 * HOW:  Computes T-channel shifts, mode probabilities
 *
 * @param bridge Bridge handle
 * @param extracellular_ph Current extracellular pH
 * @param intracellular_ph Current intracellular pH
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_thalamic_update_ph(
    ph_thalamic_bridge_t* bridge,
    float extracellular_ph,
    float intracellular_ph
);

/**
 * @brief Set local pH for specific nucleus
 *
 * @param bridge Bridge handle
 * @param nucleus Nucleus type
 * @param local_ph Local pH value
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_thalamic_set_nucleus_ph(
    ph_thalamic_bridge_t* bridge,
    ph_thalamic_nucleus_t nucleus,
    float local_ph
);

//=============================================================================
// Modulation API
//=============================================================================

/**
 * @brief Compute thalamic modulation based on current pH
 *
 * WHAT: Calculates all pH-dependent thalamic modulations
 * WHY:  Single call for complete modulation state
 * HOW:  Evaluates T-channel shifts, mode probabilities, gating
 *
 * @param bridge Bridge handle
 * @param modulation Output modulation values
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_thalamic_compute_modulation(
    ph_thalamic_bridge_t* bridge,
    ph_thalamic_modulation_t* modulation
);

/**
 * @brief Get T-channel activation voltage shift
 *
 * @param bridge Bridge handle
 * @param shift_mv Output: voltage shift in mV
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_thalamic_get_t_activation_shift(
    const ph_thalamic_bridge_t* bridge,
    float* shift_mv
);

/**
 * @brief Get T-channel current factor
 *
 * @param bridge Bridge handle
 * @param factor Output: T-current factor (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_thalamic_get_t_current_factor(
    const ph_thalamic_bridge_t* bridge,
    float* factor
);

/**
 * @brief Get current firing mode probability
 *
 * @param bridge Bridge handle
 * @param burst_prob Output: burst mode probability
 * @param tonic_prob Output: tonic mode probability
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_thalamic_get_mode_probability(
    const ph_thalamic_bridge_t* bridge,
    float* burst_prob,
    float* tonic_prob
);

/**
 * @brief Get relay fidelity factor
 *
 * @param bridge Bridge handle
 * @param nucleus Nucleus type
 * @param fidelity Output: relay fidelity (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_thalamic_get_relay_fidelity(
    const ph_thalamic_bridge_t* bridge,
    ph_thalamic_nucleus_t nucleus,
    float* fidelity
);

//=============================================================================
// Attention Gating API
//=============================================================================

/**
 * @brief Get current attention gate state
 *
 * WHAT: Returns current attention gating state
 * WHY:  pH affects attention through thalamic gating
 * HOW:  Combines pH effect with TRN state
 *
 * @param bridge Bridge handle
 * @param state Output gate state info
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_thalamic_get_gate_state(
    const ph_thalamic_bridge_t* bridge,
    ph_thalamic_gate_state_info_t* state
);

/**
 * @brief Get attention gate level
 *
 * @param bridge Bridge handle
 * @param gate_level Output: gate level (0=closed, 1=open)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_thalamic_get_gate_level(
    const ph_thalamic_bridge_t* bridge,
    float* gate_level
);

/**
 * @brief Set top-down attention signal
 *
 * @param bridge Bridge handle
 * @param attention_signal Attention signal (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_thalamic_set_attention(
    ph_thalamic_bridge_t* bridge,
    float attention_signal
);

/**
 * @brief Get TRN inhibition strength
 *
 * @param bridge Bridge handle
 * @param inhibition Output: TRN inhibition (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_thalamic_get_trn_inhibition(
    const ph_thalamic_bridge_t* bridge,
    float* inhibition
);

//=============================================================================
// Nucleus API
//=============================================================================

/**
 * @brief Get nucleus state
 *
 * @param bridge Bridge handle
 * @param nucleus Nucleus type
 * @param state Output nucleus state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_thalamic_get_nucleus_state(
    const ph_thalamic_bridge_t* bridge,
    ph_thalamic_nucleus_t nucleus,
    ph_thalamic_nucleus_state_t* state
);

/**
 * @brief Get dominant firing mode for nucleus
 *
 * @param bridge Bridge handle
 * @param nucleus Nucleus type
 * @return Current firing mode
 */
NIMCP_EXPORT ph_thalamic_mode_t ph_thalamic_get_firing_mode(
    const ph_thalamic_bridge_t* bridge,
    ph_thalamic_nucleus_t nucleus
);

//=============================================================================
// Arousal Feedback API (Thalamus -> pH)
//=============================================================================

/**
 * @brief Get arousal-based metabolic feedback
 *
 * WHAT: Returns thalamic metabolic demand
 * WHY:  High arousal increases acid production
 * HOW:  Computes based on tonic activity
 *
 * @param bridge Bridge handle
 * @param feedback Output feedback state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_thalamic_get_arousal_feedback(
    const ph_thalamic_bridge_t* bridge,
    ph_thalamic_arousal_feedback_t* feedback
);

/**
 * @brief Get current arousal level
 *
 * @param bridge Bridge handle
 * @param arousal Output: arousal level (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_thalamic_get_arousal_level(
    const ph_thalamic_bridge_t* bridge,
    float* arousal
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Process pH changes, update T-channel kinetics
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_thalamic_update(
    ph_thalamic_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_thalamic_reset(ph_thalamic_bridge_t* bridge);

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
NIMCP_EXPORT int ph_thalamic_get_stats(
    const ph_thalamic_bridge_t* bridge,
    ph_thalamic_stats_t* stats
);

/**
 * @brief Check if thalamus is in burst mode (drowsy)
 *
 * @param bridge Bridge handle
 * @return true if predominantly burst mode
 */
NIMCP_EXPORT bool ph_thalamic_is_burst_mode(
    const ph_thalamic_bridge_t* bridge
);

/**
 * @brief Check if thalamus is in tonic mode (alert)
 *
 * @param bridge Bridge handle
 * @return true if predominantly tonic mode
 */
NIMCP_EXPORT bool ph_thalamic_is_tonic_mode(
    const ph_thalamic_bridge_t* bridge
);

/**
 * @brief Check if relay is compromised by pH
 *
 * @param bridge Bridge handle
 * @return true if relay fidelity below threshold
 */
NIMCP_EXPORT bool ph_thalamic_is_relay_compromised(
    const ph_thalamic_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PH_THALAMIC_BRIDGE_H */