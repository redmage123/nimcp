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
// nimcp_ph_plasticity_bridge.h - pH Dynamics to Synaptic Plasticity Bridge
//=============================================================================
/**
 * @file nimcp_ph_plasticity_bridge.h
 * @brief Bridge connecting pH dynamics with synaptic plasticity mechanisms
 *
 * WHAT: Bidirectional bridge between pH dynamics and plasticity (STDP/LTP/LTD)
 *       for modeling pH-dependent learning rate modulation.
 *
 * WHY:  pH profoundly affects synaptic plasticity:
 *       - Acidosis impairs LTP induction (NMDA receptor block)
 *       - Moderate acidosis can enhance LTD
 *       - Alkalosis facilitates NMDA-dependent plasticity
 *       - Vesicular pH affects neurotransmitter loading/release
 *       - Intracellular pH affects protein synthesis for consolidation
 *
 * HOW:  Two-way integration:
 *       1. pH -> Plasticity: Modulate STDP windows, LTP/LTD magnitudes
 *       2. Plasticity -> pH: Intense plasticity increases metabolic demand
 *       3. NMDA gating: pH-dependent NMDA block affects Ca2+ influx
 *       4. Protein synthesis: pH affects mRNA translation rates
 *
 * BIOLOGICAL BASIS:
 * ```
 * pH DYNAMICS                              PLASTICITY EFFECTS
 * ---------------------------------------------------------------
 * Extracellular acidosis (pH < 7.3)     -> LTP impairment
 * Extracellular alkalosis (pH > 7.5)    -> Enhanced LTP induction
 * NMDA proton block                     -> Reduced Ca2+ influx
 * Intracellular acidosis                -> Impaired protein synthesis
 * Vesicular pH elevation                -> Reduced NT release
 * CO2/bicarbonate dynamics              -> Synaptic transmission speed
 * ```
 *
 * NMDA-pH INTERACTION:
 * - NMDA receptors are proton-sensitive (IC50 ~pH 7.3)
 * - Proton block is voltage-independent
 * - Acidosis shifts Mg2+ block curve
 * - Results in reduced Ca2+ influx and impaired LTP
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_PH_PLASTICITY_BRIDGE_H
#define NIMCP_PH_PLASTICITY_BRIDGE_H

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
#define PH_PLASTICITY_MODULE_NAME       "ph_plasticity_bridge"

/** Maximum tracked synapses */
#define PH_PLASTICITY_MAX_SYNAPSES      4096

/** Maximum plasticity events per update */
#define PH_PLASTICITY_MAX_EVENTS        256

/** Normal pH for plasticity reference */
#define PH_PLASTICITY_NORMAL_PH         7.4f

/** NMDA IC50 for proton block */
#define PH_PLASTICITY_NMDA_IC50         7.3f

/** pH below which LTP is impaired */
#define PH_PLASTICITY_LTP_IMPAIR_PH     7.2f

/** pH below which LTP is blocked */
#define PH_PLASTICITY_LTP_BLOCK_PH      6.8f

/** pH above which LTP is enhanced */
#define PH_PLASTICITY_LTP_ENHANCE_PH    7.5f

/** Protein synthesis impairment pH threshold */
#define PH_PLASTICITY_PROTEIN_IMPAIR    7.0f

/** LTP magnitude reduction per 0.1 pH unit acidosis */
#define PH_PLASTICITY_LTP_REDUCTION     0.15f

/** LTD enhancement per 0.1 pH unit acidosis */
#define PH_PLASTICITY_LTD_ENHANCEMENT   0.08f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Plasticity type being modulated
 */
typedef enum {
    PH_PLASTICITY_TYPE_LTP = 0,          /**< Long-term potentiation */
    PH_PLASTICITY_TYPE_LTD,              /**< Long-term depression */
    PH_PLASTICITY_TYPE_STDP,             /**< Spike-timing dependent */
    PH_PLASTICITY_TYPE_BCM,              /**< BCM sliding threshold */
    PH_PLASTICITY_TYPE_HOMEOSTATIC,      /**< Homeostatic plasticity */
    PH_PLASTICITY_TYPE_COUNT
} ph_plasticity_type_t;

/**
 * @brief pH modulation targets for plasticity
 */
