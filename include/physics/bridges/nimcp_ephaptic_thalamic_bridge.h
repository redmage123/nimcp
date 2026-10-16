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
// nimcp_ephaptic_thalamic_bridge.h - Ephaptic to Thalamic Attention Gating Bridge
//=============================================================================
/**
 * @file nimcp_ephaptic_thalamic_bridge.h
 * @brief Bridge between Ephaptic coupling and Thalamic attention gating system
 *
 * WHAT: Connects ephaptic field dynamics with thalamic gating mechanisms,
 *       enabling field-driven attention allocation and sensory gating.
 *
 * WHY:  Bridges the gap between:
 *       - Cortical ephaptic synchronization
 *       - Thalamic relay and gating functions
 *       - Attention-based information routing
 *       The thalamus acts as a "gateway" to cortex; ephaptic coherence
 *       can modulate this gateway based on cortical readiness.
 *
 * HOW:  Integration mechanisms:
 *       1. Phase coherence modulates thalamic gain
 *       2. LFP phase gates sensory relay timing
 *       3. Synchronization events trigger attention shifts
 *       4. Field patterns guide pulvinar-mediated attention
 *
 * BIOLOGICAL BASIS:
 * ```
 * EPHAPTIC COUPLING                      THALAMIC GATING
 * ─────────────────────────────────────────────────────────────────────
 * Phase coherence                     -> Thalamic relay gain
 * Alpha power (8-12 Hz)               -> Sensory gating (inhibition)
 * Gamma coherence (30+ Hz)            -> Attention focus (enhancement)
 * LFP phase                           -> Relay timing window
 * Sync events                         -> Attention shift triggers
 * Field spatial pattern               -> Pulvinar routing
 * ```
 *
 * KEY MECHANISMS:
 * - Coherence-gain coupling: High cortical coherence increases thalamic gain
 * - Alpha gating: Strong alpha suppresses irrelevant sensory input
 * - Gamma enhancement: Gamma coherence enhances attended inputs
 * - Phase-locked relay: Sensory info relayed at optimal LFP phases
 *
 * REFERENCES:
 * - Sherman & Guillery (2006) "Exploring the Thalamus and Its Role in Cortical Function"
 * - Saalmann & Kastner (2011) "Cognitive and perceptual functions of the visual thalamus"
 * - Bastos et al. (2015) "Visual areas exert feedforward and feedback influences through distinct frequency channels"
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_EPHAPTIC_THALAMIC_BRIDGE_H
#define NIMCP_EPHAPTIC_THALAMIC_BRIDGE_H

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
#define EPHAPTIC_THALAMIC_MODULE_NAME       "ephaptic_thalamic_bridge"

/** Maximum thalamic nuclei */
#define EPHAPTIC_THALAMIC_MAX_NUCLEI        32

/** Maximum attention foci */
#define EPHAPTIC_THALAMIC_MAX_FOCI          8

/** Maximum relay channels */
#define EPHAPTIC_THALAMIC_MAX_CHANNELS      64

/** Default alpha power threshold for gating */
#define EPHAPTIC_THALAMIC_DEFAULT_ALPHA_GATE    0.5f

/** Default gamma coherence for attention */
#define EPHAPTIC_THALAMIC_DEFAULT_GAMMA_ATTN    0.4f

/** Default gain modulation range */
#define EPHAPTIC_THALAMIC_DEFAULT_GAIN_MIN      0.2f
#define EPHAPTIC_THALAMIC_DEFAULT_GAIN_MAX      2.0f

/** Default phase window for relay (radians) */
#define EPHAPTIC_THALAMIC_DEFAULT_PHASE_WINDOW  0.78f  /* ~45 degrees */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Thalamic nucleus type
 */
typedef enum {
    EPHAPTIC_THAL_NUCLEUS_LGN = 0,        /**< Lateral geniculate (visual) */
    EPHAPTIC_THAL_NUCLEUS_MGN,            /**< Medial geniculate (auditory) */
    EPHAPTIC_THAL_NUCLEUS_VPL,            /**< Ventral posterolateral (somatosensory) */
    EPHAPTIC_THAL_NUCLEUS_PULVINAR,       /**< Pulvinar (attention routing) */
    EPHAPTIC_THAL_NUCLEUS_TRN,            /**< Thalamic reticular (inhibitory) */
    EPHAPTIC_THAL_NUCLEUS_MD,             /**< Mediodorsal (prefrontal) */
    EPHAPTIC_THAL_NUCLEUS_CUSTOM          /**< Custom/other nucleus */
} ephaptic_thalamic_nucleus_t;

