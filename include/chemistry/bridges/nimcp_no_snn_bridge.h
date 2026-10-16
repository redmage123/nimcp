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
// nimcp_no_snn_bridge.h - Nitric Oxide to SNN Retrograde Signaling Bridge
//=============================================================================
/**
 * @file nimcp_no_snn_bridge.h
 * @brief Bidirectional bridge between Nitric Oxide signaling and SNN systems
 *
 * WHAT: Connects NO gasotransmitter signaling with Spiking Neural Networks
 *       via retrograde volume transmission.
 *
 * WHY:  Nitric oxide is a critical retrograde messenger that:
 *       - Diffuses from postsynaptic to presynaptic terminals
 *       - Modulates presynaptic release probability
 *       - Affects multiple synapses via volume transmission (~100um radius)
 *       - Integrates NMDA receptor activation with network plasticity
 *
 * HOW:  Bidirectional integration:
 *       1. SNN → NO: Postsynaptic activity triggers NOS activation
 *       2. NO → SNN: NO diffusion modulates presynaptic release
 *       3. cGMP cascade affects spike generation threshold
 *       4. Volume transmission creates spatial domains of influence
 *
 * BIOLOGICAL BASIS:
 * ```
 * SNN ACTIVITY                              NO SIGNALING
 * ─────────────────────────────────────────────────────────────────
 * Postsynaptic depolarization           → Ca2+ influx via NMDA
 * NMDA receptor activation              → nNOS activation
 * Ca2+/Calmodulin binding               → NO production
 * NO diffusion (volume transmission)    → Presynaptic modulation
 * cGMP signaling                        → Release probability change
 * ```
 *
 * NO DIFFUSION CHARACTERISTICS:
 * - Half-life: ~1 second
 * - Diffusion coefficient: 3300 um^2/s
 * - Effective radius: ~100 um
 * - Affects multiple synapses within diffusion sphere
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_NO_SNN_BRIDGE_H
#define NIMCP_NO_SNN_BRIDGE_H

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
#define NO_SNN_MODULE_NAME          "no_snn_bridge"

/** Maximum NO sources per bridge */
#define NO_SNN_MAX_SOURCES          256

/** Maximum affected synapses per source */
#define NO_SNN_MAX_AFFECTED         64

/** Default NMDA activation threshold for NOS */
#define NO_SNN_NMDA_THRESHOLD       0.3f

/** Default Ca2+ threshold for nNOS (uM) */
#define NO_SNN_CALCIUM_THRESHOLD    0.5f

/** Default release probability modulation max */
#define NO_SNN_RELEASE_MOD_MAX      2.0f

/** Default cGMP threshold for spike modulation */
#define NO_SNN_CGMP_SPIKE_THRESHOLD 1.0f

/** Bio-async module ID */
#define BIO_MODULE_NO_SNN           0x0E01

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief NO-SNN modulation mode
 */
typedef enum {
    NO_SNN_MOD_NONE = 0,            /**< No modulation */
    NO_SNN_MOD_POTENTIATE,          /**< Increase release probability */
    NO_SNN_MOD_DEPRESS,             /**< Decrease release probability */
    NO_SNN_MOD_THRESHOLD,           /**< Modulate spike threshold */
    NO_SNN_MOD_BILATERAL            /**< Both release and threshold */
} no_snn_mod_mode_t;

/**
 * @brief NO source type based on NOS isoform
 */
typedef enum {
    NO_SNN_SOURCE_NNOS = 0,         /**< Neuronal NOS (activity-dependent) */
    NO_SNN_SOURCE_ENOS,             /**< Endothelial NOS (vascular) */
    NO_SNN_SOURCE_INOS              /**< Inducible NOS (inflammatory) */
} no_snn_source_type_t;

/**
 * @brief Retrograde signal propagation mode
 */
typedef enum {
    NO_SNN_PROP_LOCAL = 0,          /**< Single synapse only */
    NO_SNN_PROP_VOLUME,             /**< Volume transmission (sphere) */
    NO_SNN_PROP_DIRECTED            /**< Directed along axon */
} no_snn_prop_mode_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Configuration for NO-SNN bridge
 */
typedef struct {
    /** Activation parameters */
    float nmda_activation_threshold;     /**< NMDA threshold for NOS */
    float calcium_threshold_um;          /**< Ca2+ threshold (uM) */
    float nos_activation_tau_ms;         /**< NOS activation time constant */

    /** Diffusion parameters */
    float diffusion_radius_um;           /**< NO effective radius */
    float diffusion_coefficient;         /**< um^2/s */
    float no_decay_rate;                 /**< Decay rate (1/s) */

    /** Modulation parameters */
    no_snn_mod_mode_t mod_mode;          /**< Modulation mode */
    float release_mod_max;               /**< Max release probability mod */
    float threshold_shift_mv;            /**< Spike threshold shift */
    float cgmp_sensitivity;              /**< cGMP pathway sensitivity */

    /** Volume transmission */
    no_snn_prop_mode_t propagation;      /**< Signal propagation mode */
    bool enable_spatial_clustering;       /**< Cluster nearby synapses */
    float cluster_radius_um;             /**< Clustering radius */

    /** Feature enables */
    bool enable_retrograde;              /**< Enable retrograde signaling */
    bool enable_spike_modulation;        /**< Enable spike threshold mod */
    bool enable_bio_async;               /**< Enable bio-async messaging */

    /** Update interval */
    float update_interval_ms;            /**< Bridge update interval */
} no_snn_config_t;

