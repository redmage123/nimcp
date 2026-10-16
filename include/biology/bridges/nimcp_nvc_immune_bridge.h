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
// nimcp_nvc_immune_bridge.h - Neurovascular Coupling to Immune System Bridge
//=============================================================================
/**
 * @file nimcp_nvc_immune_bridge.h
 * @brief Bridge between Neurovascular Coupling and Brain Immune System
 *
 * WHAT: Connects hemodynamic responses with neuroimmune functions, enabling
 *       blood flow changes to modulate immune surveillance and response.
 *
 * WHY:  The brain's immune system is intimately linked to vasculature:
 *       - Microglia respond to perfusion changes
 *       - Blood-brain barrier regulates immune cell entry
 *       - Inflammatory signals affect vascular tone
 *       - Ischemia triggers immune cascades
 *       - Cytokines modulate neurovascular coupling
 *
 * HOW:  Bidirectional coupling:
 *       1. NVC → Immune: Blood flow affects immune cell function
 *       2. Immune → NVC: Inflammatory signals modulate vasculature
 *       3. BBB state gates immune cell infiltration
 *       4. Metabolic stress triggers microglial activation
 *
 * BIOLOGICAL BASIS:
 * ```
 * NEUROVASCULAR                           IMMUNE SYSTEM
 * ─────────────────────────────────────────────────────────────────
 * Hypoperfusion                     → Microglial activation (M1)
 * Reperfusion                       → Phagocytic clearance
 * BBB breakdown                     → Peripheral immune infiltration
 * Adenosine (ischemia)              → Anti-inflammatory signaling
 * NO from vessels                   → Immune modulation
 * CBF recovery                      → Return to surveillance (M2)
 * ─────────────────────────────────────────────────────────────────
 * Pro-inflammatory cytokines        → Vasoconstriction
 * Anti-inflammatory cytokines       → Vascular protection
 * Complement activation             → BBB permeability increase
 * Microglial NO                     → Vasodilation
 * ```
 *
 * IMMUNE CELLS IN CNS:
 * - Microglia: Resident immune cells, surveillance and response
 * - Astrocytes: Glial limitans, inflammatory signaling
 * - Perivascular macrophages: BBB interface, antigen presentation
 * - Infiltrating lymphocytes: Entry via compromised BBB
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_NVC_IMMUNE_BRIDGE_H
#define NIMCP_NVC_IMMUNE_BRIDGE_H

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
#define NVC_IMMUNE_MODULE_NAME          "nvc_immune_bridge"

/** Maximum immune regions tracked */
#define NVC_IMMUNE_MAX_REGIONS          64

/** Maximum active immune responses */
#define NVC_IMMUNE_MAX_RESPONSES        32

/** Hypoperfusion threshold for microglial activation (% baseline) */
#define NVC_IMMUNE_HYPOPERFUSION_THRESH 0.7f

/** Critical ischemia threshold (% baseline) */
#define NVC_IMMUNE_ISCHEMIA_THRESH      0.4f

/** BBB permeability threshold for infiltration */
#define NVC_IMMUNE_BBB_INFILTRATION     0.6f

/** Inflammatory cytokine decay time constant (ms) */
#define NVC_IMMUNE_CYTOKINE_TAU         5000.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Microglial activation state
 */
typedef enum {
    NVC_MICROGLIA_SURVEILLING = 0,       /**< Resting, surveillance mode */
    NVC_MICROGLIA_PRIMED,                /**< Activated but not inflammatory */
    NVC_MICROGLIA_M1,                    /**< Pro-inflammatory phenotype */
    NVC_MICROGLIA_M2,                    /**< Anti-inflammatory/repair */
    NVC_MICROGLIA_DYSTROPHIC             /**< Dysfunctional (chronic stress) */
} nvc_immune_microglia_state_t;

/**
 * @brief Inflammatory state
 */
typedef enum {
    NVC_INFLAMMATION_NONE = 0,           /**< No inflammation */
    NVC_INFLAMMATION_ACUTE,              /**< Acute response */
    NVC_INFLAMMATION_RESOLVING,          /**< Resolution phase */
    NVC_INFLAMMATION_CHRONIC             /**< Chronic inflammation */
} nvc_immune_inflammation_t;

/**
 * @brief BBB immune state
 */
