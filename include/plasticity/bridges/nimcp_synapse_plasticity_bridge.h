/**
 * @file nimcp_synapse_plasticity_bridge.h
 * @brief Synapse-Plasticity Bridge - Central hub for all plasticity mechanisms at synapses
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Central bridge connecting synapses to all 17 plasticity mechanisms
 * WHY:  Synapses are isolated from plasticity - weight changes cannot occur
 * HOW:  Accumulates changes from all mechanisms and applies coordinated updates
 *
 * BIOLOGICAL BASIS:
 * Synapses integrate multiple plasticity signals:
 * - STDP: Spike-timing dependent (Hebbian)
 * - BCM: Activity-dependent threshold sliding
 * - Homeostatic: Firing rate regulation
 * - STP: Short-term facilitation/depression
 * - Metaplasticity: Plasticity of plasticity
 * - Eligibility traces: Reward-modulated learning
 * - Heterosynaptic: Cross-synapse interactions
 * - Synaptic scaling: Global multiplicative normalization
 * - Synaptic tagging: Tag-and-capture for consolidation
 * - Calcium dynamics: Local Ca2+ signaling
 * - Neuromodulation: DA/5-HT/NE/ACh modulation
 * - Metabolic: ATP-dependent plasticity
 * - Structural: Spine morphology changes
 * - Gliotransmission: Astrocyte-mediated modulation
 * - SFA: Spike-frequency adaptation
 * - Intrinsic excitability: Non-synaptic plasticity
 * - Dendritic: Location-dependent effects
 *
 * DESIGN PATTERNS:
 * - Bridge: Decouples synapse from plasticity implementations
 * - Composite: Aggregates multiple plasticity mechanisms
 * - Mediator: Coordinates cross-mechanism interactions
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

#ifndef NIMCP_SYNAPSE_PLASTICITY_BRIDGE_H
#define NIMCP_SYNAPSE_PLASTICITY_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "core/synapse_types/nimcp_synapse_types.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "plasticity/nimcp_plasticity_orchestrator.h"
#include "plasticity/stdp/nimcp_stdp.h"
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

#define SYNAPSE_PLASTICITY_MAX_MECHANISMS  17  /**< Total plasticity mechanisms */
#define SYNAPSE_PLASTICITY_MODULE_NAME     "synapse_plasticity_bridge"

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct synapse_plasticity_bridge synapse_plasticity_bridge_t;

/* Forward declarations for plasticity mechanism types not included in headers */
/* NOTE: Many of these types come from nimcp_neuralnet.h and nimcp_stp.h */
typedef struct bcm_state bcm_state_t;
typedef struct homeostatic_state homeostatic_state_t;
/* stp_state_t defined in nimcp_stp.h */
typedef struct metaplasticity_state metaplasticity_state_t;
typedef struct eligibility_state eligibility_state_t;
typedef struct heterosynaptic_state heterosynaptic_state_t;
typedef struct synaptic_scaling_state synaptic_scaling_state_t;
typedef struct synaptic_tagging_state synaptic_tagging_state_t;
typedef struct calcium_dynamics_state calcium_dynamics_state_t;
typedef struct neuromodulator_state neuromodulator_state_t;
typedef struct metabolic_state metabolic_state_t;
typedef struct structural_plasticity_state structural_state_t;
typedef struct gliotransmission_state gliotransmission_state_t;
typedef struct spike_frequency_adaptation_state spike_frequency_adaptation_state_t;
typedef struct intrinsic_excitability_state intrinsic_excitability_state_t;
typedef struct dendritic_plasticity_state dendritic_plasticity_state_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Plasticity mechanism types
 */
typedef enum {
    PLASTICITY_STDP = 0,
    PLASTICITY_BCM,
    PLASTICITY_HOMEOSTATIC,
    PLASTICITY_STP,
    PLASTICITY_METAPLASTICITY,
    PLASTICITY_ELIGIBILITY,
    PLASTICITY_HETEROSYNAPTIC,
    PLASTICITY_SCALING,
    PLASTICITY_TAGGING,
    PLASTICITY_CALCIUM,
    PLASTICITY_NEUROMODULATOR,
    PLASTICITY_METABOLIC,
    PLASTICITY_STRUCTURAL,
    PLASTICITY_GLIOTRANSMISSION,
    PLASTICITY_SFA,
    PLASTICITY_INTRINSIC,
    PLASTICITY_DENDRITIC,
    PLASTICITY_COUNT
} plasticity_mechanism_t;

