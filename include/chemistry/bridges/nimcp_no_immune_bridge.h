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
// nimcp_no_immune_bridge.h - Nitric Oxide to Immune System Bridge
//=============================================================================
/**
 * @file nimcp_no_immune_bridge.h
 * @brief Bidirectional bridge between Nitric Oxide signaling and brain immune
 *        system for inflammation and neuroimmune modulation
 *
 * WHAT: Connects NO gasotransmitter signaling with the brain immune system
 *       for inflammation regulation and neuroimmune coordination.
 *
 * WHY:  Nitric oxide is central to neuroimmune interactions:
 *       - iNOS produces high NO during inflammation (pro-inflammatory)
 *       - Low NO from nNOS/eNOS is anti-inflammatory
 *       - NO is cytotoxic to pathogens but also to neurons (double-edged)
 *       - NO modulates microglial activation and cytokine release
 *       - Vascular NO affects immune cell trafficking
 *
 * HOW:  Bidirectional integration:
 *       1. Immune → NO: Inflammatory cytokines induce iNOS
 *       2. NO → Immune: NO modulates microglial phenotype (M1/M2)
 *       3. NO → Cytotoxic: High NO causes oxidative/nitrosative stress
 *       4. eNOS → Vascular: Blood flow affects immune cell infiltration
 *
 * BIOLOGICAL BASIS:
 * ```
 * IMMUNE SYSTEM                            NO SIGNALING
 * ─────────────────────────────────────────────────────────────────
 * Cytokines (TNF-α, IL-1β, IFN-γ)       → iNOS induction
 * Microglial activation (M1)            → High NO production
 * Anti-inflammatory (M2)                → Low NO, more eNOS
 * Oxidative stress                      ← Peroxynitrite (NO + O2-)
 * BBB permeability                      ← eNOS-mediated vasodilation
 * Phagocytosis                          ← NO enhances in microglia
 * ```
 *
 * NOS ISOFORM IMMUNE ROLES:
 * - nNOS: Neuronal, constitutive, low output, neuromodulation
 * - eNOS: Endothelial, constitutive, vascular, immune trafficking
 * - iNOS: Inducible, high output, cytotoxic, inflammatory
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_NO_IMMUNE_BRIDGE_H
#define NIMCP_NO_IMMUNE_BRIDGE_H

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
#define NO_IMMUNE_MODULE_NAME           "no_immune_bridge"

/** Maximum immune cells tracked */
#define NO_IMMUNE_MAX_CELLS             256

/** Maximum inflammatory foci */
#define NO_IMMUNE_MAX_FOCI              64

/** iNOS induction threshold (cytokine units) */
#define NO_IMMUNE_INOS_THRESHOLD        0.3f

/** Cytotoxic NO threshold (nM) */
#define NO_IMMUNE_CYTOTOXIC_THRESHOLD   500.0f

/** Pathological NO level (nM) */
#define NO_IMMUNE_PATHOLOGICAL_LEVEL    1000.0f

/** Bio-async module ID */
#define BIO_MODULE_NO_IMMUNE            0x0E06

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Microglial phenotype
 */
typedef enum {
    NO_IMMUNE_MICROGLIA_RESTING = 0,    /**< Resting (ramified) */
    NO_IMMUNE_MICROGLIA_M1,             /**< Pro-inflammatory M1 */
    NO_IMMUNE_MICROGLIA_M2,             /**< Anti-inflammatory M2 */
    NO_IMMUNE_MICROGLIA_DAM             /**< Disease-associated microglia */
} no_immune_microglia_t;

/**
 * @brief Inflammation level
 */
typedef enum {
    NO_IMMUNE_INFLAM_NONE = 0,          /**< No inflammation */
    NO_IMMUNE_INFLAM_LOW,               /**< Mild inflammation */
    NO_IMMUNE_INFLAM_MODERATE,          /**< Moderate inflammation */
    NO_IMMUNE_INFLAM_SEVERE,            /**< Severe inflammation */
    NO_IMMUNE_INFLAM_STORM              /**< Cytokine storm */
} no_immune_inflammation_t;

/**
 * @brief NO-mediated damage type
 */
typedef enum {
    NO_IMMUNE_DAMAGE_NONE = 0,          /**< No damage */
    NO_IMMUNE_DAMAGE_OXIDATIVE,         /**< Oxidative stress */
    NO_IMMUNE_DAMAGE_NITROSATIVE,       /**< Nitrosative stress */
    NO_IMMUNE_DAMAGE_EXCITOTOXIC,       /**< NO-enhanced excitotoxicity */
    NO_IMMUNE_DAMAGE_MITOCHONDRIAL      /**< Mitochondrial dysfunction */
} no_immune_damage_t;

