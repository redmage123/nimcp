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
// nimcp_nvc_plasticity_bridge.h - Neurovascular Coupling to Plasticity Bridge
//=============================================================================
/**
 * @file nimcp_nvc_plasticity_bridge.h
 * @brief Bridge between Neurovascular Coupling and Synaptic Plasticity
 *
 * WHAT: Connects blood flow dynamics with synaptic plasticity mechanisms,
 *       enabling metabolic gating of learning and memory consolidation.
 *
 * WHY:  Synaptic plasticity is energetically expensive:
 *       - LTP requires protein synthesis and receptor trafficking
 *       - Memory consolidation depends on adequate metabolic support
 *       - Hypoxia/hypoglycemia impairs learning
 *
 * HOW:  Metabolic modulation of plasticity:
 *       1. Oxygen levels gate LTP/LTD magnitude
 *       2. Glucose availability enables protein synthesis
 *       3. CBF changes modulate STDP time windows
 *       4. Astrocyte signals affect eligibility traces
 *
 * BIOLOGICAL BASIS:
 * ```
 * NEUROVASCULAR                           PLASTICITY EFFECTS
 * ─────────────────────────────────────────────────────────────────
 * Oxygen delivery                   → LTP amplitude scaling
 * Glucose availability              → Protein synthesis-dependent LTP
 * Adenosine (hypoxia marker)        → LTD bias during stress
 * Lactate shuttle (astrocyte)       → Extended plasticity window
 * NO from endothelium               → NMDA-dependent plasticity modulation
 * BOLD undershoot                   → Consolidation window marker
 * ```
 *
 * METABOLIC REQUIREMENTS:
 * - Early LTP (E-LTP): Moderate ATP for receptor insertion
 * - Late LTP (L-LTP): High glucose for protein synthesis
 * - Memory consolidation: Sustained metabolic support
 * - Synaptic tagging: Minimal metabolic cost
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_NVC_PLASTICITY_BRIDGE_H
#define NIMCP_NVC_PLASTICITY_BRIDGE_H

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
#define NVC_PLASTICITY_MODULE_NAME      "nvc_plasticity_bridge"

/** Maximum plasticity sites tracked */
#define NVC_PLASTICITY_MAX_SITES        256

/** Maximum metabolic zones */
#define NVC_PLASTICITY_MAX_ZONES        64

/** Minimum O2 for LTP (fraction of baseline) */
#define NVC_PLASTICITY_O2_LTP_MIN       0.7f

/** Minimum glucose for protein synthesis */
#define NVC_PLASTICITY_GLUCOSE_PS_MIN   0.6f

/** LTP amplitude reduction per % O2 deficit */
#define NVC_PLASTICITY_O2_LTP_SCALE     0.02f

/** Consolidation window duration (ms) */
#define NVC_PLASTICITY_CONSOLIDATION_MS 30000.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Plasticity phase
 */
typedef enum {
    NVC_PLASTICITY_PHASE_INDUCTION = 0, /**< Initial potentiation */
    NVC_PLASTICITY_PHASE_EARLY_LTP,      /**< E-LTP (minutes) */
    NVC_PLASTICITY_PHASE_LATE_LTP,       /**< L-LTP (requires protein) */
    NVC_PLASTICITY_PHASE_CONSOLIDATION,  /**< Memory consolidation */
    NVC_PLASTICITY_PHASE_MAINTAINED      /**< Stable synaptic change */
} nvc_plasticity_phase_t;

/**
 * @brief Metabolic support level
 */
typedef enum {
    NVC_METABOLIC_SUPPORT_OPTIMAL = 0,   /**< Full plasticity capacity */
    NVC_METABOLIC_SUPPORT_ADEQUATE,      /**< Normal plasticity */
    NVC_METABOLIC_SUPPORT_MARGINAL,      /**< Reduced LTP, no L-LTP */
    NVC_METABOLIC_SUPPORT_INSUFFICIENT,  /**< LTD bias, no LTP */
    NVC_METABOLIC_SUPPORT_CRITICAL       /**< Plasticity blocked */
} nvc_metabolic_support_t;

/**
 * @brief Plasticity modulation type
 */