/**
 * @brief Weight update modes
 */
typedef enum {
    WEIGHT_UPDATE_ADDITIVE = 0,    /**< Simple addition */
    WEIGHT_UPDATE_MULTIPLICATIVE,  /**< Multiplicative scaling */
    WEIGHT_UPDATE_SOFT_BOUNDS,     /**< Soft weight bounds */
    WEIGHT_UPDATE_HARD_BOUNDS      /**< Hard weight bounds */
} weight_update_mode_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Synapse-plasticity bridge configuration
 */
typedef struct {
    /* Weight bounds */
    float weight_min;                   /**< Minimum weight (default: 0.0) */
    float weight_max;                   /**< Maximum weight (default: 1.0) */
    weight_update_mode_t update_mode;   /**< How to apply weight changes */

    /* Mechanism enables */
    bool enable_stdp;
    bool enable_bcm;
    bool enable_homeostatic;
    bool enable_stp;
    bool enable_metaplasticity;
    bool enable_eligibility;
    bool enable_heterosynaptic;
    bool enable_scaling;
    bool enable_tagging;
    bool enable_calcium;
    bool enable_neuromodulator;
    bool enable_metabolic;
    bool enable_structural;
    bool enable_gliotransmission;
    bool enable_sfa;
    bool enable_intrinsic;
    bool enable_dendritic;

    /* Integration parameters */
    float integration_dt_ms;            /**< Update timestep */
    float accumulator_decay;            /**< Decay rate for accumulators */
    bool clamp_weights;                 /**< Enforce weight bounds */

    /* Bio-async */
    bool enable_bio_async;
    uint32_t inbox_capacity;
} synapse_plasticity_config_t;

/**
 * @brief Per-mechanism contribution tracking
 */
typedef struct {
    float weight_delta;                 /**< Contribution to weight change */
    float last_update_time;             /**< Last update timestamp */
    uint64_t event_count;               /**< Number of events */
    bool active;                        /**< Mechanism is active */
} mechanism_contribution_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Event counts */
    uint64_t pre_spike_count;
    uint64_t post_spike_count;
    uint64_t weight_updates;

    /* Weight change tracking */
    float total_potentiation;
    float total_depression;
    float net_weight_change;

    /* Per-mechanism stats */
    mechanism_contribution_t mechanism_stats[PLASTICITY_COUNT];

    /* Timing */
    float avg_inter_spike_interval;
    float last_update_time;
} synapse_plasticity_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Synapse-plasticity bridge state
 *
 * Central hub connecting a synapse to all plasticity mechanisms
 */
struct synapse_plasticity_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    synapse_plasticity_config_t config;

    /* Core connections */
    synapse_t* synapse;               /**< Connected synapse */
    plasticity_orchestrator_t* orch;        /**< Plasticity orchestrator */

    /* All 17 plasticity mechanisms */
    stdp_synapse_t* stdp;
    bcm_state_t* bcm;
    homeostatic_state_t* homeostatic;
    stp_state_t* stp;
    metaplasticity_state_t* meta;
    eligibility_state_t* eligibility;
    heterosynaptic_state_t* hetero;
    synaptic_scaling_state_t* scaling;
    synaptic_tagging_state_t* tagging;
    calcium_dynamics_state_t* calcium;
    neuromodulator_state_t* neuromod;
    metabolic_state_t* metabolic;
    structural_state_t* structural;
    gliotransmission_state_t* glial;
    spike_frequency_adaptation_state_t* sfa;
    intrinsic_excitability_state_t* intrinsic;
    dendritic_plasticity_state_t* dendritic;

    /* Accumulator for weight changes */
    float weight_delta_accumulator;
    mechanism_contribution_t contributions[PLASTICITY_COUNT];

    /* Spike timing */
    uint64_t last_pre_spike_time;
    uint64_t last_post_spike_time;

    /* Statistics */
    synapse_plasticity_stats_t stats;

    /* State */
    bool initialized;
    uint64_t creation_time;
};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with all mechanisms enabled
 * HOW:  Return struct with biologically plausible parameters
 *
 * @param config Output configuration
 * @return 0 on success
 */