typedef enum {
    NVC_BBB_IMMUNE_INTACT = 0,           /**< Normal barrier, no infiltration */
    NVC_BBB_IMMUNE_PERMEABLE,            /**< Increased permeability */
    NVC_BBB_IMMUNE_INFILTRATING,         /**< Active immune cell entry */
    NVC_BBB_IMMUNE_COMPROMISED           /**< Barrier dysfunction */
} nvc_immune_bbb_state_t;

/**
 * @brief Cytokine type
 */
typedef enum {
    NVC_CYTOKINE_TNF_ALPHA = 0,          /**< TNF-alpha (pro-inflammatory) */
    NVC_CYTOKINE_IL1_BETA,               /**< IL-1beta (pro-inflammatory) */
    NVC_CYTOKINE_IL6,                    /**< IL-6 (dual role) */
    NVC_CYTOKINE_IL10,                   /**< IL-10 (anti-inflammatory) */
    NVC_CYTOKINE_TGFB,                   /**< TGF-beta (anti-inflammatory) */
    NVC_CYTOKINE_COUNT
} nvc_immune_cytokine_t;

/**
 * @brief Vascular effect from immune signals
 */
typedef enum {
    NVC_VASCULAR_EFFECT_NONE = 0,
    NVC_VASCULAR_EFFECT_DILATE,          /**< Vasodilation (anti-inflam) */
    NVC_VASCULAR_EFFECT_CONSTRICT,       /**< Vasoconstriction (pro-inflam) */
    NVC_VASCULAR_EFFECT_PERMEABILIZE     /**< Increase permeability */
} nvc_immune_vascular_effect_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for NVC-Immune bridge
 */
typedef struct {
    /** Microglial activation thresholds */
    float hypoperfusion_activation;      /**< CBF for microglial priming */
    float ischemia_threshold;            /**< CBF for M1 activation */
    float reperfusion_threshold;         /**< CBF for M2 transition */

    /** BBB parameters */
    float bbb_permeability_threshold;    /**< Permeability for infiltration */
    float bbb_recovery_rate;             /**< BBB recovery rate */

    /** Cytokine dynamics */
    float cytokine_decay_tau_ms;         /**< Cytokine decay time constant */
    float cytokine_diffusion_rate;       /**< Spatial diffusion rate */

    /** Vascular effects */
    float vasoconstriction_strength;     /**< Pro-inflam vasoconstriction */
    float vasodilation_strength;         /**< Anti-inflam vasodilation */
    float permeability_increase_rate;    /**< BBB permeability increase */

    /** Feedback to NVC */
    bool enable_immune_nvc_feedback;     /**< Immune signals affect NVC */
    float cbf_inflammatory_scaling;      /**< CBF change per inflam level */

    /** Adenosine effects */
    bool enable_adenosine_antiinflam;    /**< Adenosine reduces inflammation */
    float adenosine_suppression_factor;  /**< Anti-inflammatory strength */

    /** Update parameters */
    float update_interval_ms;            /**< Bridge update interval */
} nvc_immune_config_t;

/**
 * @brief Immune region state
 */
typedef struct {
    uint32_t region_id;                  /**< Region ID */
    uint32_t nvc_unit_id;                /**< Mapped NVC unit */

    /** Microglial state */
    nvc_immune_microglia_state_t microglia_state;
    float microglia_activation;          /**< Activation level (0-1) */
    float phagocytic_activity;           /**< Phagocytosis rate */

    /** Inflammation state */
    nvc_immune_inflammation_t inflammation;
    float inflammation_level;            /**< Inflammation intensity (0-1) */
    float time_since_onset_ms;           /**< Time since inflammation start */

    /** BBB state */
    nvc_immune_bbb_state_t bbb_state;
    float bbb_permeability;              /**< Current permeability (0-1) */
    float infiltrating_cells;            /**< Infiltrating cell count */

    /** Metabolic triggers */
    float cbf_ratio;                     /**< Current CBF/baseline */
    float cumulative_ischemia;           /**< Integrated ischemic stress */
    float adenosine_level;               /**< Local adenosine */

    /** Cytokine levels */
    float cytokines[NVC_CYTOKINE_COUNT]; /**< Cytokine concentrations */
    float net_inflammatory_signal;       /**< Net pro vs anti-inflammatory */
} nvc_immune_region_state_t;

/**
 * @brief Vascular effect from immune activity
 */
