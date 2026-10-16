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
// nimcp_no_plasticity_bridge.h - Nitric Oxide to Plasticity (LTP/LTD) Bridge
//=============================================================================
/**
 * @file nimcp_no_plasticity_bridge.h
 * @brief Bidirectional bridge between Nitric Oxide signaling and synaptic plasticity
 *
 * WHAT: Connects NO gasotransmitter signaling with LTP/LTD plasticity mechanisms.
 *
 * WHY:  Nitric oxide is essential for NMDA-dependent LTP:
 *       - Acts as retrograde messenger from post- to presynaptic terminal
 *       - Required for early-phase LTP expression
 *       - cGMP/PKG pathway modulates plasticity thresholds
 *       - Volume transmission enables heterosynaptic plasticity
 *
 * HOW:  Integration pathways:
 *       1. NO → LTP: cGMP-dependent enhancement of presynaptic release
 *       2. NO → LTD: Low NO levels permissive for LTD induction
 *       3. NO threshold: Sets sliding threshold for plasticity (BCM-like)
 *       4. Metaplasticity: NO history affects future plasticity
 *
 * BIOLOGICAL BASIS:
 * ```
 * PLASTICITY MECHANISMS                    NO SIGNALING
 * ─────────────────────────────────────────────────────────────────
 * Strong tetanus (LTP induction)        → High Ca2+ → nNOS activation
 * NO release                            → cGMP → PKG → presynaptic
 * PKG activation                        → Enhanced vesicle release
 * Weak stimulation (LTD condition)      → Low NO → permissive for LTD
 * Prior activity (metaplasticity)       → NO "tag" affects future LTP
 * ```
 *
 * NO-PLASTICITY RELATIONSHIP:
 * - High NO: Promotes LTP, blocks LTD
 * - Low NO: Permissive for LTD
 * - NO history: Sets metaplastic threshold
 * - Volume transmission: Heterosynaptic tagging
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_NO_PLASTICITY_BRIDGE_H
#define NIMCP_NO_PLASTICITY_BRIDGE_H

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
#define NO_PLASTICITY_MODULE_NAME       "no_plasticity_bridge"

/** Maximum tracked synapses */
#define NO_PLASTICITY_MAX_SYNAPSES      1024

/** Default NO threshold for LTP enhancement */
#define NO_PLASTICITY_LTP_THRESHOLD     50.0f   /* nM */

/** Default NO threshold for LTD permissiveness */
#define NO_PLASTICITY_LTD_THRESHOLD     10.0f   /* nM */

/** Default cGMP saturation level */
#define NO_PLASTICITY_CGMP_SATURATION   10.0f   /* uM */

/** Default metaplasticity decay (ms) */
#define NO_PLASTICITY_META_DECAY        60000.0f

/** Bio-async module ID */
#define BIO_MODULE_NO_PLASTICITY        0x0E02

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Plasticity state influenced by NO
 */
typedef enum {
    NO_PLAST_STATE_NEUTRAL = 0,     /**< No plasticity bias */
    NO_PLAST_STATE_LTP_PRONE,       /**< Favoring LTP */
    NO_PLAST_STATE_LTD_PRONE,       /**< Favoring LTD */
    NO_PLAST_STATE_SATURATED,       /**< Plasticity saturated */
    NO_PLAST_STATE_REFRACTORY       /**< Plasticity refractory */
} no_plasticity_state_t;

/**
 * @brief NO-plasticity coupling mode
 */
typedef enum {
    NO_PLAST_MODE_CLASSICAL = 0,    /**< Classical NO-LTP coupling */
    NO_PLAST_MODE_BCM,              /**< BCM-like sliding threshold */
    NO_PLAST_MODE_METAPLASTIC,      /**< Full metaplasticity support */
    NO_PLAST_MODE_HETEROSYNAPTIC    /**< Volume transmission tagging */
} no_plasticity_mode_t;

/**
 * @brief Plasticity phase affected by NO
 */
typedef enum {
    NO_PLAST_PHASE_INDUCTION = 0,   /**< Affects LTP/LTD induction */
    NO_PLAST_PHASE_EXPRESSION,      /**< Affects plasticity expression */
    NO_PLAST_PHASE_CONSOLIDATION,   /**< Affects late-phase consolidation */
    NO_PLAST_PHASE_ALL              /**< Affects all phases */
} no_plasticity_phase_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for NO-Plasticity bridge
 */