typedef enum {
    PH_PLASTICITY_MOD_LTP_MAGNITUDE = 0, /**< LTP magnitude scaling */
    PH_PLASTICITY_MOD_LTD_MAGNITUDE,     /**< LTD magnitude scaling */
    PH_PLASTICITY_MOD_STDP_WINDOW,       /**< STDP timing window */
    PH_PLASTICITY_MOD_BCM_THRESHOLD,     /**< BCM sliding threshold */
    PH_PLASTICITY_MOD_ELIGIBILITY,       /**< Eligibility trace decay */
    PH_PLASTICITY_MOD_CONSOLIDATION,     /**< Consolidation rate */
    PH_PLASTICITY_MOD_COUNT
} ph_plasticity_mod_target_t;

/**
 * @brief Plasticity phase affected by pH
 */
typedef enum {
    PH_PLASTICITY_PHASE_INDUCTION = 0,   /**< Initial induction */
    PH_PLASTICITY_PHASE_EXPRESSION,      /**< Early expression */
    PH_PLASTICITY_PHASE_CONSOLIDATION,   /**< Late consolidation */
    PH_PLASTICITY_PHASE_MAINTENANCE      /**< Long-term maintenance */
} ph_plasticity_phase_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Bridge configuration
 */
typedef struct {
    /** pH monitoring parameters */
    bool monitor_extracellular;          /**< Monitor extracellular pH */
    bool monitor_intracellular;          /**< Monitor intracellular pH */
    bool monitor_vesicular;              /**< Monitor vesicular pH */
    float ph_sample_interval_ms;         /**< pH sampling interval */

    /** NMDA modulation parameters */
    bool enable_nmda_modulation;         /**< Enable pH-NMDA interaction */
    float nmda_ic50;                     /**< NMDA proton IC50 */
    float nmda_hill_coefficient;         /**< NMDA proton Hill coeff. */

    /** LTP/LTD modulation parameters */
    bool enable_ltp_modulation;          /**< Enable LTP scaling */
    bool enable_ltd_modulation;          /**< Enable LTD scaling */
    float ltp_impairment_threshold;      /**< pH below which LTP impaired */
    float ltp_block_threshold;           /**< pH below which LTP blocked */
    float ltp_enhancement_threshold;     /**< pH above which LTP enhanced */
    float ltp_reduction_rate;            /**< LTP reduction per pH unit */
    float ltd_enhancement_rate;          /**< LTD enhancement per pH unit */

    /** STDP window modulation */
    bool enable_stdp_modulation;         /**< Enable STDP window scaling */
    float stdp_window_reduction_rate;    /**< Window narrowing per pH unit */

    /** Consolidation modulation */
    bool enable_consolidation_mod;       /**< Enable consolidation effects */
    float protein_synthesis_threshold;   /**< pH for protein synthesis impairment */

    /** Update parameters */
    float update_interval_ms;            /**< Bridge update interval */
} ph_plasticity_config_t;

/**
 * @brief pH state for plasticity modulation
 */
typedef struct {
    float extracellular_ph;              /**< Extracellular pH */
    float intracellular_ph;              /**< Intracellular pH */
    float vesicular_ph;                  /**< Synaptic vesicle pH */
    float ph_deviation;                  /**< Deviation from 7.4 */
    float nmda_conductance_factor;       /**< NMDA proton block (0-1) */
    float ca_influx_factor;              /**< Ca2+ influx scaling */
    float buffer_capacity;               /**< Available buffering */
    float timestamp_ms;                  /**< Timestamp */
} ph_plasticity_ph_state_t;

/**
 * @brief Plasticity modulation output
 */
typedef struct {
    /** LTP/LTD magnitude modifiers */
    float ltp_magnitude_factor;          /**< LTP scaling (0-2) */
    float ltd_magnitude_factor;          /**< LTD scaling (0-2) */

    /** STDP timing modifiers */
    float stdp_ltp_window_factor;        /**< LTP window scaling */
    float stdp_ltd_window_factor;        /**< LTD window scaling */
    float stdp_asymmetry_shift;          /**< Shift in STDP asymmetry */

    /** BCM threshold modifier */
    float bcm_threshold_shift;           /**< BCM threshold shift */

    /** Eligibility trace modifier */
    float eligibility_decay_factor;      /**< Eligibility trace decay rate */

    /** Consolidation modifier */
    float consolidation_rate_factor;     /**< Protein synthesis rate */

    /** Ca2+ dynamics modifier */
    float ca_influx_factor;              /**< Ca2+ influx scaling */

    /** Overall plasticity index */
    float plasticity_capacity;           /**< Overall capacity (0-1) */
} ph_plasticity_modulation_t;

