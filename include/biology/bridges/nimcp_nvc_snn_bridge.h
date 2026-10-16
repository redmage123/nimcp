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
// nimcp_nvc_snn_bridge.h - Neurovascular Coupling to SNN Bridge
//=============================================================================
/**
 * @file nimcp_nvc_snn_bridge.h
 * @brief Bridge between Neurovascular Coupling and Spiking Neural Networks
 *
 * WHAT: Connects neurovascular dynamics with SNN metabolic constraints,
 *       enabling blood flow to gate neural computation.
 *
 * WHY:  Neural activity is metabolically expensive and requires adequate
 *       blood supply. Without sufficient oxygen/glucose, neurons cannot
 *       maintain firing rates or synaptic transmission.
 *
 * HOW:  Bidirectional coupling:
 *       1. NVC → SNN: Blood flow/O2 levels constrain firing capacity
 *       2. SNN → NVC: Spike activity triggers hemodynamic response
 *       3. Metabolic gating prevents unsustainable activity patterns
 *       4. BOLD signal correlates with SNN population activity
 *
 * BIOLOGICAL BASIS:
 * ```
 * NEUROVASCULAR                           SNN CONSTRAINTS
 * ─────────────────────────────────────────────────────────────────
 * Cerebral Blood Flow (CBF)         → Maximum sustainable firing rate
 * Oxygen Extraction Fraction (OEF)  → Synaptic transmission efficacy
 * Glucose delivery                  → ATP for ion pumps (Na+/K+-ATPase)
 * Astrocyte calcium waves           → Network synchronization signal
 * Vasoactive signals (NO, K+)       → Activity-dependent modulation
 * BOLD signal                       ← Population firing rate integral
 * ```
 *
 * METABOLIC CONSTRAINTS:
 * - Each spike costs ~10^9 ATP molecules
 * - Na+/K+-ATPase consumes 50-70% of brain energy
 * - Sustained high firing requires CBF increase
 * - Hypoperfusion leads to firing rate collapse
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_NVC_SNN_BRIDGE_H
#define NIMCP_NVC_SNN_BRIDGE_H

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
#define NVC_SNN_MODULE_NAME             "nvc_snn_bridge"

/** Maximum SNN populations tracked */
#define NVC_SNN_MAX_POPULATIONS         64

/** Maximum NVC units mapped */
#define NVC_SNN_MAX_NVC_UNITS           128

/** Default metabolic scaling factor */
#define NVC_SNN_METABOLIC_SCALE         1.0f

/** Minimum CBF ratio for normal function (% of baseline) */
#define NVC_SNN_MIN_CBF_RATIO           0.5f

/** Critical CBF ratio (ischemic threshold) */
#define NVC_SNN_CRITICAL_CBF_RATIO      0.3f

/** ATP depletion time constant (ms) */
#define NVC_SNN_ATP_TAU                 500.0f

/** Oxygen consumption per spike (arbitrary units) */
#define NVC_SNN_O2_PER_SPIKE            0.001f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Metabolic constraint mode
 */
typedef enum {
    NVC_SNN_CONSTRAINT_SOFT = 0,     /**< Gradual rate reduction */
    NVC_SNN_CONSTRAINT_HARD,         /**< Sharp cutoff at threshold */
    NVC_SNN_CONSTRAINT_ADAPTIVE      /**< Activity-dependent threshold */
} nvc_snn_constraint_mode_t;

/**
 * @brief Perfusion state for SNN region
 */
typedef enum {
    NVC_SNN_PERFUSION_NORMAL = 0,    /**< Adequate blood supply */
    NVC_SNN_PERFUSION_STRESSED,      /**< Elevated demand, near limit */
    NVC_SNN_PERFUSION_COMPROMISED,   /**< Reduced function */
    NVC_SNN_PERFUSION_CRITICAL       /**< Imminent failure */
} nvc_snn_perfusion_state_t;

/**
 * @brief Coupling direction
 */
typedef enum {
    NVC_SNN_COUPLING_NVC_TO_SNN = 0, /**< Blood flow → firing constraints */
    NVC_SNN_COUPLING_SNN_TO_NVC,     /**< Activity → hemodynamic response */
    NVC_SNN_COUPLING_BIDIRECTIONAL   /**< Full coupling */
} nvc_snn_coupling_dir_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for NVC-SNN bridge
 */
typedef struct {
    /** Coupling parameters */
    nvc_snn_coupling_dir_t coupling_direction;
    nvc_snn_constraint_mode_t constraint_mode;

    /** Metabolic constraint parameters */
    float metabolic_scale;               /**< Overall metabolic scaling */
    float min_cbf_ratio;                 /**< Min CBF for normal function */
    float critical_cbf_ratio;            /**< Ischemic threshold */
    float atp_depletion_tau_ms;          /**< ATP depletion time constant */

    /** Activity-dependent coupling */
    float o2_per_spike;                  /**< O2 consumed per spike */
    float glucose_per_spike;             /**< Glucose consumed per spike */
    float cbf_increase_per_hz;           /**< CBF increase per Hz firing */

    /** Rate limiting */
    float max_rate_reduction;            /**< Maximum rate reduction (0-1) */
    float rate_recovery_tau_ms;          /**< Recovery time constant */

    /** Synchronization */
    bool enable_astrocyte_sync;          /**< Astrocyte calcium → SNN sync */
    float sync_coupling_strength;        /**< Synchronization strength */

    /** BOLD integration */
    bool compute_bold_correlation;       /**< Track BOLD-activity correlation */
    float bold_integration_window_ms;    /**< Integration window for BOLD */

    /** Update parameters */
    float update_interval_ms;            /**< Bridge update interval */
} nvc_snn_config_t;

