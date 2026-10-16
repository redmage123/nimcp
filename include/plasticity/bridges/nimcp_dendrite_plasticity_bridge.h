/**
 * @file nimcp_dendrite_plasticity_bridge.h
 * @brief Dendrite-Plasticity Bridge - Connects dendritic compartments to plasticity mechanisms
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Bridge connecting dendritic compartments to plasticity orchestration
 * WHY:  Dendrites are isolated from plasticity - learning cannot propagate through dendritic structure
 * HOW:  Routes calcium signals, STDP events, and structural changes between dendrites and plasticity
 *
 * BIOLOGICAL BASIS:
 * - Dendritic spines are the primary sites of excitatory synaptic plasticity
 * - Local calcium dynamics in spines determine LTP/LTD direction
 * - Dendritic branch points exhibit metaplasticity (plasticity of plasticity)
 * - Structural plasticity (spine growth/shrinkage) follows functional changes
 * - Dendritic compartments can independently express STDP
 *
 * INTEGRATION POINTS:
 * - nimcp_dendritic.h: Dendritic plasticity state and compartments
 * - nimcp_plasticity_orchestrator.h: Central plasticity coordination
 * - nimcp_stdp.h: Spike-timing dependent plasticity
 * - nimcp_calcium_dynamics.h: Local calcium signaling
 *
 * DESIGN PATTERNS:
 * - Bridge: Decouples dendrite abstraction from plasticity implementation
 * - Observer: Notifies plasticity of dendritic calcium events
 * - Mediator: Coordinates multi-mechanism plasticity at dendritic sites
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_DENDRITE_PLASTICITY_BRIDGE_H
#define NIMCP_DENDRITE_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "plasticity/dendritic/nimcp_dendritic.h"
#include "plasticity/nimcp_plasticity_orchestrator.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "core/dendrite/nimcp_dendrite.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define DENDRITE_PLASTICITY_MAX_COMPARTMENTS  256   /**< Max dendritic compartments */
#define DENDRITE_PLASTICITY_CALCIUM_WINDOW_MS 100   /**< Calcium integration window */
#define DENDRITE_PLASTICITY_MODULE_NAME       "dendrite_plasticity_bridge"

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct dendrite_plasticity_bridge dendrite_plasticity_bridge_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Dendritic plasticity event types
 *
 * BIOLOGICAL BASIS:
 * Different dendritic events trigger different plasticity cascades
 */
typedef enum {
    DENDRITE_EVENT_CALCIUM_SPIKE = 0,   /**< Local calcium transient */
    DENDRITE_EVENT_BPAP,                /**< Back-propagating action potential */
    DENDRITE_EVENT_EPSP,                /**< Excitatory postsynaptic potential */
    DENDRITE_EVENT_SPINE_GROWTH,        /**< Structural spine enlargement */
    DENDRITE_EVENT_SPINE_SHRINK,        /**< Structural spine shrinkage */
    DENDRITE_EVENT_BRANCH_FORMATION,    /**< New branch point */
    DENDRITE_EVENT_BRANCH_RETRACTION    /**< Branch retraction */
} dendrite_plasticity_event_t;

/**
 * @brief Calcium level thresholds for plasticity direction
 *
 * BIOLOGICAL BASIS:
 * BCM-style sliding threshold determines LTP vs LTD
 */
typedef enum {
    CALCIUM_LEVEL_NONE = 0,     /**< Below LTD threshold */
    CALCIUM_LEVEL_LTD,          /**< Triggers LTD (low-moderate Ca) */
    CALCIUM_LEVEL_NEUTRAL,      /**< No plasticity */
    CALCIUM_LEVEL_LTP           /**< Triggers LTP (high Ca) */
} dendrite_calcium_level_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Dendrite-plasticity bridge configuration
 */