int synapse_plasticity_default_config(synapse_plasticity_config_t* config);

/**
 * @brief Create synapse-plasticity bridge
 *
 * WHAT: Initialize central plasticity hub for a synapse
 * WHY:  Enable all forms of plasticity at this synapse
 * HOW:  Allocate structures, initialize accumulators
 *
 * @param config Configuration (NULL for defaults)
 * @param synapse Synapse to connect
 * @param orch Plasticity orchestrator
 * @return New bridge or NULL on failure
 */
synapse_plasticity_bridge_t* synapse_plasticity_create(
    const synapse_plasticity_config_t* config,
    synapse_t* synapse,
    plasticity_orchestrator_t* orch);

/**
 * @brief Destroy synapse-plasticity bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free allocations, disconnect mechanisms
 *
 * @param bridge Bridge to destroy
 */
void synapse_plasticity_destroy(synapse_plasticity_bridge_t* bridge);

/* ============================================================================
 * Connection API - Connect all 17 plasticity mechanisms
 * ============================================================================ */

int synapse_plasticity_connect_stdp(synapse_plasticity_bridge_t* bridge, stdp_synapse_t* stdp);
int synapse_plasticity_connect_bcm(synapse_plasticity_bridge_t* bridge, bcm_state_t* bcm);
int synapse_plasticity_connect_homeostatic(synapse_plasticity_bridge_t* bridge, homeostatic_state_t* h);
int synapse_plasticity_connect_stp(synapse_plasticity_bridge_t* bridge, stp_state_t* stp);
int synapse_plasticity_connect_metaplasticity(synapse_plasticity_bridge_t* bridge, metaplasticity_state_t* m);
int synapse_plasticity_connect_eligibility(synapse_plasticity_bridge_t* bridge, eligibility_state_t* e);
int synapse_plasticity_connect_heterosynaptic(synapse_plasticity_bridge_t* bridge, heterosynaptic_state_t* h);
int synapse_plasticity_connect_scaling(synapse_plasticity_bridge_t* bridge, synaptic_scaling_state_t* s);
int synapse_plasticity_connect_tagging(synapse_plasticity_bridge_t* bridge, synaptic_tagging_state_t* t);
int synapse_plasticity_connect_calcium(synapse_plasticity_bridge_t* bridge, calcium_dynamics_state_t* c);
int synapse_plasticity_connect_neuromod(synapse_plasticity_bridge_t* bridge, neuromodulator_state_t* n);
int synapse_plasticity_connect_metabolic(synapse_plasticity_bridge_t* bridge, metabolic_state_t* m);
int synapse_plasticity_connect_structural(synapse_plasticity_bridge_t* bridge, structural_state_t* s);
int synapse_plasticity_connect_glial(synapse_plasticity_bridge_t* bridge, gliotransmission_state_t* g);
int synapse_plasticity_connect_sfa(synapse_plasticity_bridge_t* bridge, spike_frequency_adaptation_state_t* s);
int synapse_plasticity_connect_intrinsic(synapse_plasticity_bridge_t* bridge, intrinsic_excitability_state_t* i);
int synapse_plasticity_connect_dendritic(synapse_plasticity_bridge_t* bridge, dendritic_plasticity_state_t* d);

/**
 * @brief Connect all mechanisms from orchestrator
 *
 * WHAT: Bulk-connect all available mechanisms
 * WHY:  Convenient single-call setup
 * HOW:  Query orchestrator for available mechanisms
 *
 * @param bridge Synapse-plasticity bridge
 * @param orch Plasticity orchestrator with mechanisms
 * @return 0 on success
 */
int synapse_plasticity_connect_all(
    synapse_plasticity_bridge_t* bridge,
    plasticity_orchestrator_t* orch);