/**
 * @brief Metabolic state for SNN population
 */
typedef struct {
    uint32_t population_id;              /**< SNN population ID */
    uint32_t nvc_unit_id;                /**< Mapped NVC unit ID */

    /** Current metabolic state */
    float atp_level;                     /**< Available ATP (0-1) */
    float oxygen_level;                  /**< Local O2 concentration (0-1) */
    float glucose_level;                 /**< Local glucose (0-1) */

    /** Derived constraints */
    float max_firing_rate;               /**< Current maximum rate (Hz) */
    float synaptic_efficacy;             /**< Transmission efficiency (0-1) */
    float rate_reduction_factor;         /**< Current rate reduction (0-1) */

    /** Perfusion state */
    nvc_snn_perfusion_state_t perfusion_state;
    float cbf_ratio;                     /**< Current CBF/baseline ratio */
    float oef;                           /**< Oxygen extraction fraction */

    /** Activity feedback */
    float mean_firing_rate;              /**< Current population rate (Hz) */
    float integrated_activity;           /**< Integrated recent activity */
    float bold_correlation;              /**< Correlation with BOLD */
} nvc_snn_metabolic_state_t;

/**
 * @brief Hemodynamic feedback from SNN activity
 */
typedef struct {
    uint32_t population_id;              /**< Source population */
    float firing_rate_hz;                /**< Mean firing rate */
    float requested_cbf_increase;        /**< Requested CBF increase (%) */
    float neural_activity_level;         /**< Normalized activity (0-1) */
    float vasoactive_signal;             /**< Net vasoactive signal */
} nvc_snn_activity_feedback_t;

/**
 * @brief Astrocyte synchronization signal
 */
typedef struct {
    float calcium_wave;                  /**< Astrocyte Ca2+ wave amplitude */
    float wave_velocity;                 /**< Propagation velocity */
    float sync_phase;                    /**< Current phase for SNN sync */
    float sync_strength;                 /**< Synchronization strength */
} nvc_snn_astrocyte_sync_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t updates;                    /**< Total update calls */
    uint64_t metabolic_events;           /**< Metabolic constraint events */
    uint64_t perfusion_warnings;         /**< Perfusion warnings issued */
    uint64_t critical_events;            /**< Critical perfusion events */

    /** Averages */
    float mean_atp_level;                /**< Mean ATP across populations */
    float mean_rate_reduction;           /**< Mean rate reduction applied */
    float mean_bold_correlation;         /**< Mean BOLD-activity correlation */

    /** Extremes */
    float min_atp_level;                 /**< Minimum ATP observed */
    float max_rate_reduction;            /**< Maximum rate reduction */

    /** Activity */
    uint32_t populations_constrained;    /**< Populations with rate limits */
    uint32_t populations_critical;       /**< Populations in critical state */

    float last_update_ms;                /**< Last update timestamp */
} nvc_snn_stats_t;

