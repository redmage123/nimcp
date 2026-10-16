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
// nimcp_neurogenesis_snn_bridge.h - Neurogenesis to SNN Network Plasticity Bridge
//=============================================================================
/**
 * @file nimcp_neurogenesis_snn_bridge.h
 * @brief Bridge between neurogenesis and spiking neural network systems
 *
 * WHAT: Connects neurogenesis (new neuron creation) with SNN network topology
 *       and plasticity, enabling dynamic network growth and adaptation.
 *
 * WHY:  Bridges the gap between:
 *       - Neurogenesis (stem cells, differentiation, integration)
 *       - SNN topology (neuron addition, synapse formation)
 *       - Network plasticity (activity-dependent survival, pruning)
 *
 * HOW:  Bidirectional integration:
 *       1. Neurogenesis -> SNN: New neurons become network nodes
 *       2. SNN -> Neurogenesis: Network activity modulates proliferation
 *       3. Activity feedback determines neuron survival
 *       4. Integration phase coordinates synapse formation
 *
 * BIOLOGICAL BASIS:
 * ```
 * NEUROGENESIS                          SNN NETWORK
 * -----------------------------------------------------------------
 * Stem cell proliferation            -> Network expansion signals
 * Neuronal differentiation           -> New SNN node creation
 * Synaptic integration (4-6 weeks)   -> Gradual synapse formation
 * Activity-dependent survival        -> Spike rate thresholds
 * Pruning of inactive neurons        -> Node removal from network
 * Hippocampal dentate gyrus         -> Pattern separation layers
 * ```
 *
 * NEW NEURON SNN INTEGRATION:
 * - Immature neurons: High excitability, weak output synapses
 * - Integrating neurons: Gradual synapse strengthening
 * - Mature neurons: Full network participation
 * - Apoptotic neurons: Disconnection and removal
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_NEUROGENESIS_SNN_BRIDGE_H
#define NIMCP_NEUROGENESIS_SNN_BRIDGE_H

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
#define NEUROGENESIS_SNN_MODULE_NAME        "neurogenesis_snn_bridge"

/** Maximum new neurons tracked per update */
#define NEUROGENESIS_SNN_MAX_NEW_NEURONS    256

/** Maximum synapse formation events per update */
#define NEUROGENESIS_SNN_MAX_SYNAPSES       1024

/** Default integration period (simulation steps) */
#define NEUROGENESIS_SNN_INTEGRATION_STEPS  1000

/** Default survival activity threshold */
#define NEUROGENESIS_SNN_SURVIVAL_THRESH    0.1f

/** Default immature neuron excitability boost */
#define NEUROGENESIS_SNN_EXCITABILITY_BOOST 1.5f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Integration strategy for new neurons into SNN
 */
typedef enum {
    NG_SNN_INTEGRATE_GRADUAL = 0,    /**< Gradual synapse formation over time */
    NG_SNN_INTEGRATE_BURST,          /**< Rapid initial connection then pruning */
    NG_SNN_INTEGRATE_COMPETITIVE,    /**< Compete with existing for connections */
    NG_SNN_INTEGRATE_ACTIVITY        /**< Activity-driven selective connection */
} ng_snn_integration_mode_t;

/**
 * @brief Synapse formation strategy
 */
typedef enum {
    NG_SNN_SYNAPSE_RANDOM = 0,       /**< Random target selection */
    NG_SNN_SYNAPSE_PROXIMITY,        /**< Spatially proximal neurons */
    NG_SNN_SYNAPSE_ACTIVITY,         /**< Connect to active neurons */
    NG_SNN_SYNAPSE_HEBBIAN           /**< Co-activity driven formation */
} ng_snn_synapse_strategy_t;

/**
 * @brief New neuron SNN role
 */
typedef enum {
    NG_SNN_ROLE_EXCITATORY = 0,      /**< Excitatory (granule cells) */
    NG_SNN_ROLE_INHIBITORY,          /**< Inhibitory (interneurons) */
    NG_SNN_ROLE_MODULATORY           /**< Modulatory role */
} ng_snn_neuron_role_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for neurogenesis-SNN bridge
 */
