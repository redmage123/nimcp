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
// nimcp_ephaptic_plasticity_bridge.h - Ephaptic to Plasticity/STDP Bridge
//=============================================================================
/**
 * @file nimcp_ephaptic_plasticity_bridge.h
 * @brief Bidirectional bridge between Ephaptic coupling and STDP/plasticity
 *
 * WHAT: Connects ephaptic field dynamics with synaptic plasticity mechanisms,
 *       enabling field-modulated learning and phase-dependent STDP.
 *
 * WHY:  Bridges the gap between:
 *       - Ephaptic field coherence (population synchronization)
 *       - STDP timing windows (spike-timing dependent plasticity)
 *       - Eligibility trace modulation (three-factor learning)
 *       Ephaptic coherence provides a modulatory signal that can gate
 *       plasticity, scale learning rates, and bias LTP/LTD balance.
 *
 * HOW:  Integration mechanisms:
 *       1. Phase coherence gates eligibility trace conversion
 *       2. LFP phase modulates STDP window asymmetry
 *       3. Field strength scales learning rate
 *       4. Synchronization events trigger consolidation
 *
 * BIOLOGICAL BASIS:
 * ```
 * EPHAPTIC COUPLING                      PLASTICITY/STDP
 * ─────────────────────────────────────────────────────────────────────
 * Phase coherence (0-1)               -> Eligibility conversion gate
 * LFP phase (theta/gamma)             -> STDP window asymmetry
 * Field strength (V/m)                -> Learning rate scaling
 * Synchronization burst               -> Memory consolidation trigger
 * Kuramoto order parameter            -> Heterosynaptic plasticity scope
 * Field direction                     -> Directional synaptic bias
 * ```
 *
 * KEY MECHANISMS:
 * - Coherence-gated plasticity: High coherence permits eligibility conversion
 * - Phase-dependent STDP: LFP phase biases toward LTP or LTD
 * - Field-scaled learning: Stronger fields = faster plasticity
 * - Sync-triggered consolidation: Population sync events consolidate traces
 *
 * REFERENCES:
 * - Huerta & Lisman (1995) "Bidirectional synaptic plasticity induced by
 *   a single burst during cholinergic theta oscillation"
 * - Magee & Johnston (1997) "A synaptically controlled, associative signal
 *   for Hebbian plasticity in hippocampal neurons"
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_EPHAPTIC_PLASTICITY_BRIDGE_H
#define NIMCP_EPHAPTIC_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"
#include "constants/nimcp_math_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define EPHAPTIC_PLASTICITY_MODULE_NAME     "ephaptic_plasticity_bridge"

/** Maximum tracked synapses */
#define EPHAPTIC_PLASTICITY_MAX_SYNAPSES    8192

/** Maximum eligibility traces per update */
#define EPHAPTIC_PLASTICITY_MAX_TRACES      2048

/** Default coherence threshold for gating (order parameter) */
#define EPHAPTIC_PLASTICITY_DEFAULT_COHERENCE_GATE  0.65f

/** Default LTP phase range center (radians, peak of theta) */
#define EPHAPTIC_PLASTICITY_DEFAULT_LTP_PHASE       0.0f

/** Default LTD phase range center (radians, trough of theta) */
#define EPHAPTIC_PLASTICITY_DEFAULT_LTD_PHASE       NIMCP_PI_F

/** Default field scaling gain for learning rate */
#define EPHAPTIC_PLASTICITY_DEFAULT_FIELD_GAIN      0.1f

/** Default consolidation threshold (coherence level) */
#define EPHAPTIC_PLASTICITY_DEFAULT_CONSOL_THRESH   0.8f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Plasticity modulation mode
 */
typedef enum {
    EPHAPTIC_PLAST_MOD_GATING = 0,       /**< Coherence gates plasticity on/off */
    EPHAPTIC_PLAST_MOD_SCALING,          /**< Field scales learning rate */
    EPHAPTIC_PLAST_MOD_PHASE_BIAS,       /**< Phase biases LTP/LTD balance */
    EPHAPTIC_PLAST_MOD_COMBINED          /**< All modulations combined */
} ephaptic_plasticity_mod_mode_t;

/**
 * @brief STDP window modification type
 */
typedef enum {
    EPHAPTIC_PLAST_STDP_UNCHANGED = 0,   /**< No STDP modification */
    EPHAPTIC_PLAST_STDP_PHASE_SHIFT,     /**< Phase-dependent window shift */
    EPHAPTIC_PLAST_STDP_ASYMMETRY,       /**< Modify LTP/LTD asymmetry */
    EPHAPTIC_PLAST_STDP_WIDTH_SCALE      /**< Scale window width with field */
} ephaptic_plasticity_stdp_mod_t;