/**
 * @brief Plasticity event for feedback
 */
typedef struct {
    uint32_t synapse_id;                 /**< Synapse identifier */
    ph_plasticity_type_t type;           /**< Plasticity type */
    ph_plasticity_phase_t phase;         /**< Current phase */
    float weight_change;                 /**< Weight change applied */
    float calcium_transient;             /**< Ca2+ signal magnitude */
    float energy_consumed;               /**< Metabolic cost */
    float duration_ms;                   /**< Event duration */
} ph_plasticity_event_t;

/**
 * @brief Per-synapse pH modulation state
 */
typedef struct {
    uint32_t synapse_id;                 /**< Synapse identifier */
    float local_ph;                      /**< Local pH at synapse */
    float ltp_capacity;                  /**< LTP induction capacity */
    float ltd_capacity;                  /**< LTD induction capacity */
    float protein_synthesis_rate;        /**< Local protein synthesis */
    float last_plasticity_time_ms;       /**< Last plasticity event */
    bool consolidation_blocked;          /**< Consolidation blocked by pH */
} ph_plasticity_synapse_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t ph_updates;                 /**< pH state updates */
    uint64_t modulations_computed;       /**< Modulation computations */
    uint64_t ltp_events_modulated;       /**< LTP events affected */
    uint64_t ltd_events_modulated;       /**< LTD events affected */
    uint64_t consolidation_blocked;      /**< Events with blocked consolidation */
    float avg_ltp_factor;                /**< Average LTP scaling */
    float avg_ltd_factor;                /**< Average LTD scaling */
    float avg_nmda_block;                /**< Average NMDA block */
    float min_plasticity_capacity;       /**< Minimum capacity seen */
    float total_metabolic_cost;          /**< Cumulative metabolic cost */
    float last_update_ms;                /**< Last update timestamp */
} ph_plasticity_stats_t;