typedef struct {
    uint32_t region_id;                  /**< Source region */
    nvc_immune_vascular_effect_t effect; /**< Effect type */
    float magnitude;                     /**< Effect magnitude */
    float cbf_modulation;                /**< Requested CBF change (%) */
    float bbb_permeability_change;       /**< BBB permeability change */
} nvc_immune_vascular_modulation_t;

/**
 * @brief Immune response event
 */
typedef struct {
    uint32_t response_id;                /**< Response identifier */
    uint32_t region_id;                  /**< Region where response occurs */

    /** Response state */
    nvc_immune_inflammation_t phase;     /**< Current phase */
    float onset_time_ms;                 /**< Response onset time */
    float duration_ms;                   /**< Response duration */
    float peak_intensity;                /**< Peak inflammation reached */

    /** Triggers */
    float triggering_cbf_ratio;          /**< CBF that triggered response */
    bool triggered_by_ischemia;          /**< Ischemia-triggered */
    bool triggered_by_reperfusion;       /**< Reperfusion-triggered */

    /** Resolution */
    bool resolved;                       /**< Response resolved */
    float resolution_time_ms;            /**< Time of resolution */
} nvc_immune_response_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t updates;                    /**< Total update calls */
    uint64_t microglial_activations;     /**< M1 activations */
    uint64_t microglial_resolutions;     /**< M2 transitions/resolution */
    uint64_t bbb_breaches;               /**< BBB permeability events */
    uint64_t immune_responses;           /**< Total immune responses */

    /** Averages */
    float mean_inflammation;             /**< Mean inflammation level */
    float mean_microglia_activation;     /**< Mean microglial activation */
    float mean_bbb_permeability;         /**< Mean BBB permeability */

    /** Extremes */
    float max_inflammation;              /**< Peak inflammation */
    float max_infiltration;              /**< Peak immune infiltration */

    /** Vascular effects */
    float total_vasoconstriction;        /**< Cumulative vasoconstriction */
    float total_vasodilation;            /**< Cumulative vasodilation */

    float last_update_ms;                /**< Last update timestamp */
} nvc_immune_stats_t;