/**
 * @brief Connect to bio-async router
 */
int synapse_plasticity_connect_bio_async(synapse_plasticity_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 */
int synapse_plasticity_disconnect_bio_async(synapse_plasticity_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 */
bool synapse_plasticity_is_bio_async_connected(const synapse_plasticity_bridge_t* bridge);

/* ============================================================================
 * Spike Event API
 * ============================================================================ */

/**
 * @brief Process presynaptic spike
 *
 * WHAT: Handle arrival of presynaptic spike
 * WHY:  Trigger all pre-spike dependent plasticity
 * HOW:  Update traces, compute weight changes, accumulate
 *
 * @param bridge Synapse-plasticity bridge
 * @param spike_time Time of presynaptic spike
 * @return Total weight change from all mechanisms
 */
float synapse_plasticity_on_pre_spike(
    synapse_plasticity_bridge_t* bridge,
    uint64_t spike_time);

/**
 * @brief Process postsynaptic spike
 *
 * WHAT: Handle postsynaptic spike
 * WHY:  Trigger all post-spike dependent plasticity
 * HOW:  Update traces, compute STDP, update eligibility
 *
 * @param bridge Synapse-plasticity bridge
 * @param spike_time Time of postsynaptic spike
 * @return Total weight change from all mechanisms
 */
float synapse_plasticity_on_post_spike(
    synapse_plasticity_bridge_t* bridge,
    uint64_t spike_time);

/* ============================================================================
 * Weight Update API
 * ============================================================================ */

/**
 * @brief Apply accumulated weight changes
 *
 * WHAT: Apply all accumulated plasticity to synapse weight
 * WHY:  Batch weight updates for efficiency
 * HOW:  Sum contributions, apply bounds, update synapse
 *
 * @param bridge Synapse-plasticity bridge
 * @return Final weight after update
 */
float synapse_plasticity_apply_accumulated(synapse_plasticity_bridge_t* bridge);

/**
 * @brief Get effective weight with STP
 *
 * WHAT: Compute current effective weight including STP
 * WHY:  STP modulates weight on fast timescale
 * HOW:  Apply facilitation/depression factors
 *
 * @param bridge Synapse-plasticity bridge
 * @return Effective synaptic weight
 */
float synapse_plasticity_get_effective_weight(synapse_plasticity_bridge_t* bridge);

/**
 * @brief Get contribution from specific mechanism
 *
 * @param bridge Synapse-plasticity bridge
 * @param mechanism Mechanism type
 * @return Weight delta contribution
 */
float synapse_plasticity_get_mechanism_contribution(
    const synapse_plasticity_bridge_t* bridge,
    plasticity_mechanism_t mechanism);

/* ============================================================================
 * Update and Query API
 * ============================================================================ */

/**
 * @brief Full update cycle
 *
 * WHAT: Update all plasticity mechanisms
 * WHY:  Periodic maintenance of plasticity state
 * HOW:  Decay traces, update thresholds, apply homeostasis
 *
 * @param bridge Synapse-plasticity bridge
 * @param dt_ms Time step in milliseconds
 * @return 0 on success
 */
int synapse_plasticity_update(
    synapse_plasticity_bridge_t* bridge,
    float dt_ms);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Synapse-plasticity bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int synapse_plasticity_get_stats(
    const synapse_plasticity_bridge_t* bridge,
    synapse_plasticity_stats_t* stats);

/**
 * @brief Reset statistics
 */
void synapse_plasticity_reset_stats(synapse_plasticity_bridge_t* bridge);

/**
 * @brief Check if mechanism is connected
 *
 * @param bridge Synapse-plasticity bridge
 * @param mechanism Mechanism type
 * @return true if connected
 */
bool synapse_plasticity_is_mechanism_connected(
    const synapse_plasticity_bridge_t* bridge,
    plasticity_mechanism_t mechanism);

/* ============================================================================
 * String Conversion
 * ============================================================================ */

const char* plasticity_mechanism_to_string(plasticity_mechanism_t mechanism);
const char* weight_update_mode_to_string(weight_update_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SYNAPSE_PLASTICITY_BRIDGE_H */