/**
 * @brief Cytokine type affecting NO
 */
typedef enum {
    NO_IMMUNE_CYTOKINE_TNF = 0,         /**< TNF-alpha (iNOS inducer) */
    NO_IMMUNE_CYTOKINE_IL1,             /**< IL-1beta (iNOS inducer) */
    NO_IMMUNE_CYTOKINE_IFNG,            /**< IFN-gamma (iNOS inducer) */
    NO_IMMUNE_CYTOKINE_IL6,             /**< IL-6 (mixed effects) */
    NO_IMMUNE_CYTOKINE_IL10,            /**< IL-10 (anti-inflammatory) */
    NO_IMMUNE_CYTOKINE_TGFB             /**< TGF-beta (anti-inflammatory) */
} no_immune_cytokine_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for NO-Immune bridge
 */
typedef struct {
    /** iNOS induction */
    float inos_induction_threshold;          /**< Cytokine threshold for iNOS */
    float inos_max_output_nm;                /**< Maximum iNOS NO output */
    float inos_induction_delay_ms;           /**< iNOS induction delay */
    float inos_duration_ms;                  /**< iNOS activity duration */

    /** Cytotoxicity thresholds */
    float cytotoxic_no_threshold_nm;         /**< NO for cytotoxic effects */
    float oxidative_stress_threshold;        /**< ROS for damage */
    float pathological_no_nm;                /**< Pathological NO level */

    /** Microglial modulation */
    bool enable_microglia_modulation;        /**< Enable M1/M2 switching */
    float m1_no_threshold;                   /**< NO for M1 phenotype */
    float m2_no_range_max;                   /**< Max NO for M2 phenotype */

    /** Vascular/BBB effects */
    bool enable_bbb_effects;                 /**< Enable BBB modulation */
    float bbb_permeability_scale;            /**< NO effect on BBB */

    /** Anti-inflammatory */
    float il10_inos_suppression;             /**< IL-10 iNOS suppression */
    float tgfb_inos_suppression;             /**< TGF-b iNOS suppression */

    /** Features */
    bool enable_bio_async;                   /**< Bio-async messaging */
    float update_interval_ms;                /**< Update interval */
} no_immune_config_t;

/**
 * @brief Cytokine state influencing NO
 */
typedef struct {
    float tnf_alpha;                         /**< TNF-alpha level (0-1) */
    float il1_beta;                          /**< IL-1beta level (0-1) */
    float ifn_gamma;                         /**< IFN-gamma level (0-1) */
    float il6;                               /**< IL-6 level (0-1) */
    float il10;                              /**< IL-10 level (0-1) */
    float tgf_beta;                          /**< TGF-beta level (0-1) */

    float composite_proinflam;               /**< Combined pro-inflammatory */
    float composite_antiinflam;              /**< Combined anti-inflammatory */
    float net_inflammatory;                  /**< Net inflammatory signal */
} no_immune_cytokines_t;

/**
 * @brief Immune cell with NO modulation
 */
typedef struct {
    uint32_t cell_id;                        /**< Cell ID */
    float position[3];                       /**< Position (um) */

    /** Cell type/state */
    no_immune_microglia_t phenotype;         /**< Microglial phenotype */
    float activation_level;                  /**< Activation (0-1) */

    /** NO production */
    float nos_activity;                      /**< NOS activity (0-1) */
    bool inos_induced;                       /**< iNOS is induced */
    float inos_induction_time;               /**< Time since iNOS induction */
    float no_production_rate;                /**< NO production rate */
    float local_no_nm;                       /**< Local NO concentration */

    /** Immune effects */
    float phagocytic_activity;               /**< Phagocytosis rate */
    float cytokine_production;               /**< Cytokine output */
    float ros_production;                    /**< ROS production */

    /** Damage tracking */
    no_immune_damage_t damage_type;          /**< Current damage type */
    float damage_severity;                   /**< Damage severity (0-1) */

    bool active;
} no_immune_cell_t;

/**
 * @brief Inflammatory focus/lesion
 */
typedef struct {
    uint32_t focus_id;                       /**< Focus ID */
    float center[3];                         /**< Center position (um) */
    float radius_um;                         /**< Focus radius */

    /** Inflammation state */
    no_immune_inflammation_t level;          /**< Inflammation level */
    float intensity;                         /**< Intensity (0-1) */
    float duration_ms;                       /**< Duration so far */

    /** NO state */
    float mean_no_nm;                        /**< Mean NO in focus */
    float peak_no_nm;                        /**< Peak NO observed */
    float inos_cell_count;                   /**< iNOS+ cell count */

    /** Cytokines */
    no_immune_cytokines_t cytokines;         /**< Local cytokine state */

    /** Damage */
    float oxidative_stress;                  /**< Oxidative stress level */
    float neuronal_damage;                   /**< Neuronal damage index */
    float bbb_permeability;                  /**< BBB permeability */
} no_immune_focus_t;