typedef struct {
    /** NO thresholds */
    float ltp_no_threshold_nm;           /**< NO threshold for LTP (nM) */
    float ltd_no_threshold_nm;           /**< NO threshold for LTD (nM) */
    float cgmp_saturation_um;            /**< cGMP saturation level (uM) */

    /** Plasticity modulation */
    no_plasticity_mode_t mode;           /**< Coupling mode */
    no_plasticity_phase_t phase;         /**< Phase to modulate */
    float ltp_enhancement_max;           /**< Maximum LTP enhancement */
    float ltd_suppression_max;           /**< Maximum LTD suppression */

    /** BCM/Metaplasticity */
    bool enable_bcm_sliding;             /**< Enable BCM threshold sliding */
    float bcm_tau_ms;                    /**< BCM threshold time constant */
    float meta_decay_ms;                 /**< Metaplastic tag decay */
    float meta_no_weight;                /**< Weight of NO history */

    /** Heterosynaptic */
    bool enable_heterosynaptic;          /**< Enable volume transmission */
    float hetero_radius_um;              /**< Heterosynaptic radius */
    float hetero_strength;               /**< Heterosynaptic effect strength */

    /** Timing */
    float update_interval_ms;            /**< Update interval */

    /** Features */
    bool enable_bio_async;               /**< Bio-async messaging */
} no_plasticity_config_t;

/**
 * @brief Synapse plasticity state influenced by NO
 */
typedef struct {
    uint32_t synapse_id;                 /**< Synapse identifier */
    uint32_t no_source_id;               /**< Associated NO source */

    /** Current NO effects */
    float local_no_nm;                   /**< Local NO concentration */
    float local_cgmp_um;                 /**< Local cGMP level */
    no_plasticity_state_t state;         /**< Current plasticity state */

    /** Modulation factors */
    float ltp_modifier;                  /**< LTP magnitude modifier */
    float ltd_modifier;                  /**< LTD magnitude modifier */
    float threshold_modifier;            /**< Plasticity threshold shift */

    /** Metaplastic state */
    float no_history;                    /**< Integrated NO history */
    float bcm_threshold;                 /**< Current BCM threshold */
    float meta_tag;                      /**< Metaplastic tag value */
    float last_plasticity_ms;            /**< Last plasticity event time */

    /** Heterosynaptic */
    bool hetero_tagged;                  /**< Tagged by nearby activity */
    float hetero_influence;              /**< Heterosynaptic influence */
} no_plasticity_synapse_t;

/**
 * @brief LTP/LTD event modulated by NO
 */
typedef struct {
    uint32_t synapse_id;                 /**< Synapse ID */
    float pre_no_level;                  /**< NO before event */
    float post_no_level;                 /**< NO after event */

    /** Plasticity outcome */
    float raw_delta_w;                   /**< Raw weight change */
    float no_modifier;                   /**< NO-based modifier applied */
    float final_delta_w;                 /**< Final weight change */

    /** Flags */
    bool was_ltp;                        /**< True if LTP, false if LTD */
    bool no_gated;                       /**< NO gated the event */
    bool heterosynaptic;                 /**< Heterosynaptic event */

    float event_time_ms;
} no_plasticity_event_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;              /**< Total bridge updates */
    uint64_t ltp_events;                 /**< LTP events processed */
    uint64_t ltd_events;                 /**< LTD events processed */
    uint64_t no_enhanced_ltp;            /**< LTP enhanced by NO */
    uint64_t no_suppressed_ltd;          /**< LTD suppressed by NO */
    uint64_t no_gated_events;            /**< Events gated by NO */
    uint64_t heterosynaptic_events;      /**< Heterosynaptic events */

    uint32_t ltp_prone_synapses;         /**< Synapses in LTP-prone state */
    uint32_t ltd_prone_synapses;         /**< Synapses in LTD-prone state */

    float mean_ltp_modifier;             /**< Average LTP modification */
    float mean_ltd_modifier;             /**< Average LTD modification */
    float mean_no_level;                 /**< Average NO concentration */
    float last_update_ms;
} no_plasticity_stats_t;