/** Opaque bridge handle */
typedef struct nvc_snn_bridge_struct nvc_snn_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_snn_default_config(nvc_snn_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create NVC-SNN bridge
 *
 * WHAT: Allocates and initializes the bridge structure
 * WHY:  Establishes metabolic coupling between blood flow and neural activity
 * HOW:  Creates internal state for tracking metabolic constraints
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT nvc_snn_bridge_t* nvc_snn_bridge_create(
    const nvc_snn_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void nvc_snn_bridge_destroy(nvc_snn_bridge_t* bridge);

//=============================================================================
// Mapping API
//=============================================================================

/**
 * @brief Map SNN population to NVC unit
 *
 * WHAT: Establishes spatial correspondence between SNN and NVC
 * WHY:  Enables local metabolic constraints for specific populations
 * HOW:  Creates bidirectional mapping for constraint propagation
 *
 * @param bridge Bridge handle
 * @param population_id SNN population ID
 * @param nvc_unit_id NVC unit ID
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_snn_map_population(
    nvc_snn_bridge_t* bridge,
    uint32_t population_id,
    uint32_t nvc_unit_id
);

/**
 * @brief Remove population mapping
 *
 * @param bridge Bridge handle
 * @param population_id Population to unmap
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_snn_unmap_population(
    nvc_snn_bridge_t* bridge,
    uint32_t population_id
);

//=============================================================================
// NVC → SNN API (Metabolic Constraints)
//=============================================================================

/**
 * @brief Update metabolic state from NVC
 *
 * WHAT: Receives blood flow state from NVC system
 * WHY:  Updates metabolic constraints for SNN computation
 * HOW:  Converts CBF/OEF to ATP/O2 levels and firing constraints
 *
 * @param bridge Bridge handle
 * @param nvc_unit_id NVC unit providing state
 * @param cbf Cerebral blood flow (mL/100g/min)
 * @param cbf_baseline Baseline CBF
 * @param oef Oxygen extraction fraction
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_snn_update_from_nvc(
    nvc_snn_bridge_t* bridge,
    uint32_t nvc_unit_id,
    float cbf,
    float cbf_baseline,
    float oef
);

/**
 * @brief Get metabolic state for population
 *
 * WHAT: Retrieves current metabolic constraints
 * WHY:  SNN uses this to limit firing rates
 * HOW:  Returns derived constraints from blood flow state
 *
 * @param bridge Bridge handle
 * @param population_id Population to query
 * @param state Output metabolic state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_snn_get_metabolic_state(
    const nvc_snn_bridge_t* bridge,
    uint32_t population_id,
    nvc_snn_metabolic_state_t* state
);

/**
 * @brief Get firing rate limit for population
 *
 * WHAT: Returns maximum sustainable firing rate
 * WHY:  SNN should not exceed this rate
 * HOW:  Derived from oxygen/glucose availability
 *
 * @param bridge Bridge handle
 * @param population_id Population to query
 * @return Maximum firing rate (Hz), or -1 on error
 */
NIMCP_EXPORT float nvc_snn_get_rate_limit(
    const nvc_snn_bridge_t* bridge,
    uint32_t population_id
);

/**
 * @brief Get synaptic efficacy factor
 *
 * WHAT: Returns transmission efficiency
 * WHY:  Low ATP reduces synaptic vesicle recycling
 * HOW:  Scales with available metabolic resources
 *
 * @param bridge Bridge handle
 * @param population_id Population to query
 * @return Synaptic efficacy (0-1), or -1 on error
 */
NIMCP_EXPORT float nvc_snn_get_synaptic_efficacy(
    const nvc_snn_bridge_t* bridge,
    uint32_t population_id
);

//=============================================================================
// SNN → NVC API (Activity Feedback)
//=============================================================================

/**
 * @brief Report SNN population activity
 *
 * WHAT: Sends firing rate to NVC for hemodynamic response
 * WHY:  High activity triggers blood flow increase
 * HOW:  Converts firing rate to vasoactive signal
 *
 * @param bridge Bridge handle
 * @param population_id Population reporting
 * @param firing_rate_hz Mean firing rate
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_snn_report_activity(
    nvc_snn_bridge_t* bridge,
    uint32_t population_id,
    float firing_rate_hz
);

/**
 * @brief Get activity feedback for NVC unit
 *
 * WHAT: Retrieves aggregated activity for NVC update
 * WHY:  NVC uses this to drive hemodynamic response
 * HOW:  Combines activity from all mapped populations
 *
 * @param bridge Bridge handle
 * @param nvc_unit_id NVC unit to query
 * @param feedback Output feedback structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_snn_get_activity_feedback(
    const nvc_snn_bridge_t* bridge,
    uint32_t nvc_unit_id,
    nvc_snn_activity_feedback_t* feedback
);

//=============================================================================
// Astrocyte Synchronization API
//=============================================================================

/**
 * @brief Update astrocyte synchronization signal
 *
 * WHAT: Receives astrocyte calcium wave from NVC
 * WHY:  Astrocytes modulate network synchronization
 * HOW:  Converts calcium wave to SNN synchronization signal
 *
 * @param bridge Bridge handle
 * @param calcium_wave Astrocyte Ca2+ wave amplitude
 * @param wave_velocity Propagation velocity
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_snn_update_astrocyte_sync(
    nvc_snn_bridge_t* bridge,
    float calcium_wave,
    float wave_velocity
);

/**
 * @brief Get current synchronization signal for SNN
 *
 * @param bridge Bridge handle
 * @param sync Output synchronization structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_snn_get_sync_signal(
    const nvc_snn_bridge_t* bridge,
    nvc_snn_astrocyte_sync_t* sync
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of metabolic dynamics
 * WHY:  ATP depletion, recovery, state transitions
 * HOW:  Called during simulation timestep
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_snn_update(
    nvc_snn_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_snn_reset(nvc_snn_bridge_t* bridge);

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
NIMCP_EXPORT int nvc_snn_get_stats(
    const nvc_snn_bridge_t* bridge,
    nvc_snn_stats_t* stats
);

/**
 * @brief Get perfusion state for population
 *
 * @param bridge Bridge handle
 * @param population_id Population to query
 * @return Perfusion state enum
 */
NIMCP_EXPORT nvc_snn_perfusion_state_t nvc_snn_get_perfusion_state(
    const nvc_snn_bridge_t* bridge,
    uint32_t population_id
);

/**
 * @brief Check if any population is in critical state
 *
 * @param bridge Bridge handle
 * @return true if any population is critical
 */
NIMCP_EXPORT bool nvc_snn_has_critical_perfusion(
    const nvc_snn_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NVC_SNN_BRIDGE_H */