/**
 * @brief NO-Immune interaction event
 */
typedef struct {
    uint32_t cell_id;                        /**< Cell ID (or 0 for focus) */
    uint32_t focus_id;                       /**< Focus ID (or 0 for cell) */

    /** Event type */
    bool inos_induced;                       /**< iNOS was induced */
    bool phenotype_switched;                 /**< M1/M2 switch occurred */
    bool damage_occurred;                    /**< Damage was inflicted */

    /** Values */
    float no_level;                          /**< NO at event */
    no_immune_microglia_t new_phenotype;     /**< New phenotype (if switched) */
    no_immune_damage_t damage_type;          /**< Damage type (if occurred) */
    float damage_amount;                     /**< Damage amount */

    float event_time_ms;
} no_immune_event_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;                  /**< Total updates */
    uint64_t inos_inductions;                /**< iNOS induction events */
    uint64_t phenotype_switches;             /**< M1/M2 switches */
    uint64_t damage_events;                  /**< Damage events */
    uint64_t resolution_events;              /**< Inflammation resolutions */

    uint32_t active_cells;                   /**< Active immune cells */
    uint32_t m1_cells;                       /**< M1 phenotype count */
    uint32_t m2_cells;                       /**< M2 phenotype count */
    uint32_t inos_positive;                  /**< iNOS+ cells */
    uint32_t active_foci;                    /**< Active inflammatory foci */

    float mean_no_level;                     /**< System mean NO */
    float total_inflammatory;                /**< Total inflammatory signal */
    float total_damage;                      /**< Cumulative damage */
    float bbb_integrity;                     /**< BBB integrity (1.0 = normal) */

    float last_update_ms;
} no_immune_stats_t;

