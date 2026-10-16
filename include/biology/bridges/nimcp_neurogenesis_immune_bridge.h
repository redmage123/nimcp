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
// nimcp_neurogenesis_immune_bridge.h - Neurogenesis to Brain Immune System Bridge
//=============================================================================
/**
 * @file nimcp_neurogenesis_immune_bridge.h
 * @brief Bridge between neurogenesis and brain immune system
 *
 * WHAT: Connects neurogenesis with brain immune surveillance, coordinating
 *       microglia-mediated pruning, cytokine modulation, and inflammatory
 *       effects on new neuron development and survival.
 *
 * WHY:  Bridges the gap between:
 *       - Neurogenesis (stem cells, differentiation, integration)
 *       - Brain immune (microglia, cytokines, inflammation)
 *       - Synaptic pruning (complement-tagged elimination)
 *
 * HOW:  Bidirectional integration:
 *       1. Neurogenesis -> Immune: New neurons trigger surveillance
 *       2. Immune -> Neurogenesis: Cytokines modulate proliferation
 *       3. Microglia prune inactive new neuron synapses
 *       4. Inflammation suppresses neurogenesis
 *
 * BIOLOGICAL BASIS:
 * ```
 * NEUROGENESIS                          BRAIN IMMUNE SYSTEM
 * -----------------------------------------------------------------
 * Stem cell proliferation            <- IL-6/TNF-alpha modulation
 * Neuronal differentiation           <- Microglial phagocytosis
 * Synaptic integration               <- Complement-mediated pruning
 * Activity-dependent survival        <- Cytokine survival signals
 * Dead neuron clearance              -> Microglial engulfment
 * Niche maintenance                  <- Anti-inflammatory support
 * ```
 *
 * IMMUNE MODULATION OF NEUROGENESIS:
 * - Pro-inflammatory: IL-1beta, IL-6, TNF-alpha suppress proliferation
 * - Anti-inflammatory: IL-4, IL-10, TGF-beta support neurogenesis
 * - Microglia: Prune synapses, clear debris, provide trophic support
 * - Complement: C1q/C3 tag synapses for elimination
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_NEUROGENESIS_IMMUNE_BRIDGE_H
#define NIMCP_NEUROGENESIS_IMMUNE_BRIDGE_H

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
#define NEUROGENESIS_IMMUNE_MODULE_NAME         "neurogenesis_immune_bridge"

/** Maximum tracked cytokine types */
#define NEUROGENESIS_IMMUNE_MAX_CYTOKINES       16

/** Maximum microglia interactions per update */
#define NEUROGENESIS_IMMUNE_MAX_MICROGLIA       256

/** Default inflammation threshold for suppression */
#define NEUROGENESIS_IMMUNE_INFLAM_THRESH       0.5f

/** Default complement tagging threshold */
#define NEUROGENESIS_IMMUNE_COMP_THRESH         0.3f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Cytokine types affecting neurogenesis
 */
typedef enum {
    NG_IMMUNE_IL1_BETA = 0,          /**< IL-1beta (pro-inflammatory) */
    NG_IMMUNE_IL6,                   /**< IL-6 (pro-inflammatory) */
    NG_IMMUNE_TNF_ALPHA,             /**< TNF-alpha (pro-inflammatory) */
    NG_IMMUNE_IFN_GAMMA,             /**< IFN-gamma (mixed effects) */
    NG_IMMUNE_IL4,                   /**< IL-4 (anti-inflammatory) */
    NG_IMMUNE_IL10,                  /**< IL-10 (anti-inflammatory) */
    NG_IMMUNE_TGF_BETA,              /**< TGF-beta (anti-inflammatory) */
    NG_IMMUNE_BDNF_MICROGLIA,        /**< Microglial BDNF */
    NG_IMMUNE_CYTOKINE_COUNT
} ng_immune_cytokine_t;

/**
 * @brief Microglial activation state
 */
typedef enum {
    NG_MICROGLIA_RESTING = 0,        /**< Resting/surveying */
    NG_MICROGLIA_M1,                 /**< Pro-inflammatory (M1) */
    NG_MICROGLIA_M2,                 /**< Anti-inflammatory (M2) */
    NG_MICROGLIA_PHAGOCYTIC          /**< Actively phagocytosing */
} ng_microglia_state_t;

/**
 * @brief Complement tagging status
 */
