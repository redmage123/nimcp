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
// nimcp_nvc_thalamic_bridge.h - Neurovascular Coupling to Thalamic Gating Bridge
//=============================================================================
/**
 * @file nimcp_nvc_thalamic_bridge.h
 * @brief Bridge between Neurovascular Coupling and Thalamic Gating mechanisms
 *
 * WHAT: Connects hemodynamic state with thalamic gating functions, enabling
 *       blood flow to modulate sensory relay and attentional filtering.
 *
 * WHY:  The thalamus is metabolically demanding and blood flow affects:
 *       - Sensory gating fidelity
 *       - Attentional spotlight maintenance
 *       - Cortico-thalamic feedback loops
 *       - Sleep/wake state transitions
 *       - Arousal and vigilance levels
 *
 * HOW:  Metabolic modulation of thalamic function:
 *       1. CBF → Gating precision and signal-to-noise
 *       2. O2 levels → Relay neuron firing fidelity
 *       3. Metabolic stress → Reduced gating selectivity
 *       4. BOLD signal → Thalamic engagement indicator
 *
 * BIOLOGICAL BASIS:
 * ```
 * NEUROVASCULAR                           THALAMIC GATING
 * ─────────────────────────────────────────────────────────────────
 * Thalamic CBF                      → Relay transmission fidelity
 * Pulvinar perfusion                → Attentional filtering strength
 * Reticular nucleus O2              → Inhibitory gating precision
 * Metabolic state                   → Burst vs tonic mode bias
 * Adenosine accumulation            → Sleep pressure on gating
 * NO from thalamic vessels          → Local neuromodulation
 * ```
 *
 * THALAMIC NUCLEI CONSIDERATIONS:
 * - LGN/MGN: Sensory relay, requires high metabolic fidelity
 * - Pulvinar: Attentional modulation, high cortical connectivity
 * - TRN: Inhibitory gating, regulates all relay nuclei
 * - Intralaminar: Arousal, consciousness-related
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_NVC_THALAMIC_BRIDGE_H
#define NIMCP_NVC_THALAMIC_BRIDGE_H

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
#define NVC_THALAMIC_MODULE_NAME        "nvc_thalamic_bridge"

/** Maximum thalamic nuclei tracked */
#define NVC_THALAMIC_MAX_NUCLEI         16

/** Maximum gating channels */
#define NVC_THALAMIC_MAX_CHANNELS       64

/** Minimum CBF ratio for tonic mode (% of baseline) */
#define NVC_THALAMIC_TONIC_CBF_MIN      0.8f

/** CBF threshold for burst mode transition */
#define NVC_THALAMIC_BURST_CBF_THRESH   0.6f

/** Adenosine accumulation rate under hypoperfusion */
#define NVC_THALAMIC_ADENOSINE_RATE     0.01f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Thalamic nucleus type
 */
typedef enum {
    NVC_THALAMIC_LGN = 0,                /**< Lateral geniculate (visual) */
    NVC_THALAMIC_MGN,                    /**< Medial geniculate (auditory) */
    NVC_THALAMIC_VPN,                    /**< Ventral posterior (somatosensory) */
    NVC_THALAMIC_PULVINAR,               /**< Pulvinar (attention) */
    NVC_THALAMIC_TRN,                    /**< Thalamic reticular (inhibition) */
    NVC_THALAMIC_INTRALAMINAR,           /**< Intralaminar (arousal) */
    NVC_THALAMIC_MEDIODORSAL,            /**< Mediodorsal (prefrontal) */
    NVC_THALAMIC_ANTERIOR                /**< Anterior (memory) */
} nvc_thalamic_nucleus_t;

/**
 * @brief Thalamic firing mode
 */
typedef enum {
    NVC_THALAMIC_MODE_TONIC = 0,         /**< Faithful relay mode */
    NVC_THALAMIC_MODE_BURST,             /**< Burst firing mode */
    NVC_THALAMIC_MODE_OSCILLATORY,       /**< Rhythmic oscillations */
    NVC_THALAMIC_MODE_SUPPRESSED         /**< Metabolically suppressed */
} nvc_thalamic_mode_t;

/**
 * @brief Gating state
 */
typedef enum {
    NVC_GATE_OPEN = 0,                   /**< Full transmission */
    NVC_GATE_PARTIAL,                    /**< Reduced transmission */
    NVC_GATE_FILTERED,                   /**< Selective transmission */
    NVC_GATE_CLOSED                      /**< Transmission blocked */
} nvc_thalamic_gate_state_t;

/**
 * @brief Arousal level (for intralaminar)
 */