typedef struct {
    /** Integration parameters */
    ng_snn_integration_mode_t integration_mode; /**< How neurons integrate */
    uint32_t integration_steps;                  /**< Steps for full integration */
    float synapse_formation_rate;                /**< Synapses per step during integration */
    ng_snn_synapse_strategy_t synapse_strategy; /**< How to select targets */

    /** New neuron properties */
    float immature_excitability;                 /**< Excitability multiplier */
    float initial_weight;                        /**< Initial synapse weight */
    float weight_maturation_rate;                /**< Weight increase per step */
    ng_snn_neuron_role_t default_role;          /**< Default excitatory/inhibitory */

    /** Survival parameters */
    float survival_threshold;                    /**< Activity threshold for survival */
    float survival_window_ms;                    /**< Time window for activity measurement */
    float pruning_delay_steps;                   /**< Steps before pruning inactive */
    bool enable_competitive_survival;            /**< Competition for survival */

    /** Network feedback */
    bool enable_activity_feedback;               /**< SNN activity affects proliferation */
    float activity_proliferation_gain;           /**< How activity boosts neurogenesis */
    float network_sparsity_target;               /**< Target network sparsity */

    /** Update parameters */
    float update_interval_ms;                    /**< Bridge update interval */
    bool enable_logging;
    bool enable_metrics;
} ng_snn_config_t;

/**
 * @brief New neuron SNN integration state
 */
typedef struct {
    uint32_t neuron_id;                 /**< Neurogenesis neuron ID */
    uint32_t snn_node_id;               /**< Corresponding SNN node ID */
    float integration_progress;          /**< 0.0-1.0 integration completion */
    uint32_t afferent_synapses;          /**< Input synapses formed */
    uint32_t efferent_synapses;          /**< Output synapses formed */
    float current_activity;              /**< Recent spike rate */
    float excitability;                  /**< Current excitability multiplier */
    ng_snn_neuron_role_t role;          /**< Excitatory/inhibitory role */
    bool is_active;                      /**< Still active in network */
} ng_snn_neuron_state_t;

/**
 * @brief Synapse formation event
 */
typedef struct {
    uint32_t source_id;                  /**< Presynaptic neuron */
    uint32_t target_id;                  /**< Postsynaptic neuron */
    float initial_weight;                /**< Initial synapse weight */
    bool is_new_neuron_pre;              /**< New neuron is presynaptic */
    bool is_new_neuron_post;             /**< New neuron is postsynaptic */
    float formation_time;                /**< When synapse formed */
} ng_snn_synapse_event_t;

/**
 * @brief Network activity feedback
 */
typedef struct {
    float mean_firing_rate;              /**< Network mean firing rate */
    float network_sparsity;              /**< Fraction of active neurons */
    float pattern_separation;            /**< Pattern separation quality */
    float novelty_signal;                /**< Novelty detection signal */
    bool proliferation_boost;            /**< Should boost neurogenesis */
} ng_snn_network_feedback_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t neurons_integrated;         /**< Total neurons added to SNN */
    uint64_t neurons_pruned;             /**< Neurons removed from SNN */
    uint64_t synapses_formed;            /**< Total synapses created */
    uint64_t synapses_pruned;            /**< Synapses removed */
    float avg_integration_time;          /**< Average integration duration */
    float avg_synapses_per_neuron;       /**< Mean synapses per new neuron */
    float survival_rate;                 /**< Fraction surviving to maturity */
    float current_new_neuron_fraction;   /**< New neurons / total neurons */
    uint64_t update_count;               /**< Total updates performed */
    float last_update_ms;                /**< Last update timestamp */
} ng_snn_stats_t;