typedef enum {
    NG_COMP_NONE = 0,                /**< No complement tagging */
    NG_COMP_C1Q_TAGGED,              /**< C1q bound (early tag) */
    NG_COMP_C3_TAGGED,               /**< C3 bound (primed for pruning) */
    NG_COMP_MAC_ACTIVE               /**< MAC forming (lysis) */
} ng_complement_status_t;

/**
 * @brief Inflammatory state for neurogenesis
 */
typedef enum {
    NG_INFLAM_NONE = 0,              /**< No inflammation */
    NG_INFLAM_LOW,                   /**< Low/physiological */
    NG_INFLAM_MODERATE,              /**< Moderate (suppressive) */
    NG_INFLAM_HIGH,                  /**< High (strongly suppressive) */
    NG_INFLAM_CHRONIC                /**< Chronic inflammation */
} ng_inflammatory_state_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for neurogenesis-immune bridge
 */
typedef struct {
    /** Cytokine thresholds */
    float proinflam_suppression_threshold;   /**< When pro-inflam suppresses */
    float proinflam_suppression_strength;    /**< Suppression magnitude */
    float antiinflam_support_threshold;      /**< When anti-inflam supports */
    float antiinflam_support_strength;       /**< Support magnitude */

    /** Microglial pruning */
    float microglia_pruning_threshold;       /**< Activity threshold for pruning */
    float microglia_pruning_rate;            /**< Synapses pruned per step */
    float microglia_trophic_support;         /**< Trophic factor from M2 */
    bool enable_complement_pruning;          /**< Use complement tagging */

    /** Complement system */
    float c1q_tagging_threshold;             /**< When C1q tags synapses */
    float c3_conversion_rate;                /**< C1q -> C3 conversion rate */
    float complement_decay_rate;             /**< Complement tag decay */

    /** Inflammation effects */
    float inflammation_proliferation_penalty; /**< Proliferation reduction */
    float inflammation_survival_penalty;      /**< Survival reduction */
    float chronic_inflammation_threshold;     /**< When inflammation becomes chronic */

    /** Update parameters */
    float update_interval_ms;                /**< Bridge update interval */
    bool enable_logging;
    bool enable_metrics;
} ng_immune_config_t;

/**
 * @brief Cytokine environment state
 */
typedef struct {
    float levels[NG_IMMUNE_CYTOKINE_COUNT];  /**< Cytokine levels */
    float proinflam_sum;                      /**< Sum of pro-inflammatory */
    float antiinflam_sum;                     /**< Sum of anti-inflammatory */
    float balance;                            /**< Pro/anti balance (-1 to 1) */
    ng_inflammatory_state_t state;           /**< Overall inflammatory state */
    uint64_t last_update;                    /**< Last update time */
} ng_immune_cytokine_state_t;

/**
 * @brief Microglial interaction with neuron
 */
typedef struct {
    uint32_t microglia_id;                   /**< Microglia identifier */
    uint32_t neuron_id;                      /**< Target neuron */
    ng_microglia_state_t activation;         /**< Activation state */
    float proximity;                         /**< Distance to neuron */
    float phagocytic_activity;               /**< Phagocytosis rate */
    float trophic_support;                   /**< Trophic factor provided */
    uint32_t synapses_pruned;                /**< Synapses removed */
    bool is_engaged;                         /**< Currently interacting */
} ng_immune_microglia_interaction_t;

/**
 * @brief Synapse complement status
 */
typedef struct {
    uint32_t synapse_id;                     /**< Synapse identifier */
    uint32_t neuron_id;                      /**< Parent neuron */
    ng_complement_status_t status;           /**< Complement status */
    float c1q_level;                         /**< C1q binding level */
    float c3_level;                          /**< C3 binding level */
    float activity_level;                    /**< Recent activity */
    bool marked_for_pruning;                 /**< Scheduled for removal */
    uint64_t tag_time;                       /**< When tagged */
} ng_immune_complement_state_t;

/**
 * @brief Neuron immune state
 */