/** Opaque bridge handle */
typedef struct no_plasticity_bridge_struct no_plasticity_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_plasticity_default_config(no_plasticity_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create NO-Plasticity bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT no_plasticity_bridge_t* no_plasticity_bridge_create(
    const no_plasticity_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
NIMCP_EXPORT void no_plasticity_bridge_destroy(no_plasticity_bridge_t* bridge);

//=============================================================================
// Synapse Management API
//=============================================================================

/**
 * @brief Register synapse for NO-plasticity modulation
 *
 * WHAT: Adds synapse to NO-plasticity tracking
 * WHY:  Track NO influence on individual synapses
 * HOW:  Creates synapse entry with initial state
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse identifier
 * @param no_source_id Associated NO source (or 0 for ambient)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_plasticity_register_synapse(
    no_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    uint32_t no_source_id
);

/**
 * @brief Unregister synapse
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse to remove
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_plasticity_unregister_synapse(
    no_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Update NO level for synapse
 *
 * WHAT: Sets current NO concentration at synapse
 * WHY:  NO level determines plasticity modulation
 * HOW:  Updates synapse state and modifiers
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param no_level_nm NO concentration (nM)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_plasticity_set_no_level(
    no_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float no_level_nm
);

//=============================================================================
// Plasticity Modulation API
//=============================================================================

/**
 * @brief Get LTP modifier for synapse
 *
 * WHAT: Returns NO-based LTP magnitude modifier
 * WHY:  NO enhances LTP through cGMP/PKG pathway
 * HOW:  Calculates based on local NO and cGMP
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param[out] ltp_modifier LTP magnitude modifier (1.0 = unchanged)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_plasticity_get_ltp_modifier(
    const no_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float* ltp_modifier
);

/**
 * @brief Get LTD modifier for synapse
 *
 * WHAT: Returns NO-based LTD magnitude modifier
 * WHY:  Low NO is permissive for LTD; high NO blocks LTD
 * HOW:  Inverse relationship to NO level
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param[out] ltd_modifier LTD magnitude modifier (1.0 = unchanged)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_plasticity_get_ltd_modifier(
    const no_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float* ltd_modifier
);

/**
 * @brief Get plasticity threshold modifier
 *
 * WHAT: Returns NO-based threshold for LTP/LTD crossover
 * WHY:  NO shifts the BCM sliding threshold
 * HOW:  Integrates NO history for metaplasticity
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param[out] threshold_mod Threshold modification factor
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_plasticity_get_threshold_mod(
    const no_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float* threshold_mod
);

/**
 * @brief Modulate plasticity event with NO
 *
 * WHAT: Applies NO modulation to pending plasticity event
 * WHY:  Central function for NO-plasticity coupling
 * HOW:  Combines LTP/LTD modifiers with raw weight change
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param raw_delta_w Raw weight change (positive=LTP, negative=LTD)
 * @param[out] modulated_delta_w NO-modulated weight change
 * @param[out] event Event details (optional)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_plasticity_modulate(
    no_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float raw_delta_w,
    float* modulated_delta_w,
    no_plasticity_event_t* event
);

//=============================================================================
// Metaplasticity API
//=============================================================================

/**
 * @brief Get current BCM threshold for synapse
 *
 * WHAT: Returns the sliding LTP/LTD crossover threshold
 * WHY:  BCM threshold tracks postsynaptic activity history
 * HOW:  NO modulates the threshold position
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param[out] bcm_threshold Current BCM threshold
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_plasticity_get_bcm_threshold(
    const no_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float* bcm_threshold
);

/**
 * @brief Update metaplastic state with NO history
 *
 * WHAT: Updates metaplasticity based on NO time course
 * WHY:  Prior NO levels affect future plasticity
 * HOW:  Integrates NO with exponential decay
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param dt_ms Time delta
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_plasticity_update_metastate(
    no_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float dt_ms
);

/**
 * @brief Get metaplastic tag value
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param[out] meta_tag Metaplastic tag value
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_plasticity_get_meta_tag(
    const no_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float* meta_tag
);

//=============================================================================
// Heterosynaptic API
//=============================================================================

/**
 * @brief Tag synapses within volume transmission radius
 *
 * WHAT: Marks nearby synapses for heterosynaptic effects
 * WHY:  NO diffuses ~100um, affecting multiple synapses
 * HOW:  Calculates spatial neighborhood, sets tags
 *
 * @param bridge Bridge handle
 * @param source_synapse Source of NO release
 * @param no_concentration NO level at source
 * @return Number of synapses tagged
 */
NIMCP_EXPORT int no_plasticity_tag_heterosynaptic(
    no_plasticity_bridge_t* bridge,
    uint32_t source_synapse,
    float no_concentration
);

/**
 * @brief Get heterosynaptic influence on synapse
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param[out] influence Heterosynaptic influence factor
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_plasticity_get_hetero_influence(
    const no_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float* influence
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of NO-plasticity integration
 * WHY:  Decay metaplastic tags, update BCM thresholds
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_plasticity_update(
    no_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_plasticity_reset(no_plasticity_bridge_t* bridge);

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
NIMCP_EXPORT int no_plasticity_get_stats(
    const no_plasticity_bridge_t* bridge,
    no_plasticity_stats_t* stats
);

/**
 * @brief Get synapse plasticity state
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param[out] synapse Synapse state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_plasticity_get_synapse(
    const no_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    no_plasticity_synapse_t* synapse
);

/**
 * @brief Get current plasticity state for synapse
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @return Plasticity state
 */
NIMCP_EXPORT no_plasticity_state_t no_plasticity_get_state(
    const no_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Check if synapse is in LTP-prone state
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @return true if LTP-prone
 */
NIMCP_EXPORT bool no_plasticity_is_ltp_prone(
    const no_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NO_PLASTICITY_BRIDGE_H */