typedef enum {
    NVC_PLASTICITY_MOD_LTP_AMPLITUDE = 0,  /**< LTP magnitude scaling */
    NVC_PLASTICITY_MOD_LTD_AMPLITUDE,      /**< LTD magnitude scaling */
    NVC_PLASTICITY_MOD_STDP_WINDOW,        /**< STDP time window */
    NVC_PLASTICITY_MOD_ELIGIBILITY_DECAY,  /**< Trace decay rate */
    NVC_PLASTICITY_MOD_CONSOLIDATION_RATE  /**< Memory consolidation speed */
} nvc_plasticity_mod_type_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for NVC-Plasticity bridge
 */
typedef struct {
    /** Oxygen modulation */
    float o2_ltp_threshold;              /**< O2 level for full LTP */
    float o2_ltd_bias_threshold;         /**< O2 level for LTD bias */
    float o2_ltp_scale;                  /**< LTP scaling per O2 change */

    /** Glucose modulation */
    float glucose_protein_threshold;     /**< Glucose for protein synthesis */
    float glucose_eltp_threshold;        /**< Glucose for E-LTP */

    /** Adenosine effects */
    bool enable_adenosine_modulation;    /**< Hypoxia → adenosine → LTD */
    float adenosine_ltd_boost;           /**< LTD increase from adenosine */
    float adenosine_ltp_suppression;     /**< LTP reduction from adenosine */

    /** Lactate effects */
    bool enable_lactate_modulation;      /**< Astrocyte lactate support */
    float lactate_window_extension;      /**< Plasticity window extension */

    /** NO effects */
    bool enable_no_modulation;           /**< Endothelial NO effects */
    float no_nmda_potentiation;          /**< NMDA enhancement by NO */

    /** Consolidation */
    float consolidation_window_ms;       /**< Time for consolidation */
    float consolidation_metabolic_rate;  /**< Metabolic cost of consolidation */

    /** STDP window modulation */
    bool enable_stdp_window_scaling;     /**< CBF affects STDP window */
    float stdp_window_cbf_scale;         /**< Window scaling factor */

    /** Update parameters */
    float update_interval_ms;            /**< Bridge update interval */
} nvc_plasticity_config_t;

/**
 * @brief Metabolic state for plasticity site
 */
typedef struct {
    uint32_t site_id;                    /**< Plasticity site ID */
    uint32_t nvc_unit_id;                /**< Mapped NVC unit */

    /** Metabolic levels */
    float oxygen_level;                  /**< Local O2 (0-1) */
    float glucose_level;                 /**< Local glucose (0-1) */
    float adenosine_level;               /**< Adenosine from hypoxia (0-1) */
    float lactate_level;                 /**< Astrocyte lactate (0-1) */
    float no_level;                      /**< Nitric oxide (0-1) */

    /** Derived modulations */
    float ltp_amplitude_factor;          /**< LTP scaling (0-1) */
    float ltd_amplitude_factor;          /**< LTD scaling (0-1+) */
    float stdp_window_factor;            /**< STDP window scaling */
    float eligibility_decay_factor;      /**< Trace decay scaling */
    float consolidation_rate_factor;     /**< Consolidation speed */

    /** Support level */
    nvc_metabolic_support_t support_level;
    bool protein_synthesis_enabled;      /**< Can sustain L-LTP */
    bool consolidation_possible;         /**< Can consolidate */
} nvc_plasticity_metabolic_t;

/**
 * @brief Plasticity event tracking
 */
typedef struct {
    uint32_t site_id;                    /**< Site where event occurred */
    nvc_plasticity_phase_t phase;        /**< Current plasticity phase */
    float weight_change;                 /**< Magnitude of change */
    float metabolic_modulation;          /**< Metabolic factor applied */
    float time_since_induction_ms;       /**< Time since induction */
    bool consolidated;                   /**< Whether consolidated */
} nvc_plasticity_event_t;

/**
 * @brief Consolidation window state
 */