typedef struct {
    uint32_t neuron_id;                      /**< Neuron identifier */
    float cytokine_exposure;                 /**< Local cytokine level */
    float survival_modifier;                 /**< Immune effect on survival */
    float proliferation_modifier;            /**< Effect on local proliferation */
    uint32_t complement_tagged_synapses;     /**< Synapses with complement */
    uint32_t synapses_pruned;                /**< Synapses removed by microglia */
    float trophic_support_received;          /**< Trophic support from M2 */
    bool under_surveillance;                 /**< Microglia monitoring */
} ng_immune_neuron_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t microglia_interactions;         /**< Total microglia interactions */
    uint64_t synapses_pruned;                /**< Synapses pruned by microglia */
    uint64_t synapses_complement_tagged;     /**< Synapses tagged by complement */
    uint64_t neurons_phagocytosed;           /**< Dead neurons cleared */
    uint64_t trophic_support_events;         /**< M2 trophic support events */
    float avg_inflammation;                   /**< Average inflammation level */
    float avg_cytokine_balance;              /**< Average pro/anti balance */
    float proliferation_suppression;         /**< Total proliferation suppressed */
    float survival_modification;             /**< Net survival modification */
    ng_inflammatory_state_t current_state;   /**< Current inflammatory state */
    uint64_t update_count;                   /**< Total updates */
    float last_update_ms;                    /**< Last update timestamp */
} ng_immune_stats_t;