typedef enum {
    NVC_AROUSAL_DEEP_SLEEP = 0,
    NVC_AROUSAL_LIGHT_SLEEP,
    NVC_AROUSAL_DROWSY,
    NVC_AROUSAL_RELAXED,
    NVC_AROUSAL_ALERT,
    NVC_AROUSAL_HYPERVIGILANT
} nvc_thalamic_arousal_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for NVC-Thalamic bridge
 */
typedef struct {
    /** Mode transition thresholds */
    float tonic_cbf_threshold;           /**< CBF for tonic mode (% baseline) */
    float burst_cbf_threshold;           /**< CBF for burst transition */
    float suppression_cbf_threshold;     /**< CBF for suppression */

    /** Gating parameters */
    float gating_precision_scale;        /**< Metabolic effect on precision */
    float snr_metabolic_scale;           /**< S/N ratio metabolic scaling */

    /** Adenosine accumulation */
    bool enable_adenosine_accumulation;  /**< Track sleep pressure */
    float adenosine_accumulation_rate;   /**< Rate under hypoperfusion */
    float adenosine_clearance_rate;      /**< Clearance under normal perfusion */
    float adenosine_gating_threshold;    /**< Threshold for gating effects */

    /** TRN modulation */
    bool enable_trn_modulation;          /**< TRN affects all relay nuclei */
    float trn_inhibition_strength;       /**< TRN inhibition magnitude */

    /** Arousal coupling */
    bool couple_arousal_to_cbf;          /**< CBF affects arousal level */
    float arousal_cbf_sensitivity;       /**< Arousal sensitivity to CBF */

    /** Cortico-thalamic feedback */
    bool enable_cortical_feedback;       /**< Cortical modulation of thalamus */
    float feedback_delay_ms;             /**< Feedback loop delay */

    /** Update parameters */
    float update_interval_ms;            /**< Bridge update interval */
} nvc_thalamic_config_t;

/**
 * @brief Nucleus state
 */
typedef struct {
    nvc_thalamic_nucleus_t nucleus_type; /**< Nucleus type */
    uint32_t nucleus_id;                 /**< Nucleus ID */
    uint32_t nvc_unit_id;                /**< Mapped NVC unit */

    /** Metabolic state */
    float cbf_ratio;                     /**< Current CBF/baseline */
    float oxygen_level;                  /**< Local O2 (0-1) */
    float adenosine_level;               /**< Accumulated adenosine (0-1) */

    /** Firing mode */
    nvc_thalamic_mode_t mode;            /**< Current firing mode */
    float mode_stability;                /**< Stability of current mode */

    /** Gating state */
    nvc_thalamic_gate_state_t gate;      /**< Current gate state */
    float gating_precision;              /**< Gating precision (0-1) */
    float signal_to_noise;               /**< S/N ratio (metabolic-scaled) */

    /** Transmission metrics */
    float relay_fidelity;                /**< Transmission fidelity (0-1) */
    float transmission_gain;             /**< Effective gain */
    float filtering_selectivity;         /**< Selectivity of filtering */
} nvc_thalamic_nucleus_state_t;

/**
 * @brief Gating channel state (for specific sensory/attention channel)
 */
typedef struct {
    uint32_t channel_id;                 /**< Channel identifier */
    uint32_t source_nucleus;             /**< Source nucleus ID */
    uint32_t target_cortical_id;         /**< Target cortical region */

    /** Gating parameters */
    nvc_thalamic_gate_state_t state;     /**< Current gate state */
    float gate_opening;                  /**< Gate opening (0=closed, 1=open) */
    float attention_weight;              /**< Attentional weight (0-1) */

    /** Metabolic modulation */
    float metabolic_factor;              /**< Metabolic modulation (0-1) */
    float effective_transmission;        /**< Net transmission strength */
} nvc_thalamic_channel_state_t;

/**
 * @brief Arousal state (intralaminar nuclei)
 */
typedef struct {
    nvc_thalamic_arousal_t level;        /**< Current arousal level */
    float arousal_drive;                 /**< Arousal drive (0-1) */
    float sleep_pressure;                /**< Sleep pressure from adenosine */
    float cbf_contribution;              /**< CBF contribution to arousal */
    float metabolic_sustainability;      /**< How long arousal sustainable */
} nvc_thalamic_arousal_state_t;

/**
 * @brief TRN modulation state
 */