typedef struct {
    uint32_t site_id;                    /**< Plasticity site */
    float window_start_ms;               /**< Window start time */
    float window_duration_ms;            /**< Window duration */
    float metabolic_integral;            /**< Integrated metabolic support */
    float consolidation_progress;        /**< Progress (0-1) */
    bool window_open;                    /**< Whether window is active */
} nvc_plasticity_consolidation_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t updates;                    /**< Total update calls */
    uint64_t ltp_events;                 /**< LTP events processed */
    uint64_t ltd_events;                 /**< LTD events processed */
    uint64_t blocked_events;             /**< Events blocked by metabolism */
    uint64_t consolidations;             /**< Successful consolidations */
    uint64_t failed_consolidations;      /**< Failed consolidations */

    /** Modulation statistics */
    float mean_ltp_modulation;           /**< Mean LTP scaling applied */
    float mean_ltd_modulation;           /**< Mean LTD scaling applied */
    float mean_metabolic_support;        /**< Mean support level */

    /** Extreme values */
    float min_oxygen_during_ltp;         /**< Lowest O2 during LTP */
    float min_glucose_during_lltp;       /**< Lowest glucose during L-LTP */

    float last_update_ms;                /**< Last update timestamp */
} nvc_plasticity_stats_t;

/** Opaque bridge handle */
typedef struct nvc_plasticity_bridge_struct nvc_plasticity_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_plasticity_default_config(nvc_plasticity_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create NVC-Plasticity bridge
 *
 * WHAT: Allocates and initializes the bridge structure
 * WHY:  Establishes metabolic coupling for plasticity modulation
 * HOW:  Creates internal state for tracking metabolic effects on learning
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT nvc_plasticity_bridge_t* nvc_plasticity_bridge_create(
    const nvc_plasticity_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void nvc_plasticity_bridge_destroy(nvc_plasticity_bridge_t* bridge);

//=============================================================================
// Site Management API
//=============================================================================

/**
 * @brief Register plasticity site with NVC unit
 *
 * WHAT: Maps plasticity site to vascular territory
 * WHY:  Enables local metabolic modulation of plasticity
 * HOW:  Creates site entry with NVC mapping
 *
 * @param bridge Bridge handle
 * @param site_id Plasticity site ID
 * @param nvc_unit_id NVC unit ID
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_plasticity_register_site(
    nvc_plasticity_bridge_t* bridge,
    uint32_t site_id,
    uint32_t nvc_unit_id
);

/**
 * @brief Unregister plasticity site
 *
 * @param bridge Bridge handle
 * @param site_id Site to unregister
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_plasticity_unregister_site(
    nvc_plasticity_bridge_t* bridge,
    uint32_t site_id
);

//=============================================================================
// Metabolic State API
//=============================================================================

/**
 * @brief Update metabolic state from NVC
 *
 * WHAT: Receives blood flow and metabolite levels
 * WHY:  Updates plasticity modulation factors
 * HOW:  Converts NVC state to plasticity constraints
 *
 * @param bridge Bridge handle
 * @param nvc_unit_id NVC unit providing state
 * @param cbf Cerebral blood flow
 * @param cbf_baseline Baseline CBF
 * @param oef Oxygen extraction fraction
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_plasticity_update_from_nvc(
    nvc_plasticity_bridge_t* bridge,
    uint32_t nvc_unit_id,
    float cbf,
    float cbf_baseline,
    float oef
);

/**
 * @brief Set adenosine level (hypoxia marker)
 *
 * @param bridge Bridge handle
 * @param nvc_unit_id NVC unit
 * @param adenosine Adenosine level (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_plasticity_set_adenosine(
    nvc_plasticity_bridge_t* bridge,
    uint32_t nvc_unit_id,
    float adenosine
);

/**
 * @brief Set lactate level (astrocyte support)
 *
 * @param bridge Bridge handle
 * @param nvc_unit_id NVC unit
 * @param lactate Lactate level (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_plasticity_set_lactate(
    nvc_plasticity_bridge_t* bridge,
    uint32_t nvc_unit_id,
    float lactate
);

/**
 * @brief Get metabolic state for plasticity site
 *
 * @param bridge Bridge handle
 * @param site_id Site to query
 * @param state Output metabolic state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_plasticity_get_metabolic_state(
    const nvc_plasticity_bridge_t* bridge,
    uint32_t site_id,
    nvc_plasticity_metabolic_t* state
);

//=============================================================================
// Plasticity Modulation API
//=============================================================================

/**
 * @brief Get LTP amplitude factor
 *
 * WHAT: Returns metabolic scaling for LTP
 * WHY:  Low O2/glucose reduces LTP magnitude
 * HOW:  Computed from local metabolic state
 *
 * @param bridge Bridge handle
 * @param site_id Plasticity site
 * @return LTP scaling factor (0-1), or -1 on error
 */
NIMCP_EXPORT float nvc_plasticity_get_ltp_factor(
    const nvc_plasticity_bridge_t* bridge,
    uint32_t site_id
);

/**
 * @brief Get LTD amplitude factor
 *
 * WHAT: Returns metabolic scaling for LTD
 * WHY:  Hypoxia/adenosine may enhance LTD
 * HOW:  Computed from adenosine and stress markers
 *
 * @param bridge Bridge handle
 * @param site_id Plasticity site
 * @return LTD scaling factor (0-2+), or -1 on error
 */
NIMCP_EXPORT float nvc_plasticity_get_ltd_factor(
    const nvc_plasticity_bridge_t* bridge,
    uint32_t site_id
);

/**
 * @brief Get STDP window scaling factor
 *
 * WHAT: Returns window width scaling
 * WHY:  CBF changes affect temporal dynamics
 * HOW:  Based on blood flow state
 *
 * @param bridge Bridge handle
 * @param site_id Plasticity site
 * @return Window scaling factor, or -1 on error
 */
NIMCP_EXPORT float nvc_plasticity_get_stdp_window_factor(
    const nvc_plasticity_bridge_t* bridge,
    uint32_t site_id
);

/**
 * @brief Check if protein synthesis is possible
 *
 * WHAT: Determines if L-LTP can be sustained
 * WHY:  Protein synthesis requires glucose
 * HOW:  Checks glucose level against threshold
 *
 * @param bridge Bridge handle
 * @param site_id Plasticity site
 * @return true if protein synthesis possible
 */
NIMCP_EXPORT bool nvc_plasticity_protein_synthesis_enabled(
    const nvc_plasticity_bridge_t* bridge,
    uint32_t site_id
);

//=============================================================================
// Consolidation API
//=============================================================================

/**
 * @brief Start consolidation window
 *
 * WHAT: Opens consolidation window for plasticity site
 * WHY:  Tracks metabolic support during consolidation
 * HOW:  Begins integration of metabolic state
 *
 * @param bridge Bridge handle
 * @param site_id Plasticity site
 * @param weight_change Magnitude to consolidate
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_plasticity_start_consolidation(
    nvc_plasticity_bridge_t* bridge,
    uint32_t site_id,
    float weight_change
);

/**
 * @brief Get consolidation state
 *
 * @param bridge Bridge handle
 * @param site_id Plasticity site
 * @param consolidation Output consolidation state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_plasticity_get_consolidation_state(
    const nvc_plasticity_bridge_t* bridge,
    uint32_t site_id,
    nvc_plasticity_consolidation_t* consolidation
);

/**
 * @brief Check if consolidation succeeded
 *
 * @param bridge Bridge handle
 * @param site_id Plasticity site
 * @return true if consolidation was successful
 */
NIMCP_EXPORT bool nvc_plasticity_consolidation_succeeded(
    const nvc_plasticity_bridge_t* bridge,
    uint32_t site_id
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of metabolic dynamics
 * WHY:  Progress consolidation, update modulation factors
 * HOW:  Called during simulation timestep
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_plasticity_update(
    nvc_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge to initial state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int nvc_plasticity_reset(nvc_plasticity_bridge_t* bridge);

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
NIMCP_EXPORT int nvc_plasticity_get_stats(
    const nvc_plasticity_bridge_t* bridge,
    nvc_plasticity_stats_t* stats
);

/**
 * @brief Get metabolic support level for site
 *
 * @param bridge Bridge handle
 * @param site_id Plasticity site
 * @return Metabolic support level enum
 */
NIMCP_EXPORT nvc_metabolic_support_t nvc_plasticity_get_support_level(
    const nvc_plasticity_bridge_t* bridge,
    uint32_t site_id
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NVC_PLASTICITY_BRIDGE_H */