/** Opaque bridge handle */
typedef struct nvc_immune_bridge_struct nvc_immune_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_immune_default_config(nvc_immune_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create NVC-Immune bridge
 *
 * WHAT: Allocates and initializes the bridge structure
 * WHY:  Establishes neuroimmune-vascular coupling
 * HOW:  Creates internal state for immune tracking
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT nvc_immune_bridge_t* nvc_immune_bridge_create(
    const nvc_immune_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void nvc_immune_bridge_destroy(nvc_immune_bridge_t* bridge);

//=============================================================================
// Region Management API
//=============================================================================

/**
 * @brief Register immune region with NVC unit
 *
 * WHAT: Maps immune-active region to vascular territory
 * WHY:  Enables local immune-vascular coupling
 * HOW:  Creates region entry with NVC mapping
 *
 * @param bridge Bridge handle
 * @param nvc_unit_id NVC unit ID
 * @param region_id Output region ID
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_immune_register_region(
    nvc_immune_bridge_t* bridge,
    uint32_t nvc_unit_id,
    uint32_t* region_id
);

/**
 * @brief Unregister immune region
 *
 * @param bridge Bridge handle
 * @param region_id Region to unregister
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_immune_unregister_region(
    nvc_immune_bridge_t* bridge,
    uint32_t region_id
);

//=============================================================================
// NVC → Immune API
//=============================================================================

/**
 * @brief Update immune state from NVC
 *
 * WHAT: Receives blood flow state for immune modulation
 * WHY:  Perfusion changes affect immune function
 * HOW:  Converts CBF/OEF to microglial/inflammatory state
 *
 * @param bridge Bridge handle
 * @param nvc_unit_id NVC unit providing state
 * @param cbf Cerebral blood flow
 * @param cbf_baseline Baseline CBF
 * @param oef Oxygen extraction fraction
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_immune_update_from_nvc(
    nvc_immune_bridge_t* bridge,
    uint32_t nvc_unit_id,
    float cbf,
    float cbf_baseline,
    float oef
);

/**
 * @brief Set BBB permeability from NVC
 *
 * @param bridge Bridge handle
 * @param nvc_unit_id NVC unit
 * @param permeability BBB permeability (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_immune_set_bbb_permeability(
    nvc_immune_bridge_t* bridge,
    uint32_t nvc_unit_id,
    float permeability
);

/**
 * @brief Set adenosine level (anti-inflammatory)
 *
 * @param bridge Bridge handle
 * @param nvc_unit_id NVC unit
 * @param adenosine Adenosine level (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_immune_set_adenosine(
    nvc_immune_bridge_t* bridge,
    uint32_t nvc_unit_id,
    float adenosine
);

//=============================================================================
// Immune → NVC API
//=============================================================================

/**
 * @brief Get vascular modulation from immune activity
 *
 * WHAT: Returns immune effects on vasculature
 * WHY:  Inflammation affects blood flow
 * HOW:  Computes net vascular effect from cytokines
 *
 * @param bridge Bridge handle
 * @param region_id Region to query
 * @param modulation Output vascular modulation
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_immune_get_vascular_modulation(
    const nvc_immune_bridge_t* bridge,
    uint32_t region_id,
    nvc_immune_vascular_modulation_t* modulation
);

/**
 * @brief Get CBF modulation factor from inflammation
 *
 * WHAT: Returns CBF scaling from immune state
 * WHY:  Inflammation can reduce or increase CBF
 * HOW:  Based on pro/anti-inflammatory balance
 *
 * @param bridge Bridge handle
 * @param region_id Region to query
 * @return CBF modulation factor (e.g., 0.8 = 20% reduction)
 */
NIMCP_EXPORT float nvc_immune_get_cbf_modulation(
    const nvc_immune_bridge_t* bridge,
    uint32_t region_id
);

//=============================================================================
// Immune State API
//=============================================================================

/**
 * @brief Get region immune state
 *
 * @param bridge Bridge handle
 * @param region_id Region to query
 * @param state Output immune state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_immune_get_region_state(
    const nvc_immune_bridge_t* bridge,
    uint32_t region_id,
    nvc_immune_region_state_t* state
);

/**
 * @brief Get microglial state
 *
 * @param bridge Bridge handle
 * @param region_id Region to query
 * @return Microglial state enum
 */
NIMCP_EXPORT nvc_immune_microglia_state_t nvc_immune_get_microglia_state(
    const nvc_immune_bridge_t* bridge,
    uint32_t region_id
);

/**
 * @brief Get inflammation level
 *
 * @param bridge Bridge handle
 * @param region_id Region to query
 * @return Inflammation level (0-1)
 */
NIMCP_EXPORT float nvc_immune_get_inflammation_level(
    const nvc_immune_bridge_t* bridge,
    uint32_t region_id
);

/**
 * @brief Get cytokine level
 *
 * @param bridge Bridge handle
 * @param region_id Region to query
 * @param cytokine Cytokine type
 * @return Cytokine concentration (0-1)
 */
NIMCP_EXPORT float nvc_immune_get_cytokine(
    const nvc_immune_bridge_t* bridge,
    uint32_t region_id,
    nvc_immune_cytokine_t cytokine
);

//=============================================================================
// Response Tracking API
//=============================================================================

/**
 * @brief Get active immune responses
 *
 * @param bridge Bridge handle
 * @param responses Output array of responses
 * @param max_responses Maximum responses to return
 * @return Number of active responses
 */
NIMCP_EXPORT int nvc_immune_get_active_responses(
    const nvc_immune_bridge_t* bridge,
    nvc_immune_response_t* responses,
    uint32_t max_responses
);

/**
 * @brief Check if region has active inflammation
 *
 * @param bridge Bridge handle
 * @param region_id Region to check
 * @return true if inflammation active
 */
NIMCP_EXPORT bool nvc_immune_has_active_inflammation(
    const nvc_immune_bridge_t* bridge,
    uint32_t region_id
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of immune dynamics
 * WHY:  Progress inflammation, cytokine decay, state transitions
 * HOW:  Called during simulation timestep
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_immune_update(
    nvc_immune_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_immune_reset(nvc_immune_bridge_t* bridge);

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
NIMCP_EXPORT int nvc_immune_get_stats(
    const nvc_immune_bridge_t* bridge,
    nvc_immune_stats_t* stats
);

/**
 * @brief Check for any critical immune events
 *
 * WHAT: Checks for severe inflammation or BBB compromise
 * WHY:  May require system-level response
 * HOW:  Scans all regions for critical states
 *
 * @param bridge Bridge handle
 * @return true if critical events present
 */
NIMCP_EXPORT bool nvc_immune_has_critical_events(
    const nvc_immune_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NVC_IMMUNE_BRIDGE_H */