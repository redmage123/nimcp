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
// nimcp_ph_immune_bridge.h - pH Dynamics to Brain Immune System Bridge
//=============================================================================
/**
 * @file nimcp_ph_immune_bridge.h
 * @brief Bridge connecting pH dynamics with brain immune system and inflammation
 *
 * WHAT: Bidirectional bridge between pH dynamics and the brain immune system
 *       for modeling pH-inflammation interactions and immune-pH feedback.
 *
 * WHY:  pH and immunity are intimately connected:
 *       - Tissue acidosis is a hallmark of inflammation
 *       - Acidic pH activates microglia and astrocytes
 *       - Cytokines modulate ion channels and pH regulation
 *       - Metabolic acidosis impairs immune function
 *       - pH affects phagocytic activity and cytokine release
 *       - ASIC channels on immune cells respond to acidosis
 *
 * HOW:  Two-way integration:
 *       1. pH -> Immune: Acidosis triggers/modulates immune responses
 *       2. Immune -> pH: Inflammation produces local acidosis
 *       3. Cytokine effects: IL-1, TNF, IL-6 modulate pH regulation
 *       4. Microglial activation: pH-dependent state transitions
 *
 * BIOLOGICAL BASIS:
 * ```
 * pH DYNAMICS                              IMMUNE EFFECTS
 * ---------------------------------------------------------------
 * Extracellular acidosis (pH < 7.3)     -> Microglial activation
 * Severe acidosis (pH < 7.0)            -> Astrocyte reactivity
 * ASIC activation on microglia          -> Pro-inflammatory shift
 * Proton sensing (GPR4, TDAG8)          -> Immune cell signaling
 * Inflammation                          <- Local acid production
 * TNF-alpha release                     <- Ischemic acidosis
 * IL-1beta release                      <- Neuroinflammation
 * ```
 *
 * IMMUNE CELL pH RESPONSES:
 * - Microglia: ASIC1a activation at pH < 7.0, promotes M1 state
 * - Astrocytes: Reactive gliosis with acidosis
 * - T-cells: Reduced proliferation in acidic environment
 * - Macrophages: Altered cytokine profile with pH
 *
 * INFLAMMATION-pH FEEDBACK:
 * - Glycolytic shift produces lactic acid
 * - ROS damage impairs pH regulation
 * - Cytokines modulate NHE activity
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_PH_IMMUNE_BRIDGE_H
#define NIMCP_PH_IMMUNE_BRIDGE_H

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
#define PH_IMMUNE_MODULE_NAME           "ph_immune_bridge"

/** Maximum tracked immune regions */
#define PH_IMMUNE_MAX_REGIONS           64

/** Maximum cytokine types */
#define PH_IMMUNE_MAX_CYTOKINES         8

/** Normal tissue pH */
#define PH_IMMUNE_NORMAL_PH             7.4f

/** pH threshold for microglial activation */
#define PH_IMMUNE_MICROGLIA_THRESHOLD   7.2f

/** pH threshold for severe immune response */
#define PH_IMMUNE_SEVERE_THRESHOLD      7.0f

/** pH threshold for astrocyte reactivity */
#define PH_IMMUNE_ASTROCYTE_THRESHOLD   7.1f

/** Inflammation acid production rate */
#define PH_IMMUNE_INFLAM_ACID_RATE      0.02f

/** Cytokine NHE modulation factor */
#define PH_IMMUNE_CYTOKINE_NHE_MOD      0.15f

/** Maximum inflammation-induced pH drop */
#define PH_IMMUNE_MAX_PH_DROP           0.5f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Cytokine types affecting pH/immune interaction
 */
typedef enum {
    PH_IMMUNE_CYTOKINE_IL1_BETA = 0,     /**< Interleukin-1 beta */
    PH_IMMUNE_CYTOKINE_TNF_ALPHA,        /**< Tumor necrosis factor alpha */
    PH_IMMUNE_CYTOKINE_IL6,              /**< Interleukin-6 */
    PH_IMMUNE_CYTOKINE_IL10,             /**< Interleukin-10 (anti-inflam) */
    PH_IMMUNE_CYTOKINE_TGFB,             /**< TGF-beta */
    PH_IMMUNE_CYTOKINE_IFN_GAMMA,        /**< Interferon gamma */
    PH_IMMUNE_CYTOKINE_COUNT
} ph_immune_cytokine_t;

/**
 * @brief Microglial activation state
 */