typedef struct {
    /* Calcium thresholds */
    float calcium_ltp_threshold;        /**< Ca level for LTP (default: 0.7) */
    float calcium_ltd_threshold;        /**< Ca level for LTD (default: 0.3) */
    float calcium_decay_tau_ms;         /**< Calcium decay time constant */

    /* STDP parameters */
    float stdp_gain;                    /**< STDP weight change scaling */
    bool enable_stdp;                   /**< Enable STDP at dendrites */

    /* Structural plasticity */
    float spine_growth_threshold;       /**< Activity for spine growth */
    float spine_shrink_threshold;       /**< Inactivity for spine shrinkage */
    bool enable_structural_plasticity;  /**< Enable structural changes */

    /* BCM integration */
    float bcm_sliding_threshold;        /**< Initial BCM threshold */
    float bcm_tau_ms;                   /**< BCM threshold adaptation rate */
    bool enable_bcm;                    /**< Enable BCM metaplasticity */

    /* Bio-async */
    bool enable_bio_async;              /**< Enable bio-async messaging */
    uint32_t inbox_capacity;            /**< Bio-async inbox size */
} dendrite_plasticity_config_t;

/**
 * @brief Per-compartment plasticity state
 */
typedef struct {
    uint32_t compartment_id;            /**< Compartment identifier */
    float calcium_level;                /**< Current calcium concentration */
    float bcm_threshold;                /**< BCM sliding threshold */
    float activity_trace;               /**< Activity history trace */
    float weight_delta_sum;             /**< Accumulated weight changes */
    uint64_t last_spike_time;           /**< Last spike timestamp */
    uint64_t last_calcium_time;         /**< Last calcium event */
    dendrite_calcium_level_t ca_state;  /**< Current calcium state */
} compartment_plasticity_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t calcium_events;            /**< Total calcium events */
    uint64_t stdp_events;               /**< STDP applications */
    uint64_t ltp_events;                /**< LTP inductions */
    uint64_t ltd_events;                /**< LTD inductions */
    uint64_t structural_events;         /**< Structural changes */
    float total_weight_change;          /**< Net weight change */
    float avg_calcium_level;            /**< Average calcium */
    float avg_bcm_threshold;            /**< Average BCM threshold */
} dendrite_plasticity_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Dendrite-plasticity bridge state
 *
 * Connects dendritic compartments to all relevant plasticity mechanisms
 */
struct dendrite_plasticity_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    dendrite_plasticity_config_t config;

    /* Connections */
    dendrite_t* dendrite;                /**< Connected dendritic system */
    plasticity_orchestrator_t* plasticity_orch; /**< Plasticity orchestrator */
    stdp_synapse_t* stdp_template;              /**< STDP template for synapses */

    /* Per-compartment state */
    compartment_plasticity_state_t* compartments;
    size_t num_compartments;
    size_t compartment_capacity;

    /* Statistics */
    dendrite_plasticity_stats_t stats;

    /* State */
    bool initialized;
    uint64_t last_update_time;
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically realistic defaults
 * HOW:  Return struct with parameters based on experimental data
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int dendrite_plasticity_default_config(dendrite_plasticity_config_t* config);

/**
 * @brief Create dendrite-plasticity bridge
 *
 * WHAT: Initialize bridge connecting dendrites to plasticity
 * WHY:  Enable plasticity at dendritic sites
 * HOW:  Allocate structures, set up connections
 *
 * @param config Configuration (NULL for defaults)
 * @param dendrite Dendritic system to connect
 * @param orch Plasticity orchestrator
 * @return New bridge or NULL on failure
 */
dendrite_plasticity_bridge_t* dendrite_plasticity_create(
    const dendrite_plasticity_config_t* config,
    dendrite_t* dendrite,
    plasticity_orchestrator_t* orch);

/**
 * @brief Destroy dendrite-plasticity bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free allocations, disconnect from bio-async
 *
 * @param bridge Bridge to destroy
 */