/** Opaque bridge handle */
typedef struct ng_immune_bridge_struct ng_immune_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_immune_default_config(ng_immune_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create neurogenesis-immune bridge
 *
 * WHAT: Creates bridge for immune modulation of neurogenesis
 * WHY:  Enable cytokine and microglial regulation of new neurons
 * HOW:  Tracks cytokines, coordinates microglia, manages complement
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT ng_immune_bridge_t* ng_immune_bridge_create(
    const ng_immune_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void ng_immune_bridge_destroy(ng_immune_bridge_t* bridge);

//=============================================================================
// Cytokine API
//=============================================================================

/**
 * @brief Set cytokine level
 *
 * WHAT: Updates cytokine concentration
 * WHY:  Cytokines modulate neurogenesis
 * HOW:  Sets level, recalculates inflammatory state
 *
 * @param bridge Bridge handle
 * @param cytokine Cytokine type
 * @param level Concentration level (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_immune_set_cytokine(
    ng_immune_bridge_t* bridge,
    ng_immune_cytokine_t cytokine,
    float level
);

/**
 * @brief Get cytokine level
 *
 * @param bridge Bridge handle
 * @param cytokine Cytokine type
 * @return Cytokine level
 */
NIMCP_EXPORT float ng_immune_get_cytokine(
    const ng_immune_bridge_t* bridge,
    ng_immune_cytokine_t cytokine
);

/**
 * @brief Get full cytokine state
 *
 * @param bridge Bridge handle
 * @param state Output state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_immune_get_cytokine_state(
    const ng_immune_bridge_t* bridge,
    ng_immune_cytokine_state_t* state
);

/**
 * @brief Get proliferation modifier from immune state
 *
 * WHAT: Returns immune effect on proliferation rate
 * WHY:  Inflammation suppresses neurogenesis
 * HOW:  Based on cytokine balance
 *
 * @param bridge Bridge handle
 * @return Proliferation modifier (0-1, 1 = no effect)
 */
NIMCP_EXPORT float ng_immune_get_proliferation_modifier(
    const ng_immune_bridge_t* bridge
);

//=============================================================================
// Microglia API
//=============================================================================

/**
 * @brief Register microglia interaction with neuron
 *
 * WHAT: Records microglial surveillance of new neuron
 * WHY:  Track microglia-neuron interactions
 * HOW:  Creates interaction record
 *
 * @param bridge Bridge handle
 * @param microglia_id Microglia identifier
 * @param neuron_id Target neuron
 * @param activation Microglia activation state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_immune_register_microglia(
    ng_immune_bridge_t* bridge,
    uint32_t microglia_id,
    uint32_t neuron_id,
    ng_microglia_state_t activation
);

/**
 * @brief Request synapse pruning
 *
 * WHAT: Requests microglial pruning of weak synapse
 * WHY:  Remove inactive connections
 * HOW:  Schedules pruning based on activity
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse to prune
 * @param neuron_id Parent neuron
 * @param activity Synapse activity level
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_immune_request_pruning(
    ng_immune_bridge_t* bridge,
    uint32_t synapse_id,
    uint32_t neuron_id,
    float activity
);

/**
 * @brief Process pending microglial actions
 *
 * WHAT: Executes scheduled microglial pruning and clearance
 * WHY:  Periodic immune processing
 * HOW:  Prunes tagged synapses, clears debris
 *
 * @param bridge Bridge handle
 * @return Number of actions processed
 */
NIMCP_EXPORT int ng_immune_process_microglia(ng_immune_bridge_t* bridge);

/**
 * @brief Get microglial trophic support level
 *
 * WHAT: Returns M2 microglia trophic support
 * WHY:  Anti-inflammatory microglia support neurogenesis
 * HOW:  Based on M2 population and proximity
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @return Trophic support level
 */
NIMCP_EXPORT float ng_immune_get_trophic_support(
    const ng_immune_bridge_t* bridge,
    uint32_t neuron_id
);

//=============================================================================
// Complement API
//=============================================================================

/**
 * @brief Tag synapse with complement
 *
 * WHAT: Applies C1q/C3 tagging to synapse
 * WHY:  Marks synapses for microglial pruning
 * HOW:  Based on activity level vs threshold
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse identifier
 * @param neuron_id Parent neuron
 * @param activity Synapse activity level
 * @return Complement status applied
 */
NIMCP_EXPORT ng_complement_status_t ng_immune_tag_synapse(
    ng_immune_bridge_t* bridge,
    uint32_t synapse_id,
    uint32_t neuron_id,
    float activity
);

/**
 * @brief Get synapse complement state
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse identifier
 * @param state Output state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_immune_get_complement_state(
    const ng_immune_bridge_t* bridge,
    uint32_t synapse_id,
    ng_immune_complement_state_t* state
);

/**
 * @brief Process complement cascade
 *
 * WHAT: Advances complement tagging (C1q -> C3)
 * WHY:  Complement cascade progresses over time
 * HOW:  Converts C1q to C3, decays old tags
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step
 * @return Number of status changes
 */
NIMCP_EXPORT int ng_immune_process_complement(
    ng_immune_bridge_t* bridge,
    float dt_ms
);

//=============================================================================
// Neuron State API
//=============================================================================

/**
 * @brief Register neuron for immune tracking
 *
 * WHAT: Adds neuron to immune surveillance
 * WHY:  Track immune effects on new neurons
 * HOW:  Creates neuron immune state entry
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_immune_register_neuron(
    ng_immune_bridge_t* bridge,
    uint32_t neuron_id
);

/**
 * @brief Get neuron immune state
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @param state Output state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_immune_get_neuron_state(
    const ng_immune_bridge_t* bridge,
    uint32_t neuron_id,
    ng_immune_neuron_state_t* state
);

/**
 * @brief Get survival modifier from immune state
 *
 * WHAT: Returns immune effect on neuron survival
 * WHY:  Immune system affects survival probability
 * HOW:  Combines cytokine, microglia, complement effects
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @return Survival modifier
 */
NIMCP_EXPORT float ng_immune_get_survival_modifier(
    const ng_immune_bridge_t* bridge,
    uint32_t neuron_id
);

/**
 * @brief Report neuron death for clearance
 *
 * WHAT: Notifies immune system of dead neuron
 * WHY:  Microglia clear dead neurons
 * HOW:  Schedules phagocytosis
 *
 * @param bridge Bridge handle
 * @param neuron_id Dead neuron
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_immune_report_death(
    ng_immune_bridge_t* bridge,
    uint32_t neuron_id
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Process microglia, complement, update inflammation
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_immune_update(
    ng_immune_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_immune_reset(ng_immune_bridge_t* bridge);

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
NIMCP_EXPORT int ng_immune_get_stats(
    const ng_immune_bridge_t* bridge,
    ng_immune_stats_t* stats
);

/**
 * @brief Get cytokine name string
 *
 * @param cytokine Cytokine type
 * @return Cytokine name
 */
NIMCP_EXPORT const char* ng_immune_cytokine_name(ng_immune_cytokine_t cytokine);

/**
 * @brief Get microglia state name string
 *
 * @param state Microglia state
 * @return State name
 */
NIMCP_EXPORT const char* ng_immune_microglia_state_name(ng_microglia_state_t state);

/**
 * @brief Get inflammatory state name string
 *
 * @param state Inflammatory state
 * @return State name
 */
NIMCP_EXPORT const char* ng_immune_inflammatory_state_name(ng_inflammatory_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROGENESIS_IMMUNE_BRIDGE_H */