typedef enum {
    PH_IMMUNE_MICROGLIA_RESTING = 0,     /**< Resting/surveying (M0) */
    PH_IMMUNE_MICROGLIA_M1,              /**< Pro-inflammatory (M1) */
    PH_IMMUNE_MICROGLIA_M2,              /**< Anti-inflammatory (M2) */
    PH_IMMUNE_MICROGLIA_ACTIVATED,       /**< Non-polarized activated */
    PH_IMMUNE_MICROGLIA_PHAGOCYTIC       /**< Phagocytic state */
} ph_immune_microglia_state_t;

/**
 * @brief Astrocyte reactivity state
 */
typedef enum {
    PH_IMMUNE_ASTROCYTE_QUIESCENT = 0,   /**< Normal homeostatic */
    PH_IMMUNE_ASTROCYTE_REACTIVE_A1,     /**< Neurotoxic reactive */
    PH_IMMUNE_ASTROCYTE_REACTIVE_A2,     /**< Neuroprotective reactive */
    PH_IMMUNE_ASTROCYTE_SCAR_FORMING     /**< Scar-forming gliosis */
} ph_immune_astrocyte_state_t;

/**
 * @brief Inflammation severity level
 */
typedef enum {
    PH_IMMUNE_INFLAM_NONE = 0,           /**< No inflammation */
    PH_IMMUNE_INFLAM_MILD,               /**< Mild inflammation */
    PH_IMMUNE_INFLAM_MODERATE,           /**< Moderate inflammation */
    PH_IMMUNE_INFLAM_SEVERE,             /**< Severe inflammation */
    PH_IMMUNE_INFLAM_CHRONIC             /**< Chronic inflammation */
} ph_immune_inflammation_level_t;

/**
 * @brief pH modulation by immune signals
 */
typedef enum {
    PH_IMMUNE_MOD_NHE_ACTIVITY = 0,      /**< Na+/H+ exchanger activity */
    PH_IMMUNE_MOD_NBC_ACTIVITY,          /**< Na+/HCO3- cotransporter */
    PH_IMMUNE_MOD_BUFFER_CAPACITY,       /**< Buffer system capacity */
    PH_IMMUNE_MOD_METABOLIC_RATE,        /**< Metabolic acid production */
    PH_IMMUNE_MOD_COUNT
} ph_immune_mod_target_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Bridge configuration
 */
typedef struct {
    /** pH monitoring parameters */
    bool monitor_regional_ph;            /**< Monitor per-region pH */
    float ph_sample_interval_ms;         /**< pH sampling interval */

    /** Microglial activation parameters */
    bool enable_microglia_activation;    /**< Enable pH-microglia link */
    float microglia_activation_ph;       /**< pH for activation threshold */
    float microglia_m1_bias_acidosis;    /**< M1 bias per 0.1 pH drop */

    /** Astrocyte reactivity parameters */
    bool enable_astrocyte_reactivity;    /**< Enable pH-astrocyte link */
    float astrocyte_reactivity_ph;       /**< pH for reactivity threshold */

    /** Cytokine modulation parameters */
    bool enable_cytokine_modulation;     /**< Enable cytokine->pH effects */
    float cytokine_nhe_mod_strength;     /**< Cytokine NHE modulation */
    float cytokine_buffer_mod_strength;  /**< Cytokine buffer modulation */

    /** Inflammation feedback parameters */
    bool enable_inflammation_feedback;   /**< Enable inflammation->pH */
    float inflammation_acid_rate;        /**< Acid per inflammation unit */
    float max_inflammation_ph_drop;      /**< Maximum pH drop from inflam */

    /** Update parameters */
    float update_interval_ms;            /**< Bridge update interval */
} ph_immune_config_t;

/**
 * @brief Cytokine state
 */
typedef struct {
    ph_immune_cytokine_t type;           /**< Cytokine type */
    float concentration;                 /**< Concentration (arbitrary) */
    float production_rate;               /**< Current production rate */
    float clearance_rate;                /**< Clearance rate */
    float nhe_modulation;                /**< Effect on NHE activity */
    float buffer_modulation;             /**< Effect on buffering */
    bool is_proinflammatory;             /**< Pro or anti-inflammatory */
} ph_immune_cytokine_state_t;

/**
 * @brief Microglial state in region
 */
typedef struct {
    ph_immune_microglia_state_t state;   /**< Current activation state */
    float activation_level;              /**< Activation level (0-1) */
    float m1_m2_balance;                 /**< M1/M2 balance (-1 to +1) */
    float phagocytic_activity;           /**< Phagocytic activity (0-1) */
    float asic_activation;               /**< ASIC channel activation */
    float cytokine_release_rate;         /**< Cytokine release rate */
    float local_ph;                      /**< Local pH at cell */
    uint32_t region_id;                  /**< Region identifier */
} ph_immune_microglia_info_t;