typedef struct {
    float inhibition_strength;           /**< Current inhibition (0-1) */
    float metabolic_modulation;          /**< Metabolic effect on TRN */
    float selectivity;                   /**< Spatial selectivity */
    uint32_t* modulated_nuclei;          /**< Array of affected nuclei */
    uint32_t modulated_count;            /**< Number of affected nuclei */
} nvc_thalamic_trn_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t updates;                    /**< Total update calls */
    uint64_t mode_transitions;           /**< Tonic/burst transitions */
    uint64_t gate_changes;               /**< Gate state changes */
    uint64_t arousal_changes;            /**< Arousal level changes */

    /** Averages */
    float mean_relay_fidelity;           /**< Mean transmission fidelity */
    float mean_gating_precision;         /**< Mean gating precision */
    float mean_adenosine;                /**< Mean adenosine level */

    /** Mode distribution */
    float time_in_tonic_pct;             /**< % time in tonic mode */
    float time_in_burst_pct;             /**< % time in burst mode */
    float time_suppressed_pct;           /**< % time suppressed */

    float last_update_ms;                /**< Last update timestamp */
} nvc_thalamic_stats_t;

/** Opaque bridge handle */
typedef struct nvc_thalamic_bridge_struct nvc_thalamic_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_thalamic_default_config(nvc_thalamic_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create NVC-Thalamic bridge
 *
 * WHAT: Allocates and initializes the bridge structure
 * WHY:  Establishes metabolic coupling for thalamic gating
 * HOW:  Creates internal state for nucleus tracking
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT nvc_thalamic_bridge_t* nvc_thalamic_bridge_create(
    const nvc_thalamic_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void nvc_thalamic_bridge_destroy(nvc_thalamic_bridge_t* bridge);

//=============================================================================
// Nucleus Management API
//=============================================================================

/**
 * @brief Register thalamic nucleus with NVC unit
 *
 * WHAT: Maps thalamic nucleus to vascular territory
 * WHY:  Enables local metabolic modulation of gating
 * HOW:  Creates nucleus entry with NVC mapping
 *
 * @param bridge Bridge handle
 * @param nucleus_type Type of thalamic nucleus
 * @param nvc_unit_id NVC unit ID
 * @param nucleus_id Output nucleus ID
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_thalamic_register_nucleus(
    nvc_thalamic_bridge_t* bridge,
    nvc_thalamic_nucleus_t nucleus_type,
    uint32_t nvc_unit_id,
    uint32_t* nucleus_id
);

/**
 * @brief Unregister nucleus
 *
 * @param bridge Bridge handle
 * @param nucleus_id Nucleus to unregister
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_thalamic_unregister_nucleus(
    nvc_thalamic_bridge_t* bridge,
    uint32_t nucleus_id
);

/**
 * @brief Create gating channel
 *
 * WHAT: Creates specific gating channel through nucleus
 * WHY:  Enables fine-grained control of sensory/attention streams
 * HOW:  Associates channel with nucleus and cortical target
 *
 * @param bridge Bridge handle
 * @param source_nucleus Source nucleus ID
 * @param target_cortical Target cortical region ID
 * @param channel_id Output channel ID
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_thalamic_create_channel(
    nvc_thalamic_bridge_t* bridge,
    uint32_t source_nucleus,
    uint32_t target_cortical,
    uint32_t* channel_id
);

//=============================================================================
// Metabolic State API
//=============================================================================

/**
 * @brief Update metabolic state from NVC
 *
 * WHAT: Receives blood flow state for nucleus
 * WHY:  Updates firing mode and gating precision
 * HOW:  Converts CBF/OEF to thalamic state changes
 *
 * @param bridge Bridge handle
 * @param nvc_unit_id NVC unit providing state
 * @param cbf Cerebral blood flow
 * @param cbf_baseline Baseline CBF
 * @param oef Oxygen extraction fraction
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_thalamic_update_from_nvc(
    nvc_thalamic_bridge_t* bridge,
    uint32_t nvc_unit_id,
    float cbf,
    float cbf_baseline,
    float oef
);

/**
 * @brief Get nucleus state
 *
 * @param bridge Bridge handle
 * @param nucleus_id Nucleus to query
 * @param state Output nucleus state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_thalamic_get_nucleus_state(
    const nvc_thalamic_bridge_t* bridge,
    uint32_t nucleus_id,
    nvc_thalamic_nucleus_state_t* state
);

//=============================================================================
// Gating API
//=============================================================================

/**
 * @brief Get gating precision for nucleus
 *
 * WHAT: Returns metabolically-scaled gating precision
 * WHY:  Low CBF reduces gating selectivity
 * HOW:  Derived from metabolic state
 *
 * @param bridge Bridge handle
 * @param nucleus_id Nucleus to query
 * @return Gating precision (0-1), or -1 on error
 */
NIMCP_EXPORT float nvc_thalamic_get_gating_precision(
    const nvc_thalamic_bridge_t* bridge,
    uint32_t nucleus_id
);

/**
 * @brief Get relay fidelity
 *
 * WHAT: Returns transmission fidelity
 * WHY:  Metabolic stress reduces relay accuracy
 * HOW:  Function of O2 and firing mode
 *
 * @param bridge Bridge handle
 * @param nucleus_id Nucleus to query
 * @return Relay fidelity (0-1), or -1 on error
 */
NIMCP_EXPORT float nvc_thalamic_get_relay_fidelity(
    const nvc_thalamic_bridge_t* bridge,
    uint32_t nucleus_id
);

/**
 * @brief Get channel transmission
 *
 * WHAT: Returns effective transmission for channel
 * WHY:  Combines gating state and metabolic modulation
 * HOW:  Product of gate opening and metabolic factor
 *
 * @param bridge Bridge handle
 * @param channel_id Channel to query
 * @return Transmission strength (0-1), or -1 on error
 */
NIMCP_EXPORT float nvc_thalamic_get_channel_transmission(
    const nvc_thalamic_bridge_t* bridge,
    uint32_t channel_id
);

/**
 * @brief Set attention weight for channel
 *
 * WHAT: Modulates gating for attentional selection
 * WHY:  Attention affects channel transmission
 * HOW:  Combines with metabolic state for net effect
 *
 * @param bridge Bridge handle
 * @param channel_id Channel to modulate
 * @param attention_weight Attention weight (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_thalamic_set_attention_weight(
    nvc_thalamic_bridge_t* bridge,
    uint32_t channel_id,
    float attention_weight
);

//=============================================================================
// Firing Mode API
//=============================================================================

/**
 * @brief Get current firing mode
 *
 * WHAT: Returns current tonic/burst mode
 * WHY:  Mode affects transmission properties
 * HOW:  Determined by metabolic state
 *
 * @param bridge Bridge handle
 * @param nucleus_id Nucleus to query
 * @return Firing mode enum
 */
NIMCP_EXPORT nvc_thalamic_mode_t nvc_thalamic_get_mode(
    const nvc_thalamic_bridge_t* bridge,
    uint32_t nucleus_id
);

/**
 * @brief Check if nucleus supports tonic mode
 *
 * WHAT: Checks if metabolic state permits tonic firing
 * WHY:  Tonic mode requires adequate CBF
 * HOW:  Compares CBF to tonic threshold
 *
 * @param bridge Bridge handle
 * @param nucleus_id Nucleus to check
 * @return true if tonic mode possible
 */
NIMCP_EXPORT bool nvc_thalamic_can_sustain_tonic(
    const nvc_thalamic_bridge_t* bridge,
    uint32_t nucleus_id
);

//=============================================================================
// Arousal API
//=============================================================================

/**
 * @brief Get arousal state
 *
 * WHAT: Returns intralaminar-based arousal state
 * WHY:  Arousal affects global thalamic function
 * HOW:  Derived from intralaminar metabolic state
 *
 * @param bridge Bridge handle
 * @param arousal Output arousal state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_thalamic_get_arousal_state(
    const nvc_thalamic_bridge_t* bridge,
    nvc_thalamic_arousal_state_t* arousal
);

/**
 * @brief Get sleep pressure
 *
 * WHAT: Returns accumulated adenosine-based sleep pressure
 * WHY:  Sleep pressure affects thalamic gating
 * HOW:  Integrated from hypoperfusion periods
 *
 * @param bridge Bridge handle
 * @return Sleep pressure (0-1)
 */
NIMCP_EXPORT float nvc_thalamic_get_sleep_pressure(
    const nvc_thalamic_bridge_t* bridge
);

//=============================================================================
// TRN API
//=============================================================================

/**
 * @brief Get TRN modulation state
 *
 * WHAT: Returns TRN inhibitory modulation state
 * WHY:  TRN controls relay nucleus gating
 * HOW:  Provides current TRN inhibition strength
 *
 * @param bridge Bridge handle
 * @param trn Output TRN state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_thalamic_get_trn_state(
    const nvc_thalamic_bridge_t* bridge,
    nvc_thalamic_trn_state_t* trn
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of thalamic dynamics
 * WHY:  Mode transitions, adenosine accumulation, gating updates
 * HOW:  Called during simulation timestep
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_thalamic_update(
    nvc_thalamic_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_thalamic_reset(nvc_thalamic_bridge_t* bridge);

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
NIMCP_EXPORT int nvc_thalamic_get_stats(
    const nvc_thalamic_bridge_t* bridge,
    nvc_thalamic_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NVC_THALAMIC_BRIDGE_H */