/**
 * @brief Gating mode
 */
typedef enum {
    EPHAPTIC_THAL_GATE_OPEN = 0,          /**< Always open (no gating) */
    EPHAPTIC_THAL_GATE_ALPHA,             /**< Alpha-power gating */
    EPHAPTIC_THAL_GATE_COHERENCE,         /**< Coherence-based gating */
    EPHAPTIC_THAL_GATE_PHASE,             /**< Phase-locked gating */
    EPHAPTIC_THAL_GATE_COMBINED           /**< Combined gating */
} ephaptic_thalamic_gate_mode_t;

/**
 * @brief Attention mode
 */
typedef enum {
    EPHAPTIC_THAL_ATTN_NONE = 0,          /**< No attention modulation */
    EPHAPTIC_THAL_ATTN_GAMMA,             /**< Gamma coherence attention */
    EPHAPTIC_THAL_ATTN_SYNC_EVENT,        /**< Sync event attention */
    EPHAPTIC_THAL_ATTN_SPATIAL,           /**< Spatial field pattern attention */
    EPHAPTIC_THAL_ATTN_COMBINED           /**< Combined attention modes */
} ephaptic_thalamic_attn_mode_t;

/**
 * @brief Relay timing mode
 */
typedef enum {
    EPHAPTIC_THAL_RELAY_CONTINUOUS = 0,   /**< Continuous relay */
    EPHAPTIC_THAL_RELAY_PHASE_LOCKED,     /**< Phase-locked relay */
    EPHAPTIC_THAL_RELAY_BURST,            /**< Burst-mode relay */
    EPHAPTIC_THAL_RELAY_GATED             /**< Gated relay (coherence-dependent) */
} ephaptic_thalamic_relay_mode_t;

/**
 * @brief TRN modulation mode
 */
typedef enum {
    EPHAPTIC_TRN_MOD_NONE = 0,            /**< No TRN modulation */
    EPHAPTIC_TRN_MOD_ALPHA,               /**< TRN driven by alpha */
    EPHAPTIC_TRN_MOD_COHERENCE,           /**< TRN driven by coherence */
    EPHAPTIC_TRN_MOD_ATTENTION            /**< TRN driven by attention focus */
} ephaptic_trn_mod_mode_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for ephaptic-thalamic bridge
 */
typedef struct {
    /** Gating parameters */
    ephaptic_thalamic_gate_mode_t gate_mode;   /**< Primary gating mode */
    float alpha_gate_threshold;                 /**< Alpha power for gating */
    float coherence_gate_threshold;             /**< Coherence for gating */
    float phase_gate_center;                    /**< Center phase for gating (rad) */
    float phase_gate_width;                     /**< Phase window width (rad) */

    /** Gain modulation */
    float gain_min;                             /**< Minimum gain */
    float gain_max;                             /**< Maximum gain */
    float coherence_gain_scale;                 /**< Coherence -> gain mapping */

    /** Attention parameters */
    ephaptic_thalamic_attn_mode_t attn_mode;   /**< Attention mode */
    float gamma_attn_threshold;                 /**< Gamma coherence for attention */
    float sync_event_attn_boost;                /**< Attention boost on sync */
    float attn_decay_tau_ms;                    /**< Attention decay time constant */

    /** Relay parameters */
    ephaptic_thalamic_relay_mode_t relay_mode; /**< Relay timing mode */
    float relay_phase_target;                   /**< Target phase for relay (rad) */
    float relay_phase_window;                   /**< Phase window for relay (rad) */
    float burst_threshold;                      /**< Coherence for burst mode */

    /** TRN parameters */
    ephaptic_trn_mod_mode_t trn_mode;          /**< TRN modulation mode */
    float trn_inhibition_strength;              /**< TRN inhibition strength */
    float trn_alpha_coupling;                   /**< Alpha -> TRN coupling */

    /** Update parameters */
    float update_interval_ms;                   /**< Bridge update interval */
} ephaptic_thalamic_config_t;

