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
// nimcp_no_thalamic_bridge.h - Nitric Oxide to Thalamic Attention Gating Bridge
//=============================================================================
/**
 * @file nimcp_no_thalamic_bridge.h
 * @brief Bidirectional bridge between Nitric Oxide signaling and thalamic
 *        attention gating mechanisms
 *
 * WHAT: Connects NO gasotransmitter signaling with thalamic relay and
 *       reticular nucleus for attention and sensory gating.
 *
 * WHY:  Nitric oxide modulates thalamic function in attention-relevant ways:
 *       - NO in thalamus modulates relay neuron excitability
 *       - eNOS activity affects thalamic blood flow (metabolic gating)
 *       - NO influences TRN (thalamic reticular nucleus) inhibition
 *       - Volume transmission enables rapid attentional state changes
 *
 * HOW:  Bidirectional integration:
 *       1. Attention → NO: Attentional demands trigger thalamic NO release
 *       2. NO → Gating: NO modulates thalamic relay gain
 *       3. NO → TRN: NO affects reticular inhibitory gating
 *       4. Vascular: NO-mediated blood flow supports attentional focus
 *
 * BIOLOGICAL BASIS:
 * ```
 * THALAMIC ATTENTION                       NO SIGNALING
 * ─────────────────────────────────────────────────────────────────
 * Relay neuron depolarization           → Ca2+ influx → nNOS activation
 * Attentional focus (cortical feedback) → Sustained NO production
 * TRN inhibitory gating                 ← NO modulates GABA release
 * Sensory filtering                     ← NO adjusts relay threshold
 * Metabolic allocation                  ← eNOS vasodilation
 * ```
 *
 * THALAMIC NUCLEI AFFECTED:
 * - LGN (Lateral Geniculate): Visual attention
 * - MGN (Medial Geniculate): Auditory attention
 * - VPL/VPM (Ventral Posterior): Somatosensory attention
 * - Pulvinar: High-order attention coordination
 * - TRN: Inhibitory gating of all modalities
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_NO_THALAMIC_BRIDGE_H
#define NIMCP_NO_THALAMIC_BRIDGE_H

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
#define NO_THALAMIC_MODULE_NAME         "no_thalamic_bridge"

/** Maximum thalamic relay units */
#define NO_THALAMIC_MAX_RELAYS          256

/** Maximum TRN units */
#define NO_THALAMIC_MAX_TRN             128

/** Default gain modulation range */
#define NO_THALAMIC_GAIN_MIN            0.2f
#define NO_THALAMIC_GAIN_MAX            2.0f

/** Default NO threshold for attention gating */
#define NO_THALAMIC_ATTENTION_THRESHOLD 30.0f  /* nM */

/** Bio-async module ID */
#define BIO_MODULE_NO_THALAMIC          0x0E05

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Thalamic nucleus type
 */
typedef enum {
    NO_THAL_NUCLEUS_LGN = 0,            /**< Lateral Geniculate (visual) */
    NO_THAL_NUCLEUS_MGN,                /**< Medial Geniculate (auditory) */
    NO_THAL_NUCLEUS_VPL,                /**< Ventral Posterolateral (touch) */
    NO_THAL_NUCLEUS_VPM,                /**< Ventral Posteromedial (face) */
    NO_THAL_NUCLEUS_PULVINAR,           /**< Pulvinar (attention) */
    NO_THAL_NUCLEUS_MD,                 /**< Mediodorsal (executive) */
    NO_THAL_NUCLEUS_TRN,                /**< Thalamic Reticular Nucleus */
    NO_THAL_NUCLEUS_OTHER               /**< Other nuclei */
} no_thalamic_nucleus_t;

/**
 * @brief Attention state influenced by NO
 */
typedef enum {
    NO_THAL_ATTN_UNFOCUSED = 0,         /**< Low attention, high filtering */
    NO_THAL_ATTN_MONITORING,            /**< Ambient monitoring */
    NO_THAL_ATTN_FOCUSED,               /**< Selective attention */
    NO_THAL_ATTN_HYPERFOCUSED           /**< Intense focus (high NO) */
} no_thalamic_attn_state_t;

/**
 * @brief Gating mode for sensory relay
 */
typedef enum {
    NO_THAL_GATE_CLOSED = 0,            /**< Strong TRN inhibition */
    NO_THAL_GATE_PARTIAL,               /**< Filtered relay */
    NO_THAL_GATE_OPEN,                  /**< Full relay */
    NO_THAL_GATE_ENHANCED               /**< Amplified relay */
} no_thalamic_gate_mode_t;

