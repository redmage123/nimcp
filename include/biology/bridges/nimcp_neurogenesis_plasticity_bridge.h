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
// nimcp_neurogenesis_plasticity_bridge.h - Neurogenesis to Plasticity Bridge
//=============================================================================
/**
 * @file nimcp_neurogenesis_plasticity_bridge.h
 * @brief Bridge between neurogenesis and synaptic plasticity systems
 *
 * WHAT: Connects neurogenesis with STDP/plasticity mechanisms, coordinating
 *       learning rules for new neurons during their critical integration period.
 *
 * WHY:  Bridges the gap between:
 *       - Neurogenesis (new neuron development stages)
 *       - Synaptic plasticity (STDP, metaplasticity)
 *       - Critical period dynamics (enhanced plasticity windows)
 *
 * HOW:  Bidirectional integration:
 *       1. Neurogenesis -> Plasticity: New neurons have enhanced plasticity
 *       2. Plasticity -> Neurogenesis: Learning success affects survival
 *       3. Critical period modulation of STDP parameters
 *       4. Metaplasticity adjustments for network stability
 *
 * BIOLOGICAL BASIS:
 * ```
 * NEUROGENESIS                          PLASTICITY
 * -----------------------------------------------------------------
 * Immature neurons                   -> Enhanced LTP/LTD windows
 * Integration period (4-6 weeks)     -> Critical period plasticity
 * Synapse formation                  -> Initial weight setting
 * Activity-dependent survival        -> Learning success criteria
 * Dendritic growth                   -> Structural plasticity
 * BDNF signaling                     -> Plasticity threshold modulation
 * ```
 *
 * CRITICAL PERIOD PLASTICITY:
 * - New neurons: Lower LTP threshold, wider STDP windows
 * - Gradual normalization as neuron matures
 * - Competition for synaptic resources
 * - Metaplastic stabilization post-integration
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_NEUROGENESIS_PLASTICITY_BRIDGE_H
#define NIMCP_NEUROGENESIS_PLASTICITY_BRIDGE_H

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
#define NEUROGENESIS_PLASTICITY_MODULE_NAME     "neurogenesis_plasticity_bridge"

/** Maximum synapses with enhanced plasticity */
#define NEUROGENESIS_PLASTICITY_MAX_SYNAPSES    2048

/** Default critical period duration (steps) */
#define NEUROGENESIS_PLASTICITY_CRITICAL_PERIOD 1000

/** Default LTP enhancement for immature neurons */
#define NEUROGENESIS_PLASTICITY_LTP_ENHANCE     2.0f

/** Default STDP window expansion factor */
#define NEUROGENESIS_PLASTICITY_WINDOW_EXPAND   1.5f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Plasticity enhancement mode for new neurons
 */
typedef enum {
    NG_PLAST_ENHANCE_LTP = 0,        /**< Enhanced LTP only */
    NG_PLAST_ENHANCE_LTD,            /**< Enhanced LTD only */
    NG_PLAST_ENHANCE_BOTH,           /**< Enhanced LTP and LTD */
    NG_PLAST_ENHANCE_WINDOW          /**< Wider STDP windows */
} ng_plasticity_enhance_mode_t;

/**
 * @brief Critical period phase
 */
typedef enum {
    NG_PLAST_PHASE_SILENT = 0,       /**< Pre-integration, no plasticity */
    NG_PLAST_PHASE_OPENING,          /**< Critical period opening */
    NG_PLAST_PHASE_PEAK,             /**< Peak plasticity */
    NG_PLAST_PHASE_CLOSING,          /**< Critical period closing */
    NG_PLAST_PHASE_MATURE            /**< Normal adult plasticity */
} ng_plasticity_phase_t;

/**
 * @brief Metaplasticity rule
 */
typedef enum {
    NG_PLAST_META_BCM = 0,           /**< BCM-like sliding threshold */
    NG_PLAST_META_HOMEOSTATIC,       /**< Homeostatic scaling */
    NG_PLAST_META_SYNAPTIC_TAG,      /**< Synaptic tagging */
    NG_PLAST_META_NONE               /**< No metaplasticity */
} ng_plasticity_meta_rule_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for neurogenesis-plasticity bridge
 */
typedef struct {
    /** Critical period parameters */
    uint32_t critical_period_duration;   /**< Duration in simulation steps */
    float opening_fraction;              /**< Fraction for opening phase */
    float peak_fraction;                 /**< Fraction at peak phase */
    float closing_fraction;              /**< Fraction for closing phase */

    /** Plasticity enhancement */
    ng_plasticity_enhance_mode_t enhance_mode; /**< What to enhance */
    float ltp_enhancement;               /**< LTP magnitude multiplier */
    float ltd_enhancement;               /**< LTD magnitude multiplier */
    float stdp_window_expansion;         /**< STDP window width multiplier */
    float threshold_reduction;           /**< LTP threshold reduction */

    /** Metaplasticity */
    ng_plasticity_meta_rule_t meta_rule; /**< Metaplasticity rule */
    float meta_time_constant;            /**< Metaplasticity time constant */
    float target_activity;               /**< Target activity for homeostasis */

    /** Survival coupling */
    bool enable_survival_coupling;       /**< Learning affects survival */
    float learning_survival_weight;      /**< Weight of learning for survival */
    float min_potentiation_threshold;    /**< Min LTP for survival credit */

    /** Structural plasticity */
    bool enable_structural_plasticity;   /**< Allow synapse addition/removal */
    float structural_plasticity_rate;    /**< Rate of structural changes */
    uint32_t min_synapses;               /**< Minimum synapses to maintain */

    /** Update parameters */
    float update_interval_ms;            /**< Bridge update interval */
    bool enable_logging;
    bool enable_metrics;
} ng_plasticity_config_t;