void dendrite_plasticity_destroy(dendrite_plasticity_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect STDP mechanism
 *
 * WHAT: Link bridge to STDP for timing-dependent plasticity
 * WHY:  Enable STDP at dendritic synapses
 * HOW:  Store STDP template for synapse-level plasticity
 *
 * @param bridge Dendrite-plasticity bridge
 * @param stdp STDP synapse template
 * @return 0 on success
 */
int dendrite_plasticity_connect_stdp(
    dendrite_plasticity_bridge_t* bridge,
    stdp_synapse_t* stdp);

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for plasticity events
 * WHY:  Distributed plasticity coordination
 * HOW:  Register module with bio-router
 *
 * @param bridge Dendrite-plasticity bridge
 * @return 0 on success
 */
int dendrite_plasticity_connect_bio_async(dendrite_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Disable bio-async messaging
 * WHY:  Clean shutdown
 * HOW:  Unregister from bio-router
 *
 * @param bridge Dendrite-plasticity bridge
 * @return 0 on success
 */
int dendrite_plasticity_disconnect_bio_async(dendrite_plasticity_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Dendrite-plasticity bridge
 * @return true if connected
 */
bool dendrite_plasticity_is_bio_async_connected(const dendrite_plasticity_bridge_t* bridge);

/* ============================================================================
 * Calcium Event API
 * ============================================================================ */

/**
 * @brief Update calcium level in compartment
 *
 * WHAT: Process calcium influx at dendritic compartment
 * WHY:  Calcium drives plasticity direction (LTP vs LTD)
 * HOW:  Update level, check thresholds, trigger plasticity
 *
 * BIOLOGICAL BASIS:
 * High Ca → LTP (NMDAR-dependent)
 * Low-moderate Ca → LTD (mGluR-dependent)
 *
 * @param bridge Dendrite-plasticity bridge
 * @param compartment_id Compartment with calcium event
 * @param calcium_influx Amount of calcium influx
 * @return 0 on success
 */
int dendrite_plasticity_update_calcium(
    dendrite_plasticity_bridge_t* bridge,
    uint32_t compartment_id,
    float calcium_influx);

/**
 * @brief Get calcium state for compartment
 *
 * @param bridge Dendrite-plasticity bridge
 * @param compartment_id Compartment to query
 * @return Calcium level classification
 */
dendrite_calcium_level_t dendrite_plasticity_get_calcium_state(
    const dendrite_plasticity_bridge_t* bridge,
    uint32_t compartment_id);

/**
 * @brief Decay calcium in all compartments
 *
 * WHAT: Apply exponential calcium decay
 * WHY:  Model calcium clearance over time
 * HOW:  Multiply by exp(-dt/tau) for each compartment
 *
 * @param bridge Dendrite-plasticity bridge
 * @param dt_ms Time step in milliseconds
 * @return 0 on success
 */
int dendrite_plasticity_decay_calcium(
    dendrite_plasticity_bridge_t* bridge,
    float dt_ms);

/* ============================================================================
 * STDP API
 * ============================================================================ */

/**
 * @brief Apply STDP at dendritic location
 *
 * WHAT: Apply spike-timing dependent plasticity
 * WHY:  Hebbian learning at dendritic synapses
 * HOW:  Compute weight change from pre/post timing
 *
 * @param bridge Dendrite-plasticity bridge
 * @param compartment_id Compartment location
 * @param pre_spike_time Presynaptic spike time (ms)
 * @param post_spike_time Postsynaptic spike time (ms)
 * @return Weight change applied
 */
float dendrite_plasticity_apply_stdp(
    dendrite_plasticity_bridge_t* bridge,
    uint32_t compartment_id,
    float pre_spike_time,
    float post_spike_time);

/**
 * @brief Process back-propagating action potential
 *
 * WHAT: Handle BPAP reaching dendritic compartment
 * WHY:  BPAP provides post-spike signal for STDP
 * HOW:  Update post-spike trace, trigger plasticity check
 *
 * @param bridge Dendrite-plasticity bridge
 * @param compartment_id Compartment reached by BPAP
 * @param bpap_time Time of BPAP arrival
 * @param attenuation BPAP amplitude attenuation
 * @return 0 on success
 */
int dendrite_plasticity_process_bpap(
    dendrite_plasticity_bridge_t* bridge,
    uint32_t compartment_id,
    float bpap_time,
    float attenuation);

/* ============================================================================
 * Structural Plasticity API
 * ============================================================================ */

/**
 * @brief Apply structural plasticity to compartment
 *
 * WHAT: Modify dendritic structure based on activity
 * WHY:  Long-term structural changes follow functional plasticity
 * HOW:  Check activity history, trigger growth/shrinkage
 *
 * BIOLOGICAL BASIS:
 * Active spines grow; inactive spines shrink and may be pruned
 *
 * @param bridge Dendrite-plasticity bridge
 * @param compartment_id Compartment for structural changes
 * @return 0 on success
 */
int dendrite_plasticity_apply_structural(
    dendrite_plasticity_bridge_t* bridge,
    uint32_t compartment_id);

/**
 * @brief Get spine density for compartment
 *
 * @param bridge Dendrite-plasticity bridge
 * @param compartment_id Compartment to query
 * @return Spine density (spines per micron)
 */
float dendrite_plasticity_get_spine_density(
    const dendrite_plasticity_bridge_t* bridge,
    uint32_t compartment_id);

/* ============================================================================
 * BCM Metaplasticity API
 * ============================================================================ */

/**
 * @brief Update BCM sliding threshold
 *
 * WHAT: Adapt plasticity threshold based on activity
 * WHY:  Metaplasticity prevents runaway LTP/LTD
 * HOW:  Adjust threshold toward recent activity average
 *
 * BIOLOGICAL BASIS:
 * BCM theory: high activity raises LTP threshold
 * Low activity lowers LTP threshold → homeostasis
 *
 * @param bridge Dendrite-plasticity bridge
 * @param compartment_id Compartment to update
 * @param activity Current activity level
 * @return 0 on success
 */
int dendrite_plasticity_update_bcm(
    dendrite_plasticity_bridge_t* bridge,
    uint32_t compartment_id,
    float activity);

/**
 * @brief Get BCM threshold for compartment
 *
 * @param bridge Dendrite-plasticity bridge
 * @param compartment_id Compartment to query
 * @return Current BCM threshold
 */
float dendrite_plasticity_get_bcm_threshold(
    const dendrite_plasticity_bridge_t* bridge,
    uint32_t compartment_id);

/* ============================================================================
 * Update and Query API
 * ============================================================================ */

/**
 * @brief Full update cycle
 *
 * WHAT: Perform complete plasticity update
 * WHY:  Periodic maintenance of plasticity state
 * HOW:  Decay traces, update thresholds, apply pending changes
 *
 * @param bridge Dendrite-plasticity bridge
 * @param dt_ms Time step in milliseconds
 * @return 0 on success
 */
int dendrite_plasticity_update(
    dendrite_plasticity_bridge_t* bridge,
    float dt_ms);

/**
 * @brief Get accumulated weight delta
 *
 * @param bridge Dendrite-plasticity bridge
 * @param compartment_id Compartment to query
 * @return Accumulated weight change
 */
float dendrite_plasticity_get_weight_delta(
    const dendrite_plasticity_bridge_t* bridge,
    uint32_t compartment_id);

/**
 * @brief Apply accumulated changes to orchestrator
 *
 * WHAT: Push accumulated plasticity to orchestrator
 * WHY:  Coordinate with global plasticity state
 * HOW:  Send weight deltas to orchestrator for integration
 *
 * @param bridge Dendrite-plasticity bridge
 * @return 0 on success
 */
int dendrite_plasticity_apply_to_orchestrator(
    dendrite_plasticity_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Dendrite-plasticity bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int dendrite_plasticity_get_stats(
    const dendrite_plasticity_bridge_t* bridge,
    dendrite_plasticity_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param bridge Dendrite-plasticity bridge
 */
void dendrite_plasticity_reset_stats(dendrite_plasticity_bridge_t* bridge);

/* ============================================================================
 * String Conversion
 * ============================================================================ */

const char* dendrite_plasticity_event_to_string(dendrite_plasticity_event_t event);
const char* dendrite_calcium_level_to_string(dendrite_calcium_level_t level);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DENDRITE_PLASTICITY_BRIDGE_H */