/** Opaque bridge handle */
typedef struct no_immune_bridge_struct no_immune_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_immune_default_config(no_immune_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create NO-Immune bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT no_immune_bridge_t* no_immune_bridge_create(
    const no_immune_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
NIMCP_EXPORT void no_immune_bridge_destroy(no_immune_bridge_t* bridge);

//=============================================================================
// Immune Cell Management API
//=============================================================================

/**
 * @brief Register immune cell (microglia)
 *
 * WHAT: Adds immune cell for NO-immune modulation
 * WHY:  Microglia produce and respond to NO
 * HOW:  Creates cell entry with phenotype state
 *
 * @param bridge Bridge handle
 * @param cell_id Cell ID
 * @param position 3D position [x, y, z] in um
 * @param phenotype Initial phenotype
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_immune_register_cell(
    no_immune_bridge_t* bridge,
    uint32_t cell_id,
    const float position[3],
    no_immune_microglia_t phenotype
);

/**
 * @brief Unregister immune cell
 *
 * @param bridge Bridge handle
 * @param cell_id Cell to remove
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_immune_unregister_cell(
    no_immune_bridge_t* bridge,
    uint32_t cell_id
);

/**
 * @brief Create inflammatory focus
 *
 * WHAT: Creates localized inflammatory region
 * WHY:  Inflammation is spatially organized
 * HOW:  Defines focus with center and radius
 *
 * @param bridge Bridge handle
 * @param center Center position [x, y, z]
 * @param radius_um Focus radius
 * @param initial_level Initial inflammation level
 * @param[out] focus_id Assigned focus ID
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_immune_create_focus(
    no_immune_bridge_t* bridge,
    const float center[3],
    float radius_um,
    no_immune_inflammation_t initial_level,
    uint32_t* focus_id
);

/**
 * @brief Resolve inflammatory focus
 *
 * @param bridge Bridge handle
 * @param focus_id Focus to resolve
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_immune_resolve_focus(
    no_immune_bridge_t* bridge,
    uint32_t focus_id
);

//=============================================================================
// Cytokine -> NO API
//=============================================================================

/**
 * @brief Set cytokine levels for cell
 *
 * WHAT: Updates cytokine environment at cell
 * WHY:  Cytokines induce iNOS and modulate phenotype
 * HOW:  Sets local cytokine state
 *
 * @param bridge Bridge handle
 * @param cell_id Cell ID
 * @param cytokines Cytokine levels
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_immune_set_cytokines(
    no_immune_bridge_t* bridge,
    uint32_t cell_id,
    const no_immune_cytokines_t* cytokines
);

/**
 * @brief Set single cytokine level
 *
 * @param bridge Bridge handle
 * @param cell_id Cell ID
 * @param cytokine Cytokine type
 * @param level Cytokine level (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_immune_set_cytokine(
    no_immune_bridge_t* bridge,
    uint32_t cell_id,
    no_immune_cytokine_t cytokine,
    float level
);

/**
 * @brief Induce iNOS in cell
 *
 * WHAT: Triggers iNOS induction in immune cell
 * WHY:  iNOS produces high NO for pathogen killing
 * HOW:  Activates iNOS with delay and duration
 *
 * @param bridge Bridge handle
 * @param cell_id Cell ID
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_immune_induce_inos(
    no_immune_bridge_t* bridge,
    uint32_t cell_id
);

/**
 * @brief Check if iNOS is induced
 *
 * @param bridge Bridge handle
 * @param cell_id Cell ID
 * @return true if iNOS is induced
 */
NIMCP_EXPORT bool no_immune_is_inos_induced(
    const no_immune_bridge_t* bridge,
    uint32_t cell_id
);

//=============================================================================
// NO -> Immune API
//=============================================================================

/**
 * @brief Get microglial phenotype
 *
 * WHAT: Returns current M1/M2 phenotype
 * WHY:  NO influences phenotype switching
 * HOW:  Based on local NO and cytokine environment
 *
 * @param bridge Bridge handle
 * @param cell_id Cell ID
 * @return Current phenotype
 */
NIMCP_EXPORT no_immune_microglia_t no_immune_get_phenotype(
    const no_immune_bridge_t* bridge,
    uint32_t cell_id
);

/**
 * @brief Get NO production rate for cell
 *
 * @param bridge Bridge handle
 * @param cell_id Cell ID
 * @param[out] production_rate NO production rate
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_immune_get_no_production(
    const no_immune_bridge_t* bridge,
    uint32_t cell_id,
    float* production_rate
);

/**
 * @brief Get inflammation level at focus
 *
 * @param bridge Bridge handle
 * @param focus_id Focus ID
 * @return Inflammation level
 */
NIMCP_EXPORT no_immune_inflammation_t no_immune_get_inflammation(
    const no_immune_bridge_t* bridge,
    uint32_t focus_id
);

/**
 * @brief Get damage assessment for focus
 *
 * WHAT: Returns NO-mediated damage in focus
 * WHY:  High NO causes oxidative/nitrosative damage
 * HOW:  Integrates NO exposure over time
 *
 * @param bridge Bridge handle
 * @param focus_id Focus ID
 * @param[out] damage_type Type of damage
 * @param[out] severity Damage severity (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_immune_get_damage(
    const no_immune_bridge_t* bridge,
    uint32_t focus_id,
    no_immune_damage_t* damage_type,
    float* severity
);

//=============================================================================
// BBB/Vascular API
//=============================================================================

/**
 * @brief Get BBB permeability at position
 *
 * WHAT: Returns NO-modulated BBB permeability
 * WHY:  eNOS affects vascular tone and BBB
 * HOW:  Calculates based on local NO and inflammation
 *
 * @param bridge Bridge handle
 * @param position Position to check
 * @param[out] permeability BBB permeability (1.0 = normal)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_immune_get_bbb_permeability(
    const no_immune_bridge_t* bridge,
    const float position[3],
    float* permeability
);

/**
 * @brief Get system-wide BBB integrity
 *
 * @param bridge Bridge handle
 * @param[out] integrity BBB integrity (1.0 = fully intact)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_immune_get_bbb_integrity(
    const no_immune_bridge_t* bridge,
    float* integrity
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of NO-immune integration
 * WHY:  Advance iNOS dynamics, phenotype, damage
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_immune_update(
    no_immune_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_immune_reset(no_immune_bridge_t* bridge);

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
NIMCP_EXPORT int no_immune_get_stats(
    const no_immune_bridge_t* bridge,
    no_immune_stats_t* stats
);

/**
 * @brief Get cell state
 *
 * @param bridge Bridge handle
 * @param cell_id Cell ID
 * @param[out] cell Cell state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_immune_get_cell(
    const no_immune_bridge_t* bridge,
    uint32_t cell_id,
    no_immune_cell_t* cell
);

/**
 * @brief Get focus state
 *
 * @param bridge Bridge handle
 * @param focus_id Focus ID
 * @param[out] focus Focus state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_immune_get_focus(
    const no_immune_bridge_t* bridge,
    uint32_t focus_id,
    no_immune_focus_t* focus
);

/**
 * @brief Check if any inflammation is active
 *
 * @param bridge Bridge handle
 * @return true if inflammation present
 */
NIMCP_EXPORT bool no_immune_has_inflammation(
    const no_immune_bridge_t* bridge
);

/**
 * @brief Check if pathological NO levels present
 *
 * @param bridge Bridge handle
 * @return true if pathological NO
 */
NIMCP_EXPORT bool no_immune_is_pathological(
    const no_immune_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NO_IMMUNE_BRIDGE_H */