/**
 * @brief Synapse plasticity state for new neuron
 */
typedef struct {
    uint32_t synapse_id;                 /**< Synapse identifier */
    uint32_t neuron_id;                  /**< Parent new neuron ID */
    float current_weight;                /**< Current synaptic weight */
    float plasticity_multiplier;         /**< Current enhancement factor */
    float ltp_accumulated;               /**< Accumulated LTP */
    float ltd_accumulated;               /**< Accumulated LTD */
    float sliding_threshold;             /**< BCM sliding threshold */
    ng_plasticity_phase_t phase;         /**< Current critical period phase */
    bool is_tagged;                      /**< Synaptic tag state */
    float tag_strength;                  /**< Tag strength (for capture) */
} ng_plasticity_synapse_state_t;

/**
 * @brief Neuron plasticity summary
 */
typedef struct {
    uint32_t neuron_id;                  /**< Neuron identifier */
    ng_plasticity_phase_t phase;         /**< Current phase */
    float phase_progress;                /**< Progress through current phase */
    uint32_t synapse_count;              /**< Total synapses */
    float mean_weight;                   /**< Mean synaptic weight */
    float total_ltp;                     /**< Total LTP received */
    float total_ltd;                     /**< Total LTD received */
    float learning_score;                /**< Learning success metric */
    float plasticity_multiplier;         /**< Current plasticity enhancement */
} ng_plasticity_neuron_summary_t;

/**
 * @brief Plasticity event for new neuron synapse
 */
typedef struct {
    uint32_t synapse_id;                 /**< Synapse identifier */
    uint32_t neuron_id;                  /**< Parent neuron ID */
    float weight_change;                 /**< Weight change magnitude */
    float enhancement_applied;           /**< Enhancement factor applied */
    bool is_ltp;                         /**< True for LTP, false for LTD */
    float pre_weight;                    /**< Weight before change */
    float post_weight;                   /**< Weight after change */
} ng_plasticity_event_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t plasticity_events;          /**< Total plasticity events */
    uint64_t ltp_events;                 /**< LTP events */
    uint64_t ltd_events;                 /**< LTD events */
    uint64_t synapses_tracked;           /**< Synapses with enhanced plasticity */
    uint64_t structural_additions;       /**< Synapses added */
    uint64_t structural_removals;        /**< Synapses removed */
    float avg_enhancement_factor;        /**< Average enhancement applied */
    float avg_learning_score;            /**< Average learning success */
    uint32_t neurons_in_critical_period; /**< Neurons in critical period */
    float total_weight_change;           /**< Net weight change */
    uint64_t update_count;               /**< Total updates */
    float last_update_ms;                /**< Last update timestamp */
} ng_plasticity_stats_t;