/**
 * @brief Nucleus state
 */
typedef struct {
    ephaptic_thalamic_nucleus_t type;          /**< Nucleus type */
    uint32_t nucleus_id;                        /**< Nucleus identifier */
    float gain;                                 /**< Current gain */
    float gate_state;                           /**< Gate openness [0,1] */
    float attention_weight;                     /**< Attention weight [0,1] */
    bool is_relaying;                           /**< Currently relaying */
    float last_relay_ms;                        /**< Last relay time */
    float coherence_coupling;                   /**< Coupling to cortical coherence */
} ephaptic_thalamic_nucleus_state_t;

/**
 * @brief Gating decision result
 */
typedef struct {
    bool gate_open;                             /**< Is gate open */
    float gate_level;                           /**< Gate level [0,1] */
    float alpha_contribution;                   /**< Alpha power effect */
    float coherence_contribution;               /**< Coherence effect */
    float phase_contribution;                   /**< Phase effect */
    float effective_gain;                       /**< Effective relay gain */
} ephaptic_thalamic_gating_t;

/**
 * @brief Attention state
 */
typedef struct {
    uint32_t focus_count;                       /**< Number of active foci */
    uint32_t focus_nuclei[8];                   /**< Nuclei with attention focus */
    float focus_strengths[8];                   /**< Focus strength per nucleus */
    float global_attention;                     /**< Global attention level */
    float gamma_coherence;                      /**< Current gamma coherence */
    bool sync_event_active;                     /**< Sync event attention active */
    float last_shift_ms;                        /**< Last attention shift time */
} ephaptic_thalamic_attention_t;

/**
 * @brief Relay event
 */
typedef struct {
    uint32_t nucleus_id;                        /**< Nucleus that relayed */
    float relay_time_ms;                        /**< Relay timestamp */
    float relay_gain;                           /**< Gain applied to relay */
    float lfp_phase;                            /**< LFP phase at relay */
    float coherence;                            /**< Coherence at relay */
    bool burst_relay;                           /**< Was burst-mode relay */
} ephaptic_thalamic_relay_event_t;

/**
 * @brief TRN (Thalamic Reticular Nucleus) state
 *
 * BIOLOGICAL: The TRN provides inhibitory gating of thalamic relay.
 * It's modulated by attention and cortical feedback.
 */
typedef struct {
    float inhibition_level;                     /**< Current inhibition [0,1] */
    float alpha_drive;                          /**< Alpha-driven component */
    float attention_modulation;                 /**< Attention modulation */
    float coherence_modulation;                 /**< Coherence modulation */
    uint32_t target_nuclei[8];                  /**< Nuclei being inhibited */
    uint32_t target_count;                      /**< Number of targets */
} ephaptic_thalamic_trn_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t gating_decisions;                  /**< Gating decisions made */
    uint64_t gates_opened;                      /**< Gates opened */
    uint64_t gates_closed;                      /**< Gates closed */
    uint64_t relay_events;                      /**< Relay events */
    uint64_t attention_shifts;                  /**< Attention shifts */
    uint64_t burst_relays;                      /**< Burst-mode relays */
    float avg_gain;                             /**< Average gain applied */
    float avg_gate_level;                       /**< Average gate level */
    float avg_attention;                        /**< Average attention level */
    float last_update_ms;                       /**< Last update timestamp */
} ephaptic_thalamic_stats_t;