/**
 * @brief Thalamocortical oscillation mode
 */
typedef enum {
    NO_THAL_OSC_BURST = 0,              /**< Burst mode (low attention) */
    NO_THAL_OSC_TONIC,                  /**< Tonic mode (high attention) */
    NO_THAL_OSC_MIXED                   /**< Mixed mode */
} no_thalamic_osc_mode_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for NO-Thalamic bridge
 */
typedef struct {
    /** Gain modulation */
    float gain_min;                          /**< Minimum relay gain */
    float gain_max;                          /**< Maximum relay gain */
    float no_to_gain_scale;                  /**< NO concentration to gain */
    float gain_time_const_ms;                /**< Gain adjustment tau */

    /** Attention thresholds */
    float attention_no_threshold_nm;         /**< NO for attention shift */
    float focus_no_threshold_nm;             /**< NO for focused state */
    float hyperfocus_no_threshold_nm;        /**< NO for hyperfocus */

    /** TRN modulation */
    bool enable_trn_modulation;              /**< Enable TRN effects */
    float trn_no_sensitivity;                /**< TRN sensitivity to NO */
    float trn_inhibition_baseline;           /**< Baseline TRN inhibition */

    /** Oscillation mode */
    bool enable_osc_modulation;              /**< Enable burst/tonic switch */
    float burst_no_threshold;                /**< NO threshold for tonic */

    /** Vascular coupling */
    bool enable_vascular;                    /**< Enable blood flow effects */
    float vascular_weight;                   /**< Blood flow contribution */

    /** Features */
    bool enable_bio_async;                   /**< Bio-async messaging */
    float update_interval_ms;                /**< Update interval */
} no_thalamic_config_t;

/**
 * @brief Thalamic relay unit with NO modulation
 */
typedef struct {
    uint32_t relay_id;                       /**< Relay unit ID */
    no_thalamic_nucleus_t nucleus;           /**< Thalamic nucleus */
    float position[3];                       /**< Position (um) */

    /** NO state */
    float local_no_nm;                       /**< Local NO concentration */
    float local_cgmp_um;                     /**< Local cGMP level */
    uint32_t no_source_id;                   /**< Associated NO source */

    /** Gating state */
    no_thalamic_gate_mode_t gate_mode;       /**< Current gating mode */
    float relay_gain;                        /**< Current relay gain */
    float trn_inhibition;                    /**< TRN inhibition level */

    /** Attention state */
    no_thalamic_attn_state_t attn_state;     /**< Attention state */
    float attention_level;                   /**< Attention intensity (0-1) */

    /** Oscillation */
    no_thalamic_osc_mode_t osc_mode;         /**< Burst vs tonic mode */
    float burst_probability;                 /**< Probability of burst firing */

    /** Input/Output */
    float sensory_input;                     /**< Incoming sensory signal */
    float relay_output;                      /**< Gated output to cortex */

    bool active;
} no_thalamic_relay_t;

/**
 * @brief TRN unit with NO modulation
 */
typedef struct {
    uint32_t trn_id;                         /**< TRN unit ID */
    float position[3];                       /**< Position (um) */

    /** NO modulation */
    float local_no_nm;                       /**< Local NO concentration */
    float no_sensitivity;                    /**< Sensitivity to NO */

    /** Inhibitory output */
    float inhibitory_output;                 /**< GABA output level */
    float baseline_inhibition;               /**< Baseline output */

    /** Targets */
    uint32_t target_relays[16];              /**< Target relay units */
    uint32_t num_targets;

    bool active;
} no_thalamic_trn_t;

/**
 * @brief Attention gating event
 */
typedef struct {
    uint32_t relay_id;                       /**< Relay unit ID */
    no_thalamic_nucleus_t nucleus;           /**< Nucleus type */

    /** Before/after state */
    no_thalamic_gate_mode_t prev_gate;       /**< Previous gate mode */
    no_thalamic_gate_mode_t new_gate;        /**< New gate mode */
    float prev_gain;                         /**< Previous gain */
    float new_gain;                          /**< New gain */

    /** Causes */
    float no_level;                          /**< NO at transition */
    float attention_change;                  /**< Attention change */

    float event_time_ms;
} no_thalamic_event_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;                  /**< Total updates */
    uint64_t gate_transitions;               /**< Gate mode changes */
    uint64_t attention_shifts;               /**< Attention state changes */
    uint64_t osc_mode_switches;              /**< Burst/tonic switches */

    uint32_t active_relays;                  /**< Active relay units */
    uint32_t focused_relays;                 /**< Relays in focused state */
    uint32_t open_gates;                     /**< Open gate count */

    float mean_relay_gain;                   /**< Average relay gain */
    float mean_no_level;                     /**< Average NO level */
    float mean_attention;                    /**< Average attention */
    float total_trn_inhibition;              /**< Total TRN output */

    float last_update_ms;
} no_thalamic_stats_t;