/** Opaque bridge handle */
typedef struct ng_plasticity_bridge_struct ng_plasticity_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_plasticity_default_config(ng_plasticity_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create neurogenesis-plasticity bridge
 *
 * WHAT: Creates bridge for coordinating new neuron plasticity
 * WHY:  Enable critical period enhanced plasticity
 * HOW:  Tracks synapse states, modulates STDP parameters
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT ng_plasticity_bridge_t* ng_plasticity_bridge_create(
    const ng_plasticity_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void ng_plasticity_bridge_destroy(ng_plasticity_bridge_t* bridge);

//=============================================================================
// Neuron Registration API
//=============================================================================

/**
 * @brief Register new neuron for enhanced plasticity
 *
 * WHAT: Starts critical period plasticity tracking for new neuron
 * WHY:  New neurons require enhanced plasticity for integration
 * HOW:  Initializes plasticity parameters with enhancement factors
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_plasticity_register_neuron(
    ng_plasticity_bridge_t* bridge,
    uint32_t neuron_id
);

/**
 * @brief Register synapse for plasticity tracking
 *
 * WHAT: Adds synapse to enhanced plasticity tracking
 * WHY:  Track individual synapse plasticity during critical period
 * HOW:  Creates synapse state with current phase parameters
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse identifier
 * @param neuron_id Parent neuron ID
 * @param initial_weight Initial synaptic weight
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_plasticity_register_synapse(
    ng_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    uint32_t neuron_id,
    float initial_weight
);

/**
 * @brief Get neuron plasticity summary
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @param summary Output summary
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_plasticity_get_neuron_summary(
    const ng_plasticity_bridge_t* bridge,
    uint32_t neuron_id,
    ng_plasticity_neuron_summary_t* summary
);

/**
 * @brief Get synapse plasticity state
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse identifier
 * @param state Output state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_plasticity_get_synapse_state(
    const ng_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    ng_plasticity_synapse_state_t* state
);

//=============================================================================
// Plasticity Modulation API
//=============================================================================

/**
 * @brief Get plasticity parameters for synapse
 *
 * WHAT: Returns enhanced STDP parameters for synapse
 * WHY:  New neuron synapses need modified plasticity rules
 * HOW:  Applies phase-dependent enhancement factors
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse identifier
 * @param ltp_mult Output LTP multiplier
 * @param ltd_mult Output LTD multiplier
 * @param window_mult Output STDP window multiplier
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_plasticity_get_parameters(
    const ng_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float* ltp_mult,
    float* ltd_mult,
    float* window_mult
);

/**
 * @brief Apply plasticity event
 *
 * WHAT: Records plasticity event with enhancement
 * WHY:  Track learning progress for survival coupling
 * HOW:  Accumulates LTP/LTD, updates learning score
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse identifier
 * @param weight_change Raw weight change (positive = LTP)
 * @param event Output event record (optional)
 * @return Enhanced weight change
 */
NIMCP_EXPORT float ng_plasticity_apply_event(
    ng_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float weight_change,
    ng_plasticity_event_t* event
);

/**
 * @brief Update sliding threshold (BCM metaplasticity)
 *
 * WHAT: Updates BCM-like sliding threshold
 * WHY:  Prevent runaway potentiation/depression
 * HOW:  Adjusts threshold based on recent activity
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse identifier
 * @param activity Recent postsynaptic activity
 * @return New sliding threshold
 */
NIMCP_EXPORT float ng_plasticity_update_threshold(
    ng_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float activity
);

/**
 * @brief Apply synaptic tag for capture
 *
 * WHAT: Sets synaptic tag for late-phase consolidation
 * WHY:  Enable synaptic tagging and capture
 * HOW:  Marks synapse for protein synthesis-dependent LTP
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse identifier
 * @param tag_strength Tag strength (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_plasticity_apply_tag(
    ng_plasticity_bridge_t* bridge,
    uint32_t synapse_id,
    float tag_strength
);

//=============================================================================
// Critical Period API
//=============================================================================

/**
 * @brief Advance critical period phase
 *
 * WHAT: Progresses neuron through critical period phases
 * WHY:  Gradual transition from enhanced to normal plasticity
 * HOW:  Updates phase based on elapsed time
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @param dt_steps Time steps elapsed
 * @return New phase
 */
NIMCP_EXPORT ng_plasticity_phase_t ng_plasticity_advance_phase(
    ng_plasticity_bridge_t* bridge,
    uint32_t neuron_id,
    uint32_t dt_steps
);

/**
 * @brief Force phase transition
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @param phase Target phase
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_plasticity_set_phase(
    ng_plasticity_bridge_t* bridge,
    uint32_t neuron_id,
    ng_plasticity_phase_t phase
);

/**
 * @brief Get learning score for survival coupling
 *
 * WHAT: Returns learning success metric for survival decision
 * WHY:  Successful learning promotes survival
 * HOW:  Aggregates LTP history weighted by recency
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @return Learning score (0-1)
 */
NIMCP_EXPORT float ng_plasticity_get_learning_score(
    const ng_plasticity_bridge_t* bridge,
    uint32_t neuron_id
);

//=============================================================================
// Structural Plasticity API
//=============================================================================

/**
 * @brief Request synapse addition (structural plasticity)
 *
 * WHAT: Requests formation of new synapse
 * WHY:  High activity can trigger new connections
 * HOW:  Queues synapse addition based on activity patterns
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @param target_id Target neuron
 * @param synapse_id Output new synapse ID
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_plasticity_request_synapse(
    ng_plasticity_bridge_t* bridge,
    uint32_t neuron_id,
    uint32_t target_id,
    uint32_t* synapse_id
);

/**
 * @brief Request synapse removal (structural plasticity)
 *
 * WHAT: Marks synapse for removal
 * WHY:  Low activity leads to synapse elimination
 * HOW:  Queues removal based on weight/activity threshold
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse identifier
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_plasticity_remove_synapse(
    ng_plasticity_bridge_t* bridge,
    uint32_t synapse_id
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Advance phases, apply metaplasticity, update stats
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_plasticity_update(
    ng_plasticity_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_plasticity_reset(ng_plasticity_bridge_t* bridge);

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
NIMCP_EXPORT int ng_plasticity_get_stats(
    const ng_plasticity_bridge_t* bridge,
    ng_plasticity_stats_t* stats
);

/**
 * @brief Get neurons in specific phase
 *
 * @param bridge Bridge handle
 * @param phase Phase to query
 * @param ids Output array
 * @param max_ids Maximum IDs to return
 * @return Number of IDs returned
 */
NIMCP_EXPORT int ng_plasticity_get_neurons_by_phase(
    const ng_plasticity_bridge_t* bridge,
    ng_plasticity_phase_t phase,
    uint32_t* ids,
    uint32_t max_ids
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROGENESIS_PLASTICITY_BRIDGE_H */