/**
 * @brief Astrocyte state in region
 */
typedef struct {
    ph_immune_astrocyte_state_t state;   /**< Current reactivity state */
    float reactivity_level;              /**< Reactivity level (0-1) */
    float a1_a2_balance;                 /**< A1/A2 balance (-1 to +1) */
    float glutamate_uptake;              /**< Glutamate uptake capacity */
    float lactate_shuttle;               /**< Lactate shuttle activity */
    float local_ph;                      /**< Local pH at cell */
    uint32_t region_id;                  /**< Region identifier */
} ph_immune_astrocyte_info_t;

/**
 * @brief Regional inflammation state
 */
typedef struct {
    uint32_t region_id;                  /**< Region identifier */
    ph_immune_inflammation_level_t level; /**< Inflammation level */
    float inflammation_index;            /**< Quantitative index (0-1) */
    float local_ph;                      /**< Local pH */
    float acid_production;               /**< Acid produced by inflam */
    float cytokine_load;                 /**< Total cytokine burden */
    ph_immune_microglia_info_t microglia; /**< Microglial state */
    ph_immune_astrocyte_info_t astrocytes; /**< Astrocyte state */
    float duration_ms;                   /**< Inflammation duration */
} ph_immune_region_state_t;

/**
 * @brief pH effect on immune function
 */
typedef struct {
    /** Microglial modulation */
    float microglia_activation_factor;   /**< Activation enhancement */
    float m1_polarization_factor;        /**< M1 polarization bias */
    float phagocytosis_factor;           /**< Phagocytic efficiency */

    /** Astrocyte modulation */
    float astrocyte_reactivity_factor;   /**< Reactivity enhancement */
    float a1_polarization_factor;        /**< A1 polarization bias */

    /** Cytokine modulation */
    float proinflam_cytokine_factor;     /**< Pro-inflam cytokine boost */
    float antiinflam_cytokine_factor;    /**< Anti-inflam cytokine factor */

    /** General immune */
    float immune_suppression_factor;     /**< Overall suppression at low pH */
} ph_immune_effect_t;

/**
 * @brief Immune effect on pH regulation
 */
typedef struct {
    /** Pump modulation */
    float nhe_activity_factor;           /**< NHE activity modifier */
    float nbc_activity_factor;           /**< NBC activity modifier */

    /** Buffer modulation */
    float buffer_capacity_factor;        /**< Buffer capacity modifier */

    /** Acid production */
    float inflammation_acid_load;        /**< Acid from inflammation */
    float metabolic_acid_load;           /**< Acid from activated cells */

    /** Predicted pH effect */
    float predicted_ph_change;           /**< Predicted pH change */
} ph_immune_ph_effect_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t ph_updates;                 /**< pH state updates */
    uint64_t immune_updates;             /**< Immune state updates */
    uint64_t microglia_activations;      /**< Microglial activation events */
    uint64_t astrocyte_activations;      /**< Astrocyte reactivity events */
    uint64_t cytokine_events;            /**< Cytokine release events */
    float total_inflammation_acid;       /**< Total acid from inflammation */
    float avg_m1_m2_balance;             /**< Average M1/M2 balance */
    float avg_inflammation_index;        /**< Average inflammation index */
    float time_in_acidosis_ms;           /**< Time with pH < 7.35 */
    float last_update_ms;                /**< Last update timestamp */
} ph_immune_stats_t;