/**
 * @brief NO source linked to SNN neuron
 */
typedef struct {
    uint32_t neuron_id;                  /**< Associated SNN neuron */
    uint32_t source_id;                  /**< NO source ID */
    float position[3];                   /**< 3D position (um) */
    no_snn_source_type_t source_type;    /**< NOS isoform */

    /** Activation state */
    float nmda_level;                    /**< Current NMDA activation */
    float calcium_level;                 /**< Ca2+ concentration (uM) */
    float nos_activity;                  /**< NOS activity (0-1) */
    float no_concentration;              /**< Local NO (nM) */

    /** Target tracking */
    uint32_t affected_synapses[NO_SNN_MAX_AFFECTED];
    float synapse_distances[NO_SNN_MAX_AFFECTED];
    uint32_t num_affected;

    bool active;
} no_snn_source_t;

/**
 * @brief NO effect on SNN synapse
 */
typedef struct {
    uint32_t synapse_id;                 /**< Target synapse */
    uint32_t source_id;                  /**< NO source ID */
    float distance_um;                   /**< Distance from source */

    /** Modulation */
    float no_concentration;              /**< Local NO level */
    float cgmp_level;                    /**< cGMP concentration */
    float release_mod;                   /**< Release probability modifier */
    float threshold_mod_mv;              /**< Threshold shift (mV) */

    no_snn_mod_mode_t active_mode;       /**< Current modulation mode */
    float last_update_ms;
} no_snn_effect_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;              /**< Total bridge updates */
    uint64_t nos_activations;            /**< NOS activation events */
    uint64_t no_release_events;          /**< NO release events */
    uint64_t retrograde_events;          /**< Retrograde modulations */
    uint64_t spike_modulations;          /**< Spike threshold changes */

    uint32_t active_sources;             /**< Currently active NO sources */
    uint32_t affected_synapses;          /**< Synapses under NO influence */

    float mean_no_concentration;         /**< Average NO level */
    float mean_release_mod;              /**< Average release modification */
    float total_cgmp_produced;           /**< Total cGMP produced */
    float last_update_ms;                /**< Last update timestamp */
} no_snn_stats_t;