/**
 * @brief Consolidation trigger mode
 */
typedef enum {
    EPHAPTIC_PLAST_CONSOL_NONE = 0,      /**< No consolidation triggers */
    EPHAPTIC_PLAST_CONSOL_COHERENCE,     /**< Coherence threshold trigger */
    EPHAPTIC_PLAST_CONSOL_BURST,         /**< Synchronization burst trigger */
    EPHAPTIC_PLAST_CONSOL_PHASE_LOCK     /**< Phase-lock event trigger */
} ephaptic_plasticity_consol_mode_t;

/**
 * @brief Eligibility trace type affected
 */
typedef enum {
    EPHAPTIC_PLAST_TRACE_PRE = 0,        /**< Presynaptic trace */
    EPHAPTIC_PLAST_TRACE_POST,           /**< Postsynaptic trace */
    EPHAPTIC_PLAST_TRACE_HETERO,         /**< Heterosynaptic trace */
    EPHAPTIC_PLAST_TRACE_ALL             /**< All trace types */
} ephaptic_plasticity_trace_type_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for ephaptic-plasticity bridge
 */
typedef struct {
    /** Modulation parameters */
    ephaptic_plasticity_mod_mode_t mod_mode;     /**< Primary modulation mode */
    float coherence_gate_threshold;               /**< Coherence for gating */
    float field_scaling_gain;                     /**< Field->learning rate gain */

    /** Phase-dependent STDP parameters */
    ephaptic_plasticity_stdp_mod_t stdp_mod;     /**< STDP modification type */
    float ltp_preferred_phase;                    /**< Preferred phase for LTP (rad) */
    float ltd_preferred_phase;                    /**< Preferred phase for LTD (rad) */
    float phase_selectivity;                      /**< Phase selectivity width (rad) */

    /** STDP window modulation */
    float ltp_window_base_ms;                     /**< Base LTP window width */
    float ltd_window_base_ms;                     /**< Base LTD window width */
    float window_scale_factor;                    /**< Field-based window scaling */

    /** Eligibility trace modulation */
    bool enable_trace_gating;                     /**< Gate trace conversion */
    ephaptic_plasticity_trace_type_t trace_type; /**< Which traces to modulate */
    float trace_decay_modulation;                 /**< Field effect on trace decay */

    /** Consolidation parameters */
    ephaptic_plasticity_consol_mode_t consol_mode; /**< Consolidation mode */
    float consolidation_threshold;                /**< Threshold for consolidation */
    float consolidation_factor;                   /**< Consolidation strength */

    /** Update parameters */
    float update_interval_ms;                     /**< Bridge update interval */
    bool enable_heterosynaptic;                   /**< Enable heterosynaptic effects */
} ephaptic_plasticity_config_t;

/**
 * @brief Plasticity modulation result for a synapse
 */
typedef struct {
    uint32_t synapse_id;                          /**< Synapse identifier */
    float learning_rate_factor;                   /**< Learning rate multiplier */
    float ltp_probability;                        /**< LTP probability modifier */
    float ltd_probability;                        /**< LTD probability modifier */
    float ltp_window_ms;                          /**< Effective LTP window */
    float ltd_window_ms;                          /**< Effective LTD window */
    float eligibility_decay_factor;               /**< Trace decay modifier */
    bool plasticity_gated;                        /**< Is plasticity enabled */
} ephaptic_plasticity_modulation_t;

/**
 * @brief Eligibility trace state with ephaptic modulation
 */
typedef struct {
    uint32_t synapse_id;                          /**< Synapse identifier */
    float trace_value;                            /**< Current trace value */
    float accumulated_coherence;                  /**< Integrated coherence */
    float phase_at_activation;                    /**< LFP phase when activated */
    float field_strength_avg;                     /**< Averaged field strength */
    bool ready_for_conversion;                    /**< Meets conversion criteria */
    float last_update_ms;                         /**< Last update time */
} ephaptic_plasticity_trace_t;

/**
 * @brief Consolidation event
 */
typedef struct {
    float event_time_ms;                          /**< Event timestamp */
    float coherence_at_event;                     /**< Coherence level */
    float field_strength;                         /**< Field strength */
    uint32_t traces_consolidated;                 /**< Number of traces converted */
    float total_weight_change;                    /**< Aggregate weight change */
    bool is_ltp_dominant;                         /**< LTP > LTD in this event */
} ephaptic_plasticity_consol_event_t;

/**
 * @brief Phase-dependent plasticity bias
 */