/** Opaque bridge handle */
typedef struct ph_plasticity_bridge_struct ph_plasticity_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_plasticity_default_config(ph_plasticity_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create pH-plasticity bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT ph_plasticity_bridge_t* ph_plasticity_bridge_create(
    const ph_plasticity_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void ph_plasticity_bridge_destroy(ph_plasticity_bridge_t* bridge);

//=============================================================================
// pH Input API (pH Dynamics -> Plasticity)
//=============================================================================

/**
 * @brief Update pH state from pH dynamics module
 *
 * WHAT: Receives current pH values from pH dynamics
 * WHY:  pH state determines plasticity modulation
 * HOW:  Stores pH state, computes NMDA block, updates factors
 *
 * @param bridge Bridge handle
 * @param extracellular_ph Current extracellular pH
 * @param intracellular_ph Current intracellular pH
 * @param vesicular_ph Current vesicular pH (0 to ignore)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_plasticity_update_ph(
    ph_plasticity_bridge_t* bridge,
    float extracellular_ph,
    float intracellular_ph,
    float vesicular_ph
);

/**
 * @brief Set buffer capacity for recovery estimation
 *
 * @param bridge Bridge handle
 * @param buffer_capacity Current buffer capacity
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_plasticity_set_buffer_capacity(
    ph_plasticity_bridge_t* bridge,
    float buffer_capacity
);

//=============================================================================
// Plasticity Modulation API
//=============================================================================

/**
 * @brief Compute plasticity modulation based on current pH
 *
 * WHAT: Calculates all pH-dependent plasticity modulations
 * WHY:  Single call for complete modulation state
 * HOW:  Evaluates NMDA block, LTP/LTD factors, STDP windows
 *
 * @param bridge Bridge handle
 * @param modulation Output modulation values
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_plasticity_compute_modulation(
    ph_plasticity_bridge_t* bridge,
    ph_plasticity_modulation_t* modulation
);

/**
 * @brief Get LTP magnitude scaling factor
 *
 * @param bridge Bridge handle
 * @param factor Output: LTP magnitude factor (0-2)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_plasticity_get_ltp_factor(
    const ph_plasticity_bridge_t* bridge,
    float* factor
);

/**
 * @brief Get LTD magnitude scaling factor
 *
 * @param bridge Bridge handle
 * @param factor Output: LTD magnitude factor (0-2)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_plasticity_get_ltd_factor(
    const ph_plasticity_bridge_t* bridge,
    float* factor
);

/**
 * @brief Get STDP timing window scaling
 *
 * @param bridge Bridge handle
 * @param ltp_window Output: LTP window factor
 * @param ltd_window Output: LTD window factor
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_plasticity_get_stdp_windows(
    const ph_plasticity_bridge_t* bridge,
    float* ltp_window,
    float* ltd_window
);

/**
 * @brief Get NMDA-mediated Ca2+ influx factor
 *
 * @param bridge Bridge handle
 * @param factor Output: Ca2+ influx factor (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_plasticity_get_ca_influx_factor(
    const ph_plasticity_bridge_t* bridge,
    float* factor
);

/**
 * @brief Get protein synthesis/consolidation factor
 *
 * @param bridge Bridge handle
 * @param factor Output: consolidation rate factor (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_plasticity_get_consolidation_factor(
    const ph_plasticity_bridge_t* bridge,
    float* factor
);

/**
 * @brief Check if plasticity induction is possible
 *
 * @param bridge Bridge handle
 * @param type Plasticity type
 * @return true if induction possible at current pH
 */
NIMCP_EXPORT bool ph_plasticity_can_induce(
    const ph_plasticity_bridge_t* bridge,
    ph_plasticity_type_t type
);

//=============================================================================
// Per-Synapse API
//=============================================================================

/**
 * @brief Get modulation for specific synapse
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse identifier
 * @param local_ph Local pH at synapse
 * @param modulation Output modulation
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_plasticity_get_synapse_modulation(
    const ph_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float local_ph,
    ph_plasticity_modulation_t* modulation
);

/**
 * @brief Register synapse for pH tracking
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse identifier
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_plasticity_register_synapse(
    ph_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Unregister synapse from pH tracking
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse identifier
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_plasticity_unregister_synapse(
    ph_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

//=============================================================================
// Plasticity Feedback API (Plasticity -> pH)
//=============================================================================

/**
 * @brief Report plasticity event for metabolic feedback
 *
 * WHAT: Receives plasticity event from plasticity module
 * WHY:  Intense plasticity increases metabolic acid production
 * HOW:  Accumulates metabolic cost, signals to pH dynamics
 *
 * @param bridge Bridge handle
 * @param event Plasticity event
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_plasticity_report_event(
    ph_plasticity_bridge_t* bridge,
    const ph_plasticity_event_t* event
);

/**
 * @brief Get accumulated metabolic cost from plasticity
 *
 * @param bridge Bridge handle
 * @param cost Output: metabolic cost (arbitrary units)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_plasticity_get_metabolic_cost(
    const ph_plasticity_bridge_t* bridge,
    float* cost
);

/**
 * @brief Reset accumulated metabolic cost
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_plasticity_reset_metabolic_cost(
    ph_plasticity_bridge_t* bridge
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Track pH changes, update modulation factors
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_plasticity_update(
    ph_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_plasticity_reset(ph_plasticity_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get current pH state
 *
 * @param bridge Bridge handle
 * @param state Output pH state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_plasticity_get_ph_state(
    const ph_plasticity_bridge_t* bridge,
    ph_plasticity_ph_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_plasticity_get_stats(
    const ph_plasticity_bridge_t* bridge,
    ph_plasticity_stats_t* stats
);

/**
 * @brief Check if LTP is currently impaired by pH
 *
 * @param bridge Bridge handle
 * @return true if LTP is impaired
 */
NIMCP_EXPORT bool ph_plasticity_is_ltp_impaired(
    const ph_plasticity_bridge_t* bridge
);

/**
 * @brief Check if consolidation is blocked by pH
 *
 * @param bridge Bridge handle
 * @return true if consolidation blocked
 */
NIMCP_EXPORT bool ph_plasticity_is_consolidation_blocked(
    const ph_plasticity_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PH_PLASTICITY_BRIDGE_H */