/** Opaque bridge handle */
typedef struct ng_snn_bridge_struct ng_snn_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_snn_default_config(ng_snn_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create neurogenesis-SNN bridge
 *
 * WHAT: Creates bridge connecting neurogenesis to SNN network
 * WHY:  Enable dynamic network growth through neurogenesis
 * HOW:  Allocates state, registers callbacks with both systems
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT ng_snn_bridge_t* ng_snn_bridge_create(
    const ng_snn_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy
 */
NIMCP_EXPORT void ng_snn_bridge_destroy(ng_snn_bridge_t* bridge);

//=============================================================================
// Neuron Integration API (Neurogenesis -> SNN)
//=============================================================================

/**
 * @brief Register new neuron for SNN integration
 *
 * WHAT: Notifies bridge of new neuron from neurogenesis
 * WHY:  Initiates integration process into SNN
 * HOW:  Creates SNN node, begins synapse formation phase
 *
 * @param bridge Bridge handle
 * @param neuron_id Neurogenesis neuron ID
 * @param role Excitatory/inhibitory role
 * @param position Spatial position (optional, NULL for auto)
 * @param snn_node_id Output SNN node ID created
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_snn_register_neuron(
    ng_snn_bridge_t* bridge,
    uint32_t neuron_id,
    ng_snn_neuron_role_t role,
    const float* position,
    uint32_t* snn_node_id
);

/**
 * @brief Form synapse for integrating neuron
 *
 * WHAT: Creates new synapse for new neuron
 * WHY:  Builds connectivity during integration phase
 * HOW:  Uses configured strategy (random, proximity, activity)
 *
 * @param bridge Bridge handle
 * @param neuron_id New neuron ID
 * @param is_afferent True for input synapse, false for output
 * @param target_id Target neuron (0 for auto-select)
 * @param event Output synapse event (optional)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_snn_form_synapse(
    ng_snn_bridge_t* bridge,
    uint32_t neuron_id,
    bool is_afferent,
    uint32_t target_id,
    ng_snn_synapse_event_t* event
);

/**
 * @brief Get neuron integration state
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron ID
 * @param state Output state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_snn_get_neuron_state(
    const ng_snn_bridge_t* bridge,
    uint32_t neuron_id,
    ng_snn_neuron_state_t* state
);

/**
 * @brief Complete neuron integration (mature)
 *
 * WHAT: Marks neuron as fully integrated/mature
 * WHY:  Ends integration phase, normal network participation
 * HOW:  Sets full excitability, stops special tracking
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron ID
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_snn_complete_integration(
    ng_snn_bridge_t* bridge,
    uint32_t neuron_id
);

//=============================================================================
// Activity Feedback API (SNN -> Neurogenesis)
//=============================================================================

/**
 * @brief Report activity for new neuron
 *
 * WHAT: Records spike activity for survival assessment
 * WHY:  Activity determines neuron survival
 * HOW:  Accumulates in sliding window for comparison to threshold
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron ID
 * @param spike_count Number of spikes in interval
 * @param interval_ms Time interval
 * @return Current activity level (0.0-1.0)
 */
NIMCP_EXPORT float ng_snn_report_activity(
    ng_snn_bridge_t* bridge,
    uint32_t neuron_id,
    uint32_t spike_count,
    float interval_ms
);

/**
 * @brief Process network feedback for neurogenesis modulation
 *
 * WHAT: Analyzes network state for neurogenesis feedback
 * WHY:  Network needs modulate proliferation rate
 * HOW:  Computes sparsity, novelty, pattern separation metrics
 *
 * @param bridge Bridge handle
 * @param feedback Output feedback signals
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_snn_compute_feedback(
    ng_snn_bridge_t* bridge,
    ng_snn_network_feedback_t* feedback
);

/**
 * @brief Set network activity level (bulk update)
 *
 * @param bridge Bridge handle
 * @param mean_rate Network mean firing rate
 * @param active_fraction Fraction of neurons active
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_snn_set_network_activity(
    ng_snn_bridge_t* bridge,
    float mean_rate,
    float active_fraction
);

//=============================================================================
// Pruning API
//=============================================================================

/**
 * @brief Check neuron for pruning
 *
 * WHAT: Evaluates if neuron should be pruned
 * WHY:  Remove inactive neurons (use it or lose it)
 * HOW:  Compare activity against threshold over window
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron ID
 * @return true if neuron should be pruned
 */
NIMCP_EXPORT bool ng_snn_should_prune(
    const ng_snn_bridge_t* bridge,
    uint32_t neuron_id
);

/**
 * @brief Prune neuron from network
 *
 * WHAT: Removes inactive neuron from SNN
 * WHY:  Implements activity-dependent survival
 * HOW:  Disconnects synapses, removes node, notifies neurogenesis
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron ID
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_snn_prune_neuron(
    ng_snn_bridge_t* bridge,
    uint32_t neuron_id
);

/**
 * @brief Process all pending pruning
 *
 * @param bridge Bridge handle
 * @return Number of neurons pruned
 */
NIMCP_EXPORT int ng_snn_process_pruning(ng_snn_bridge_t* bridge);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of bridge internals
 * WHY:  Progress integration, check survival, update stats
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_snn_update(
    ng_snn_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_snn_reset(ng_snn_bridge_t* bridge);

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
NIMCP_EXPORT int ng_snn_get_stats(
    const ng_snn_bridge_t* bridge,
    ng_snn_stats_t* stats
);

/**
 * @brief Get count of neurons in each integration stage
 *
 * @param bridge Bridge handle
 * @param integrating Output count of integrating neurons
 * @param mature Output count of mature neurons
 * @param pending_prune Output count pending pruning
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int ng_snn_get_stage_counts(
    const ng_snn_bridge_t* bridge,
    uint32_t* integrating,
    uint32_t* mature,
    uint32_t* pending_prune
);

/**
 * @brief Get list of active new neuron IDs
 *
 * @param bridge Bridge handle
 * @param ids Output array
 * @param max_ids Maximum IDs to return
 * @return Number of IDs returned
 */
NIMCP_EXPORT int ng_snn_get_active_neurons(
    const ng_snn_bridge_t* bridge,
    uint32_t* ids,
    uint32_t max_ids
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEUROGENESIS_SNN_BRIDGE_H */