/** Opaque bridge handle */
typedef struct ephaptic_thalamic_bridge_struct ephaptic_thalamic_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible default configuration values
 * WHY:  Provide biologically plausible thalamic parameters
 * HOW:  Based on thalamic gating literature
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_thalamic_default_config(
    ephaptic_thalamic_config_t* config
);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create ephaptic-thalamic bridge
 *
 * WHAT: Allocates and initializes bridge instance
 * WHY:  Enable ephaptic-modulated thalamic gating
 * HOW:  Sets up nuclei, gating tables, attention tracking
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT ephaptic_thalamic_bridge_t* ephaptic_thalamic_bridge_create(
    const ephaptic_thalamic_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
NIMCP_EXPORT void ephaptic_thalamic_bridge_destroy(
    ephaptic_thalamic_bridge_t* bridge
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_thalamic_bridge_reset(
    ephaptic_thalamic_bridge_t* bridge
);

//=============================================================================
// Nucleus Management API
//=============================================================================

/**
 * @brief Register thalamic nucleus
 *
 * WHAT: Adds nucleus to bridge for gating control
 * WHY:  Each nucleus has distinct gating properties
 * HOW:  Stores nucleus with type-specific defaults
 *
 * @param bridge     Bridge handle
 * @param nucleus_id Nucleus identifier
 * @param type       Nucleus type
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_thalamic_register_nucleus(
    ephaptic_thalamic_bridge_t* bridge,
    uint32_t nucleus_id,
    ephaptic_thalamic_nucleus_t type
);

/**
 * @brief Get nucleus state
 *
 * @param bridge     Bridge handle
 * @param nucleus_id Nucleus identifier
 * @param state      Output nucleus state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_thalamic_get_nucleus_state(
    const ephaptic_thalamic_bridge_t* bridge,
    uint32_t nucleus_id,
    ephaptic_thalamic_nucleus_state_t* state
);

/**
 * @brief Set nucleus coherence coupling
 *
 * WHAT: Sets how strongly nucleus couples to cortical coherence
 * WHY:  Different nuclei have different coupling strengths
 * HOW:  Stores coupling factor for gain computation
 *
 * @param bridge     Bridge handle
 * @param nucleus_id Nucleus identifier
 * @param coupling   Coupling strength [0,1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_thalamic_set_coherence_coupling(
    ephaptic_thalamic_bridge_t* bridge,
    uint32_t nucleus_id,
    float coupling
);

//=============================================================================
// Gating API
//=============================================================================

/**
 * @brief Compute gating for nucleus
 *
 * WHAT: Determines gate state based on ephaptic state
 * WHY:  Ephaptic coherence modulates thalamic relay
 * HOW:  Combines alpha, coherence, and phase factors
 *
 * BIOLOGICAL: High alpha power closes the thalamic gate,
 * suppressing sensory input. High gamma coherence opens it.
 *
 * @param bridge        Bridge handle
 * @param nucleus_id    Nucleus identifier
 * @param alpha_power   Current alpha band power
 * @param gamma_coherence Current gamma coherence
 * @param lfp_phase     Current LFP phase
 * @param gating        Output gating decision
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_thalamic_compute_gating(
    ephaptic_thalamic_bridge_t* bridge,
    uint32_t nucleus_id,
    float alpha_power,
    float gamma_coherence,
    float lfp_phase,
    ephaptic_thalamic_gating_t* gating
);

/**
 * @brief Apply gating to relay signal
 *
 * WHAT: Modulates signal based on gating state
 * WHY:  Actually gate the thalamic relay
 * HOW:  Multiplies signal by gate_level * gain
 *
 * @param bridge        Bridge handle
 * @param nucleus_id    Nucleus identifier
 * @param input_signal  Input signal to gate
 * @param output_signal Output: gated signal
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_thalamic_apply_gating(
    ephaptic_thalamic_bridge_t* bridge,
    uint32_t nucleus_id,
    float input_signal,
    float* output_signal
);

/**
 * @brief Check if relay should occur
 *
 * WHAT: Determines if nucleus should relay now
 * WHY:  Phase-locked relay for optimal timing
 * HOW:  Checks phase against relay window
 *
 * @param bridge     Bridge handle
 * @param nucleus_id Nucleus identifier
 * @param lfp_phase  Current LFP phase
 * @return true if relay should occur
 */
NIMCP_EXPORT bool ephaptic_thalamic_should_relay(
    const ephaptic_thalamic_bridge_t* bridge,
    uint32_t nucleus_id,
    float lfp_phase
);

//=============================================================================
// Attention API
//=============================================================================

/**
 * @brief Update attention state
 *
 * WHAT: Updates attention allocation across nuclei
 * WHY:  Track where cortical coherence focuses attention
 * HOW:  Maps coherence patterns to attention weights
 *
 * @param bridge          Bridge handle
 * @param gamma_coherence Current gamma coherence
 * @param sync_event      Is sync event occurring
 * @param attention       Output attention state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_thalamic_update_attention(
    ephaptic_thalamic_bridge_t* bridge,
    float gamma_coherence,
    bool sync_event,
    ephaptic_thalamic_attention_t* attention
);

/**
 * @brief Set attention focus to nucleus
 *
 * WHAT: Directs attention to specific nucleus
 * WHY:  External attention control (e.g., from pulvinar)
 * HOW:  Sets attention weight for nucleus
 *
 * @param bridge     Bridge handle
 * @param nucleus_id Nucleus to focus
 * @param strength   Focus strength [0,1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_thalamic_set_focus(
    ephaptic_thalamic_bridge_t* bridge,
    uint32_t nucleus_id,
    float strength
);

/**
 * @brief Get current attention state
 *
 * @param bridge    Bridge handle
 * @param attention Output attention state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_thalamic_get_attention(
    const ephaptic_thalamic_bridge_t* bridge,
    ephaptic_thalamic_attention_t* attention
);

//=============================================================================
// TRN API
//=============================================================================

/**
 * @brief Update TRN (Thalamic Reticular Nucleus) state
 *
 * WHAT: Updates inhibitory modulation from TRN
 * WHY:  TRN provides attentional gating of thalamus
 * HOW:  Computes TRN activity from alpha and attention
 *
 * @param bridge      Bridge handle
 * @param alpha_power Current alpha power
 * @param attention   Current attention state
 * @param trn_state   Output TRN state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_thalamic_update_trn(
    ephaptic_thalamic_bridge_t* bridge,
    float alpha_power,
    const ephaptic_thalamic_attention_t* attention,
    ephaptic_thalamic_trn_state_t* trn_state
);

/**
 * @brief Get TRN state
 *
 * @param bridge    Bridge handle
 * @param trn_state Output TRN state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_thalamic_get_trn(
    const ephaptic_thalamic_bridge_t* bridge,
    ephaptic_thalamic_trn_state_t* trn_state
);

//=============================================================================
// Relay Event API
//=============================================================================

/**
 * @brief Process relay event
 *
 * WHAT: Records and processes a thalamic relay event
 * WHY:  Track relay timing and gating effectiveness
 * HOW:  Logs event with current ephaptic context
 *
 * @param bridge     Bridge handle
 * @param nucleus_id Nucleus that relayed
 * @param lfp_phase  LFP phase at relay
 * @param coherence  Coherence at relay
 * @param event      Output relay event
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_thalamic_process_relay(
    ephaptic_thalamic_bridge_t* bridge,
    uint32_t nucleus_id,
    float lfp_phase,
    float coherence,
    ephaptic_thalamic_relay_event_t* event
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Decay attention, update gating, process events
 * HOW:  Called during simulation step
 *
 * @param bridge        Bridge handle
 * @param dt_ms         Time step in milliseconds
 * @param alpha_power   Current alpha power
 * @param gamma_coherence Current gamma coherence
 * @param lfp_phase     Current LFP phase
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_thalamic_update(
    ephaptic_thalamic_bridge_t* bridge,
    float dt_ms,
    float alpha_power,
    float gamma_coherence,
    float lfp_phase
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
NIMCP_EXPORT int ephaptic_thalamic_get_stats(
    const ephaptic_thalamic_bridge_t* bridge,
    ephaptic_thalamic_stats_t* stats
);

/**
 * @brief Get average gate level
 *
 * @param bridge     Bridge handle
 * @param gate_level Output: average gate level
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_thalamic_get_avg_gate(
    const ephaptic_thalamic_bridge_t* bridge,
    float* gate_level
);

/**
 * @brief Check if bridge is active
 *
 * @param bridge Bridge handle
 * @return true if bridge is active
 */
NIMCP_EXPORT bool ephaptic_thalamic_is_active(
    const ephaptic_thalamic_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EPHAPTIC_THALAMIC_BRIDGE_H */