/** Opaque bridge handle */
typedef struct ph_immune_bridge_struct ph_immune_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_immune_default_config(ph_immune_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create pH-immune bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT ph_immune_bridge_t* ph_immune_bridge_create(
    const ph_immune_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void ph_immune_bridge_destroy(ph_immune_bridge_t* bridge);

//=============================================================================
// pH Input API (pH Dynamics -> Immune)
//=============================================================================

/**
 * @brief Update pH state from pH dynamics module
 *
 * WHAT: Receives current pH values from pH dynamics
 * WHY:  pH state determines immune response modulation
 * HOW:  Evaluates thresholds, updates glial activation
 *
 * @param bridge Bridge handle
 * @param extracellular_ph Current extracellular pH
 * @param intracellular_ph Current intracellular pH
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_immune_update_ph(
    ph_immune_bridge_t* bridge,
    float extracellular_ph,
    float intracellular_ph
);

/**
 * @brief Set regional pH for immune modulation
 *
 * @param bridge Bridge handle
 * @param region_id Region identifier
 * @param local_ph Local pH value
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_immune_set_region_ph(
    ph_immune_bridge_t* bridge,
    uint32_t region_id,
    float local_ph
);

/**
 * @brief Compute pH effect on immune function
 *
 * WHAT: Calculates pH-dependent immune modulation
 * WHY:  pH affects microglial and astrocyte responses
 * HOW:  Evaluates activation thresholds and polarization
 *
 * @param bridge Bridge handle
 * @param effect Output immune effect
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_immune_compute_effect(
    const ph_immune_bridge_t* bridge,
    ph_immune_effect_t* effect
);

//=============================================================================
// Immune Input API (Immune -> pH)
//=============================================================================

/**
 * @brief Update inflammation state for region
 *
 * WHAT: Receives inflammation state from immune system
 * WHY:  Inflammation produces local acidosis
 * HOW:  Calculates acid production from immune activity
 *
 * @param bridge Bridge handle
 * @param region_id Region identifier
 * @param inflammation_level Inflammation severity
 * @param inflammation_index Quantitative index (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_immune_update_inflammation(
    ph_immune_bridge_t* bridge,
    uint32_t region_id,
    ph_immune_inflammation_level_t inflammation_level,
    float inflammation_index
);

/**
 * @brief Update cytokine concentration
 *
 * @param bridge Bridge handle
 * @param cytokine Cytokine type
 * @param concentration Concentration level
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_immune_update_cytokine(
    ph_immune_bridge_t* bridge,
    ph_immune_cytokine_t cytokine,
    float concentration
);

/**
 * @brief Update microglial state
 *
 * @param bridge Bridge handle
 * @param region_id Region identifier
 * @param state Microglial activation state
 * @param m1_m2_balance M1/M2 polarization balance
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_immune_update_microglia(
    ph_immune_bridge_t* bridge,
    uint32_t region_id,
    ph_immune_microglia_state_t state,
    float m1_m2_balance
);

/**
 * @brief Compute immune effect on pH regulation
 *
 * @param bridge Bridge handle
 * @param effect Output pH regulation effect
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_immune_compute_ph_effect(
    const ph_immune_bridge_t* bridge,
    ph_immune_ph_effect_t* effect
);

//=============================================================================
// State Query API
//=============================================================================

/**
 * @brief Get microglial state for region
 *
 * @param bridge Bridge handle
 * @param region_id Region identifier
 * @param info Output microglial info
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_immune_get_microglia_state(
    const ph_immune_bridge_t* bridge,
    uint32_t region_id,
    ph_immune_microglia_info_t* info
);

/**
 * @brief Get astrocyte state for region
 *
 * @param bridge Bridge handle
 * @param region_id Region identifier
 * @param info Output astrocyte info
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_immune_get_astrocyte_state(
    const ph_immune_bridge_t* bridge,
    uint32_t region_id,
    ph_immune_astrocyte_info_t* info
);

/**
 * @brief Get regional inflammation state
 *
 * @param bridge Bridge handle
 * @param region_id Region identifier
 * @param state Output region state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_immune_get_region_state(
    const ph_immune_bridge_t* bridge,
    uint32_t region_id,
    ph_immune_region_state_t* state
);

/**
 * @brief Get cytokine state
 *
 * @param bridge Bridge handle
 * @param cytokine Cytokine type
 * @param state Output cytokine state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_immune_get_cytokine_state(
    const ph_immune_bridge_t* bridge,
    ph_immune_cytokine_t cytokine,
    ph_immune_cytokine_state_t* state
);

/**
 * @brief Get inflammation-induced acid load
 *
 * @param bridge Bridge handle
 * @param acid_load Output: acid load from inflammation
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_immune_get_inflammation_acid(
    const ph_immune_bridge_t* bridge,
    float* acid_load
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Process pH and immune state changes
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_immune_update(
    ph_immune_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ph_immune_reset(ph_immune_bridge_t* bridge);

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
NIMCP_EXPORT int ph_immune_get_stats(
    const ph_immune_bridge_t* bridge,
    ph_immune_stats_t* stats
);

/**
 * @brief Check if inflammation is active
 *
 * @param bridge Bridge handle
 * @return true if any region has active inflammation
 */
NIMCP_EXPORT bool ph_immune_is_inflamed(
    const ph_immune_bridge_t* bridge
);

/**
 * @brief Check if microglia are activated
 *
 * @param bridge Bridge handle
 * @return true if microglia in M1 or M2 state
 */
NIMCP_EXPORT bool ph_immune_microglia_activated(
    const ph_immune_bridge_t* bridge
);

/**
 * @brief Check if pH is triggering immune response
 *
 * @param bridge Bridge handle
 * @return true if pH below immune activation threshold
 */
NIMCP_EXPORT bool ph_immune_ph_triggering_response(
    const ph_immune_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PH_IMMUNE_BRIDGE_H */