/** Opaque bridge handle */
typedef struct no_thalamic_bridge_struct no_thalamic_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_thalamic_default_config(no_thalamic_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create NO-Thalamic bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT no_thalamic_bridge_t* no_thalamic_bridge_create(
    const no_thalamic_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
NIMCP_EXPORT void no_thalamic_bridge_destroy(no_thalamic_bridge_t* bridge);

//=============================================================================
// Relay Management API
//=============================================================================

/**
 * @brief Register thalamic relay unit
 *
 * WHAT: Adds relay unit for NO-attention modulation
 * WHY:  Relay neurons gate sensory information to cortex
 * HOW:  Creates relay with nucleus type and position
 *
 * @param bridge Bridge handle
 * @param relay_id Relay unit ID
 * @param nucleus Thalamic nucleus type
 * @param position 3D position [x, y, z] in um
 * @param no_source_id Associated NO source (or 0)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_thalamic_register_relay(
    no_thalamic_bridge_t* bridge,
    uint32_t relay_id,
    no_thalamic_nucleus_t nucleus,
    const float position[3],
    uint32_t no_source_id
);

/**
 * @brief Unregister thalamic relay
 *
 * @param bridge Bridge handle
 * @param relay_id Relay to remove
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_thalamic_unregister_relay(
    no_thalamic_bridge_t* bridge,
    uint32_t relay_id
);

/**
 * @brief Register TRN unit
 *
 * WHAT: Adds TRN unit for inhibitory gating
 * WHY:  TRN provides inhibitory control of relay neurons
 * HOW:  Creates TRN unit with target relays
 *
 * @param bridge Bridge handle
 * @param trn_id TRN unit ID
 * @param position 3D position
 * @param target_relays Array of target relay IDs
 * @param num_targets Number of targets
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_thalamic_register_trn(
    no_thalamic_bridge_t* bridge,
    uint32_t trn_id,
    const float position[3],
    const uint32_t* target_relays,
    uint32_t num_targets
);

//=============================================================================
// NO -> Gating API
//=============================================================================

/**
 * @brief Set NO level for relay
 *
 * WHAT: Updates NO concentration at relay
 * WHY:  NO modulates relay gain and gating
 * HOW:  Updates relay state, recalculates gating
 *
 * @param bridge Bridge handle
 * @param relay_id Relay ID
 * @param no_level_nm NO concentration (nM)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_thalamic_set_no_level(
    no_thalamic_bridge_t* bridge,
    uint32_t relay_id,
    float no_level_nm
);

/**
 * @brief Get current relay gain
 *
 * WHAT: Returns NO-modulated relay gain
 * WHY:  Gain determines sensory signal amplification
 * HOW:  Calculated from NO level and attention state
 *
 * @param bridge Bridge handle
 * @param relay_id Relay ID
 * @param[out] gain Current relay gain
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_thalamic_get_gain(
    const no_thalamic_bridge_t* bridge,
    uint32_t relay_id,
    float* gain
);

/**
 * @brief Get current gate mode
 *
 * @param bridge Bridge handle
 * @param relay_id Relay ID
 * @return Current gate mode
 */
NIMCP_EXPORT no_thalamic_gate_mode_t no_thalamic_get_gate_mode(
    const no_thalamic_bridge_t* bridge,
    uint32_t relay_id
);

/**
 * @brief Apply gating to sensory input
 *
 * WHAT: Filters sensory signal through NO-modulated gate
 * WHY:  Compute gated output for cortex
 * HOW:  Applies gain and gate mode to input
 *
 * @param bridge Bridge handle
 * @param relay_id Relay ID
 * @param sensory_input Input signal
 * @param[out] gated_output Gated output
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_thalamic_gate_input(
    no_thalamic_bridge_t* bridge,
    uint32_t relay_id,
    float sensory_input,
    float* gated_output
);

//=============================================================================
// Attention State API
//=============================================================================

/**
 * @brief Get attention state for relay
 *
 * @param bridge Bridge handle
 * @param relay_id Relay ID
 * @return Current attention state
 */
NIMCP_EXPORT no_thalamic_attn_state_t no_thalamic_get_attention_state(
    const no_thalamic_bridge_t* bridge,
    uint32_t relay_id
);

/**
 * @brief Get attention level (0-1)
 *
 * @param bridge Bridge handle
 * @param relay_id Relay ID
 * @param[out] attention Attention level
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_thalamic_get_attention_level(
    const no_thalamic_bridge_t* bridge,
    uint32_t relay_id,
    float* attention
);

/**
 * @brief Set cortical feedback (top-down attention)
 *
 * WHAT: Receives top-down attention signal from cortex
 * WHY:  Cortex can direct thalamic attention
 * HOW:  Modulates NO production and gating
 *
 * @param bridge Bridge handle
 * @param relay_id Relay ID
 * @param cortical_attention Cortical attention signal (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_thalamic_set_cortical_feedback(
    no_thalamic_bridge_t* bridge,
    uint32_t relay_id,
    float cortical_attention
);

/**
 * @brief Get nucleus-wide attention summary
 *
 * WHAT: Returns attention state for entire nucleus
 * WHY:  Nucleus-level coordination of attention
 * HOW:  Aggregates across all relays in nucleus
 *
 * @param bridge Bridge handle
 * @param nucleus Thalamic nucleus
 * @param[out] mean_attention Average attention
 * @param[out] mean_gain Average gain
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_thalamic_get_nucleus_attention(
    const no_thalamic_bridge_t* bridge,
    no_thalamic_nucleus_t nucleus,
    float* mean_attention,
    float* mean_gain
);

//=============================================================================
// TRN Modulation API
//=============================================================================

/**
 * @brief Get TRN inhibition level
 *
 * WHAT: Returns NO-modulated TRN inhibitory output
 * WHY:  TRN gates relay neurons via inhibition
 * HOW:  NO modulates GABA release from TRN
 *
 * @param bridge Bridge handle
 * @param trn_id TRN unit ID
 * @param[out] inhibition Inhibitory output level
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_thalamic_get_trn_inhibition(
    const no_thalamic_bridge_t* bridge,
    uint32_t trn_id,
    float* inhibition
);

/**
 * @brief Set NO level for TRN unit
 *
 * @param bridge Bridge handle
 * @param trn_id TRN unit ID
 * @param no_level_nm NO concentration
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_thalamic_set_trn_no_level(
    no_thalamic_bridge_t* bridge,
    uint32_t trn_id,
    float no_level_nm
);

//=============================================================================
// Oscillation Mode API
//=============================================================================

/**
 * @brief Get oscillation mode (burst vs tonic)
 *
 * WHAT: Returns current burst/tonic firing mode
 * WHY:  Mode affects information transfer fidelity
 * HOW:  NO influences mode via membrane properties
 *
 * @param bridge Bridge handle
 * @param relay_id Relay ID
 * @return Current oscillation mode
 */
NIMCP_EXPORT no_thalamic_osc_mode_t no_thalamic_get_osc_mode(
    const no_thalamic_bridge_t* bridge,
    uint32_t relay_id
);

/**
 * @brief Get burst probability
 *
 * @param bridge Bridge handle
 * @param relay_id Relay ID
 * @param[out] burst_prob Burst probability (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_thalamic_get_burst_prob(
    const no_thalamic_bridge_t* bridge,
    uint32_t relay_id,
    float* burst_prob
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of NO-thalamic integration
 * WHY:  Advance gating, attention, and TRN states
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_thalamic_update(
    no_thalamic_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_thalamic_reset(no_thalamic_bridge_t* bridge);

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
NIMCP_EXPORT int no_thalamic_get_stats(
    const no_thalamic_bridge_t* bridge,
    no_thalamic_stats_t* stats
);

/**
 * @brief Get relay state
 *
 * @param bridge Bridge handle
 * @param relay_id Relay ID
 * @param[out] relay Relay state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_thalamic_get_relay(
    const no_thalamic_bridge_t* bridge,
    uint32_t relay_id,
    no_thalamic_relay_t* relay
);

/**
 * @brief Check if relay is in focused attention state
 *
 * @param bridge Bridge handle
 * @param relay_id Relay ID
 * @return true if focused
 */
NIMCP_EXPORT bool no_thalamic_is_focused(
    const no_thalamic_bridge_t* bridge,
    uint32_t relay_id
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NO_THALAMIC_BRIDGE_H */