typedef struct {
    float current_phase;                          /**< Current LFP phase */
    float ltp_bias;                               /**< LTP bias factor [0,2] */
    float ltd_bias;                               /**< LTD bias factor [0,2] */
    float net_bias;                               /**< Net plasticity bias */
    bool in_ltp_window;                           /**< In LTP-preferred phase */
    bool in_ltd_window;                           /**< In LTD-preferred phase */
} ephaptic_plasticity_phase_bias_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t modulations_computed;                /**< Total modulations computed */
    uint64_t traces_gated;                        /**< Traces gated by coherence */
    uint64_t traces_converted;                    /**< Traces converted to weights */
    uint64_t consolidation_events;                /**< Consolidation triggers */
    uint64_t ltp_events;                          /**< LTP events */
    uint64_t ltd_events;                          /**< LTD events */
    float avg_learning_rate_factor;               /**< Average LR multiplier */
    float avg_coherence;                          /**< Average observed coherence */
    float total_weight_change;                    /**< Net weight change */
    float ltp_ltd_ratio;                          /**< LTP/LTD event ratio */
    float last_update_ms;                         /**< Last update timestamp */
} ephaptic_plasticity_stats_t;

/** Opaque bridge handle */
typedef struct ephaptic_plasticity_bridge_struct ephaptic_plasticity_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible default configuration values
 * WHY:  Provide biologically plausible starting parameters
 * HOW:  Based on theta-phase plasticity literature
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_plasticity_default_config(
    ephaptic_plasticity_config_t* config
);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create ephaptic-plasticity bridge
 *
 * WHAT: Allocates and initializes bridge instance
 * WHY:  Enable ephaptic modulation of plasticity
 * HOW:  Allocates trace buffers, sets up modulation tables
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT ephaptic_plasticity_bridge_t* ephaptic_plasticity_bridge_create(
    const ephaptic_plasticity_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
NIMCP_EXPORT void ephaptic_plasticity_bridge_destroy(
    ephaptic_plasticity_bridge_t* bridge
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_plasticity_bridge_reset(
    ephaptic_plasticity_bridge_t* bridge
);

//=============================================================================
// Modulation Computation API
//=============================================================================

/**
 * @brief Compute plasticity modulation for synapse
 *
 * WHAT: Calculates ephaptic-based modulation of plasticity
 * WHY:  Field coherence should influence learning
 * HOW:  Combines coherence gating, phase bias, field scaling
 *
 * BIOLOGICAL: Theta oscillations (4-8 Hz) modulate plasticity,
 * with LTP favored at theta peak and LTD at theta trough.
 *
 * @param bridge         Bridge handle
 * @param synapse_id     Synapse identifier
 * @param coherence      Current phase coherence [0,1]
 * @param lfp_phase      Current LFP phase (radians)
 * @param field_strength E-field strength (V/m)
 * @param modulation     Output modulation result
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_plasticity_compute_modulation(
    ephaptic_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float coherence,
    float lfp_phase,
    float field_strength,
    ephaptic_plasticity_modulation_t* modulation
);

/**
 * @brief Compute phase-dependent bias
 *
 * WHAT: Calculates LTP/LTD bias based on LFP phase
 * WHY:  Phase encodes when learning should occur
 * HOW:  Cosine similarity to preferred phases
 *
 * @param bridge   Bridge handle
 * @param lfp_phase Current LFP phase (radians)
 * @param bias     Output phase bias
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_plasticity_compute_phase_bias(
    ephaptic_plasticity_bridge_t* bridge,
    float lfp_phase,
    ephaptic_plasticity_phase_bias_t* bias
);

/**
 * @brief Scale STDP windows based on field
 *
 * WHAT: Modifies STDP window widths based on field strength
 * WHY:  Strong fields may narrow/widen learning windows
 * HOW:  Window = base_window * (1 + field * scale_factor)
 *
 * @param bridge         Bridge handle
 * @param field_strength E-field strength (V/m)
 * @param ltp_window_ms  Output: LTP window width
 * @param ltd_window_ms  Output: LTD window width
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_plasticity_scale_stdp_windows(
    ephaptic_plasticity_bridge_t* bridge,
    float field_strength,
    float* ltp_window_ms,
    float* ltd_window_ms
);

//=============================================================================
// Eligibility Trace API
//=============================================================================

/**
 * @brief Register eligibility trace for modulation
 *
 * WHAT: Tracks eligibility trace with ephaptic state
 * WHY:  Enable coherence-gated trace conversion
 * HOW:  Stores trace with current phase/field context
 *
 * @param bridge     Bridge handle
 * @param synapse_id Synapse identifier
 * @param trace_value Initial trace value
 * @param lfp_phase  LFP phase at trace creation
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_plasticity_register_trace(
    ephaptic_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float trace_value,
    float lfp_phase
);

/**
 * @brief Update eligibility trace with current state
 *
 * WHAT: Updates trace accumulation
 * WHY:  Integrate coherence and field effects over time
 * HOW:  Accumulates coherence, averages field strength
 *
 * @param bridge      Bridge handle
 * @param synapse_id  Synapse identifier
 * @param coherence   Current coherence
 * @param field_strength Current field strength
 * @param dt_ms       Time step
 * @param trace       Output updated trace state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_plasticity_update_trace(
    ephaptic_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float coherence,
    float field_strength,
    float dt_ms,
    ephaptic_plasticity_trace_t* trace
);

/**
 * @brief Check if trace is ready for conversion
 *
 * WHAT: Determines if trace meets conversion criteria
 * WHY:  Coherence-gated plasticity requires threshold
 * HOW:  Checks accumulated coherence against threshold
 *
 * @param bridge     Bridge handle
 * @param synapse_id Synapse identifier
 * @return true if ready for conversion
 */
NIMCP_EXPORT bool ephaptic_plasticity_trace_ready(
    const ephaptic_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Convert eligible traces to weight changes
 *
 * WHAT: Converts accumulated eligibility to weights
 * WHY:  Complete the three-factor learning rule
 * HOW:  Applies trace × coherence × phase_bias to weights
 *
 * @param bridge  Bridge handle
 * @param event   Output consolidation event (if any)
 * @return Number of traces converted, -1 on error
 */
NIMCP_EXPORT int ephaptic_plasticity_convert_traces(
    ephaptic_plasticity_bridge_t* bridge,
    ephaptic_plasticity_consol_event_t* event
);

//=============================================================================
// Consolidation API
//=============================================================================

/**
 * @brief Check for consolidation trigger
 *
 * WHAT: Determines if consolidation should occur
 * WHY:  High coherence events trigger memory consolidation
 * HOW:  Monitors coherence for threshold crossing
 *
 * @param bridge    Bridge handle
 * @param coherence Current coherence level
 * @return true if consolidation should trigger
 */
NIMCP_EXPORT bool ephaptic_plasticity_should_consolidate(
    ephaptic_plasticity_bridge_t* bridge,
    float coherence
);

/**
 * @brief Trigger consolidation event
 *
 * WHAT: Forces consolidation of eligible traces
 * WHY:  External trigger for consolidation (e.g., sync burst)
 * HOW:  Converts all ready traces immediately
 *
 * @param bridge  Bridge handle
 * @param event   Output consolidation event
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_plasticity_trigger_consolidation(
    ephaptic_plasticity_bridge_t* bridge,
    ephaptic_plasticity_consol_event_t* event
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Decay traces, check consolidation triggers, update stats
 * HOW:  Called during simulation step
 *
 * @param bridge     Bridge handle
 * @param dt_ms      Time step in milliseconds
 * @param coherence  Current ephaptic coherence
 * @param lfp_phase  Current LFP phase
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_plasticity_update(
    ephaptic_plasticity_bridge_t* bridge,
    float dt_ms,
    float coherence,
    float lfp_phase
);

/**
 * @brief Set coherence gate threshold
 *
 * @param bridge    Bridge handle
 * @param threshold New threshold [0,1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_plasticity_set_coherence_threshold(
    ephaptic_plasticity_bridge_t* bridge,
    float threshold
);

/**
 * @brief Set preferred phases for LTP/LTD
 *
 * @param bridge    Bridge handle
 * @param ltp_phase LTP preferred phase (radians)
 * @param ltd_phase LTD preferred phase (radians)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_plasticity_set_phase_preferences(
    ephaptic_plasticity_bridge_t* bridge,
    float ltp_phase,
    float ltd_phase
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
NIMCP_EXPORT int ephaptic_plasticity_get_stats(
    const ephaptic_plasticity_bridge_t* bridge,
    ephaptic_plasticity_stats_t* stats
);

/**
 * @brief Get current plasticity state
 *
 * @param bridge              Bridge handle
 * @param plasticity_enabled  Output: is plasticity currently enabled
 * @param ltp_bias            Output: current LTP bias factor
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ephaptic_plasticity_get_state(
    const ephaptic_plasticity_bridge_t* bridge,
    bool* plasticity_enabled,
    float* ltp_bias
);

/**
 * @brief Get trace count
 *
 * @param bridge Bridge handle
 * @return Number of active traces, or 0 on error
 */
NIMCP_EXPORT uint32_t ephaptic_plasticity_get_trace_count(
    const ephaptic_plasticity_bridge_t* bridge
);

/**
 * @brief Check if bridge is active
 *
 * @param bridge Bridge handle
 * @return true if bridge is active
 */
NIMCP_EXPORT bool ephaptic_plasticity_is_active(
    const ephaptic_plasticity_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EPHAPTIC_PLASTICITY_BRIDGE_H */