/** Opaque bridge handle */
typedef struct no_snn_bridge_struct no_snn_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_snn_default_config(no_snn_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create NO-SNN bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT no_snn_bridge_t* no_snn_bridge_create(
    const no_snn_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
NIMCP_EXPORT void no_snn_bridge_destroy(no_snn_bridge_t* bridge);

//=============================================================================
// Source Management API
//=============================================================================

/**
 * @brief Register NO source for SNN neuron
 *
 * WHAT: Links NO source to postsynaptic neuron
 * WHY:  nNOS is anchored near NMDA receptors at postsynaptic density
 * HOW:  Creates source entry with spatial position for diffusion
 *
 * @param bridge Bridge handle
 * @param neuron_id SNN neuron ID
 * @param position 3D position [x, y, z] in um
 * @param source_type NOS isoform type
 * @param[out] source_id Assigned source ID
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_snn_register_source(
    no_snn_bridge_t* bridge,
    uint32_t neuron_id,
    const float position[3],
    no_snn_source_type_t source_type,
    uint32_t* source_id
);

/**
 * @brief Unregister NO source
 *
 * @param bridge Bridge handle
 * @param source_id Source to remove
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_snn_unregister_source(
    no_snn_bridge_t* bridge,
    uint32_t source_id
);

/**
 * @brief Link synapse to NO source (within diffusion radius)
 *
 * WHAT: Associates presynaptic terminal with NO source
 * WHY:  NO diffuses to affect nearby presynaptic terminals
 * HOW:  Calculates distance and adds to affected list
 *
 * @param bridge Bridge handle
 * @param source_id NO source ID
 * @param synapse_id Presynaptic synapse ID
 * @param synapse_position 3D position of synapse
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_snn_link_synapse(
    no_snn_bridge_t* bridge,
    uint32_t source_id,
    uint32_t synapse_id,
    const float synapse_position[3]
);

//=============================================================================
// SNN -> NO Activation API
//=============================================================================

/**
 * @brief Report NMDA receptor activation
 *
 * WHAT: Notifies bridge of NMDA activation at source
 * WHY:  NMDA-mediated Ca2+ influx activates nNOS
 * HOW:  Updates source activation state
 *
 * @param bridge Bridge handle
 * @param source_id NO source ID
 * @param nmda_activation NMDA activation level (0-1)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_snn_set_nmda_activation(
    no_snn_bridge_t* bridge,
    uint32_t source_id,
    float nmda_activation
);

/**
 * @brief Report calcium level at source
 *
 * WHAT: Updates intracellular Ca2+ for NOS activation
 * WHY:  nNOS requires Ca2+/calmodulin binding
 * HOW:  Direct calcium level update
 *
 * @param bridge Bridge handle
 * @param source_id NO source ID
 * @param calcium_um Calcium concentration (uM)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_snn_set_calcium(
    no_snn_bridge_t* bridge,
    uint32_t source_id,
    float calcium_um
);

/**
 * @brief Report postsynaptic spike (triggers NO burst)
 *
 * WHAT: Notifies bridge of backpropagating action potential
 * WHY:  bAP + EPSP = supralinear Ca2+ influx = NO burst
 * HOW:  Triggers transient NOS activation
 *
 * @param bridge Bridge handle
 * @param neuron_id Postsynaptic neuron ID
 * @param spike_time_ms Spike time
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_snn_report_spike(
    no_snn_bridge_t* bridge,
    uint32_t neuron_id,
    float spike_time_ms
);

//=============================================================================
// NO -> SNN Modulation API
//=============================================================================

/**
 * @brief Get release probability modifier for synapse
 *
 * WHAT: Returns NO-mediated release probability change
 * WHY:  NO enhances presynaptic glutamate release
 * HOW:  Calculates based on local NO/cGMP concentration
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param[out] release_mod Release probability modifier (1.0 = unchanged)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_snn_get_release_mod(
    const no_snn_bridge_t* bridge,
    uint32_t synapse_id,
    float* release_mod
);

/**
 * @brief Get spike threshold modifier for neuron
 *
 * WHAT: Returns NO-mediated threshold shift
 * WHY:  cGMP can modulate ion channel properties
 * HOW:  Integrates NO effects across all sources
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron ID
 * @param[out] threshold_shift_mv Threshold shift in mV
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_snn_get_threshold_mod(
    const no_snn_bridge_t* bridge,
    uint32_t neuron_id,
    float* threshold_shift_mv
);

/**
 * @brief Get NO effect summary for synapse
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse ID
 * @param[out] effect Effect details
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_snn_get_effect(
    const no_snn_bridge_t* bridge,
    uint32_t synapse_id,
    no_snn_effect_t* effect
);

//=============================================================================
// Diffusion API
//=============================================================================

/**
 * @brief Compute NO diffusion from source to targets
 *
 * WHAT: Calculates NO concentration at each target synapse
 * WHY:  Volume transmission affects multiple synapses
 * HOW:  Spherical diffusion with distance-dependent decay
 *
 * @param bridge Bridge handle
 * @param source_id Source ID
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_snn_compute_diffusion(
    no_snn_bridge_t* bridge,
    uint32_t source_id
);

/**
 * @brief Get synapses within diffusion radius of source
 *
 * @param bridge Bridge handle
 * @param source_id Source ID
 * @param[out] synapse_ids Array to fill with synapse IDs
 * @param max_synapses Maximum synapses to return
 * @return Number of synapses found
 */
NIMCP_EXPORT int no_snn_get_affected_synapses(
    const no_snn_bridge_t* bridge,
    uint32_t source_id,
    uint32_t* synapse_ids,
    uint32_t max_synapses
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of NO-SNN integration
 * WHY:  Advance diffusion, decay, and modulation states
 * HOW:  Called during simulation step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_snn_update(
    no_snn_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_snn_reset(no_snn_bridge_t* bridge);

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
NIMCP_EXPORT int no_snn_get_stats(
    const no_snn_bridge_t* bridge,
    no_snn_stats_t* stats
);

/**
 * @brief Get source by ID
 *
 * @param bridge Bridge handle
 * @param source_id Source ID
 * @param[out] source Source data
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int no_snn_get_source(
    const no_snn_bridge_t* bridge,
    uint32_t source_id,
    no_snn_source_t* source
);

/**
 * @brief Check if source is actively producing NO
 *
 * @param bridge Bridge handle
 * @param source_id Source ID
 * @return true if source is producing NO
 */
NIMCP_EXPORT bool no_snn_is_source_active(
    const no_snn_bridge_t* bridge,
    uint32_t source_id
);

/**
 * @brief Get total NO concentration at position
 *
 * @param bridge Bridge handle
 * @param position 3D position
 * @return NO concentration (nM) at position
 */
NIMCP_EXPORT float no_snn_get_concentration_at(
    const no_snn_bridge_t* bridge,
    const float position[3]
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NO_SNN_BRIDGE_H */