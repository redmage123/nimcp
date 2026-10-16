/**
 * @file nimcp_neural_plasticity_coordinator.h
 * @brief Neural Plasticity Coordinator - Unified Neural-Plasticity Integration
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Unified coordinator for neural computation and plasticity
 * WHY:  Provides single point of control for:
 *       - Neuron dynamics (integrate-and-fire, Izhikevich, etc.)
 *       - Axon spike propagation with delays
 *       - Dendritic integration and structural plasticity
 *       - Plasticity orchestrator (STDP, BCM, homeostatic, etc.)
 * HOW:  Wraps axon/neuron/dendrite bridges with consistent simulation interface
 *
 * ARCHITECTURE:
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                    NEURAL PLASTICITY COORDINATOR                        │
 * │                                                                         │
 * │  ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐           │
 * │  │ Axon-Orch       │ │ Dendrite-Orch   │ │ Neuron-Orch     │           │
 * │  │ Bridge          │ │ Bridge          │ │ Bridge          │           │
 * │  └────────┬────────┘ └────────┬────────┘ └────────┬────────┘           │
 * └───────────┼───────────────────┼───────────────────┼────────────────────┘
 *             │                   │                   │
 *             ▼                   ▼                   ▼
 *     ┌───────────────┐   ┌───────────────┐   ┌───────────────┐
 *     │ Axon Network  │   │ Dendrite Net  │   │ Neuron Models │
 *     │ - Spike prop  │   │ - Spines      │   │ - Izhikevich  │
 *     │ - Delays      │   │ - STDP        │   │ - Two-comp    │
 *     └───────────────┘   └───────────────┘   └───────────────┘
 *             │                   │                   │
 *             └───────────────────┴───────────────────┘
 *                                 │
 *                     ┌───────────▼───────────┐
 *                     │ PLASTICITY ORCHESTRATOR│
 *                     │ + Bio-async + Immune   │
 *                     │ + UMM + Logging        │
 *                     └────────────────────────┘
 *
 * SIMULATION LOOP:
 * 1. neural_plasticity_step() called each timestep
 * 2. Step all neurons (detect spikes, trigger cascades)
 * 3. Process axon arrivals (forward to orchestrator pre_spike)
 * 4. Update dendrites (process inputs, update spines)
 * 5. Run orchestrator update (apply plasticity)
 * 6. Sync spine↔weight changes
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEURAL_PLASTICITY_COORDINATOR_H
#define NIMCP_NEURAL_PLASTICITY_COORDINATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>
#include "plasticity/orchestrator/nimcp_axon_orchestrator_bridge.h"
#include "plasticity/orchestrator/nimcp_neuron_orchestrator_bridge.h"
#include "plasticity/orchestrator/nimcp_dendrite_orchestrator_bridge.h"
#include "plasticity/nimcp_plasticity_orchestrator.h"
#include "core/axon/nimcp_axon.h"
#include "core/dendrite/nimcp_dendrite.h"
#include "core/neuron_models/nimcp_neuron_model.h"
#include "async/nimcp_bio_router.h"

/* Forward declarations */
struct brain_immune_system;
struct unified_mem_manager;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Default simulation timestep (ms) */
#define NEURAL_COORD_DEFAULT_DT_MS 0.1f

/** Default update interval for orchestrator (ms) */
#define NEURAL_COORD_DEFAULT_ORCH_INTERVAL_MS 1.0f

/** Default sync interval for spine-weight (ms) */
#define NEURAL_COORD_DEFAULT_SYNC_INTERVAL_MS 10.0f

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Neural plasticity coordinator configuration
 *
 * WHAT: Master configuration for all subsystems
 * WHY:  Single point for system-wide settings
 */
typedef struct {
    /** Axon bridge configuration (NULL for defaults) */
    const axon_orchestrator_config_t* axon_config;

    /** Neuron bridge configuration (NULL for defaults) */
    const neuron_orchestrator_config_t* neuron_config;

    /** Dendrite bridge configuration (NULL for defaults) */
    const dendrite_orchestrator_config_t* dendrite_config;

    /** Orchestrator configuration (NULL for defaults) */
    const plasticity_orchestrator_config_t* orchestrator_config;

    /** Default simulation timestep (ms) */
    float default_dt_ms;

    /** How often to run orchestrator update (ms) */
    float orchestrator_interval_ms;

    /** How often to sync spine↔weight (ms) */
    float sync_interval_ms;

    /** Enable bio-async for all components */
    bool enable_bio_async;

    /** Enable immune system integration */
    bool enable_immune_integration;

    /** Enable UMM memory management */
    bool enable_umm;

} neural_plasticity_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Aggregated statistics from all subsystems
 */
typedef struct {
    /* Neuron statistics */
    uint64_t total_spikes;
    uint64_t total_neuron_steps;
    uint32_t neurons_registered;

    /* Axon statistics */
    uint64_t axon_spikes_initiated;
    uint64_t axon_spikes_arrived;
    uint32_t axon_mappings;

    /* Dendrite statistics */
    uint64_t pre_spikes_forwarded;
    uint64_t spines_registered;
    uint64_t spines_eliminated;
    uint32_t dendrite_mappings;

    /* Plasticity statistics */
    uint64_t ltp_events;
    uint64_t ltd_events;
    uint64_t orchestrator_updates;

    /* Timing */
    uint64_t total_steps;
    float last_step_time_us;
    float mean_step_time_us;

} neural_plasticity_stats_t;

/* ============================================================================
 * Coordinator Structure
 * ============================================================================ */

/**
 * @brief Neural plasticity coordinator handle
 */
typedef struct neural_plasticity_coordinator neural_plasticity_coordinator_t;

/** Forward declaration for plasticity bridge (Phase 5 backprop gating) */
#ifndef TPB_CONTEXT_TYPEDEF
#define TPB_CONTEXT_TYPEDEF
typedef struct tpb_context tpb_context_t;
#endif

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default coordinator configuration
 *
 * WHAT: Initialize configuration with sensible defaults
 * WHY:  Quick setup without specifying all options
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on NULL config
 */
int neural_plasticity_default_config(neural_plasticity_config_t* config);

/**
 * @brief Create neural plasticity coordinator
 *
 * WHAT: Create unified coordinator with all subsystems
 * WHY:  Single entry point for neural-plasticity simulation
 * HOW:  Creates orchestrator, bridges, connects everything
 *
 * @param config Configuration (NULL for defaults)
 * @param axon_network Axon network (optional, but needed for spike propagation)
 * @param dendrite_network Dendrite network (optional, but needed for spines)
 * @return Coordinator handle or NULL on failure
 */
neural_plasticity_coordinator_t* neural_plasticity_coordinator_create(
    const neural_plasticity_config_t* config,
    axon_network_t* axon_network,
    dendrite_network_t* dendrite_network
);

/**
 * @brief Destroy neural plasticity coordinator
 *
 * WHAT: Free coordinator and all subsystems
 * WHY:  Proper cleanup prevents memory leaks
 *
 * @param coordinator Coordinator to destroy (NULL-safe)
 */
void neural_plasticity_coordinator_destroy(neural_plasticity_coordinator_t* coordinator);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect immune system
 *
 * WHAT: Link coordinator to brain immune system
 * WHY:  Inflammation modulates plasticity
 *
 * @param coordinator Coordinator
 * @param immune_system Brain immune system
 * @return 0 on success, -1 on error
 */
int neural_plasticity_connect_immune(
    neural_plasticity_coordinator_t* coordinator,
    struct brain_immune_system* immune_system
);

/**
 * @brief Connect unified memory manager
 *
 * WHAT: Link coordinator to UMM for efficient memory
 * WHY:  High-frequency events benefit from memory pools
 *
 * @param coordinator Coordinator
 * @param umm Unified memory manager
 * @return 0 on success, -1 on error
 */
int neural_plasticity_connect_umm(
    neural_plasticity_coordinator_t* coordinator,
    struct unified_mem_manager* umm
);

/**
 * @brief Connect bio-async for all subsystems
 *
 * WHAT: Register all bridges with bio-async router
 * WHY:  Enable inter-module messaging
 *
 * @param coordinator Coordinator
 * @return 0 on success, -1 on error
 */
int neural_plasticity_connect_bio_async(neural_plasticity_coordinator_t* coordinator);

/* ============================================================================
 * Neuron Registration API
 * ============================================================================ */

/**
 * @brief Register neuron with coordinator
 *
 * WHAT: Add neuron to be managed by the coordinator
 * WHY:  Neurons need to be tracked for spike detection
 *
 * @param coordinator Coordinator
 * @param neuron_id Unique neuron ID
 * @param model_state Neuron model state
 * @param vtable Neuron model vtable
 * @return 0 on success, -1 on error
 */
int neural_plasticity_register_neuron(
    neural_plasticity_coordinator_t* coordinator,
    uint32_t neuron_id,
    neuron_model_state_t model_state,
    const neuron_model_vtable_t* vtable
);

/**
 * @brief Add axon to neuron
 *
 * WHAT: Register output axon for a neuron
 * WHY:  Spikes propagate to output axons
 *
 * @param coordinator Coordinator
 * @param neuron_id Neuron ID
 * @param axon_id Axon ID
 * @return 0 on success, -1 on error
 */
int neural_plasticity_add_neuron_axon(
    neural_plasticity_coordinator_t* coordinator,
    uint32_t neuron_id,
    uint32_t axon_id
);

/**
 * @brief Add dendrite to neuron
 *
 * WHAT: Register dendrite for a neuron
 * WHY:  bAPs propagate to dendrites
 *
 * @param coordinator Coordinator
 * @param neuron_id Neuron ID
 * @param dendrite_id Dendrite ID
 * @return 0 on success, -1 on error
 */
int neural_plasticity_add_neuron_dendrite(
    neural_plasticity_coordinator_t* coordinator,
    uint32_t neuron_id,
    uint32_t dendrite_id
);

/* ============================================================================
 * Synapse Registration API
 * ============================================================================ */

/**
 * @brief Register synapse with mappings
 *
 * WHAT: Register synapse with axon and spine mappings
 * WHY:  Enables proper routing of spikes and plasticity
 *
 * @param coordinator Coordinator
 * @param synapse_id Synapse ID
 * @param axon_id Presynaptic axon ID
 * @param dendrite_id Postsynaptic dendrite ID
 * @param spine_index Spine index on dendrite
 * @param initial_weight Initial synaptic weight
 * @return 0 on success, -1 on error
 */
int neural_plasticity_register_synapse(
    neural_plasticity_coordinator_t* coordinator,
    uint32_t synapse_id,
    uint32_t axon_id,
    uint32_t dendrite_id,
    uint32_t spine_index,
    float initial_weight
);

/**
 * @brief Unregister synapse
 *
 * WHAT: Remove synapse from coordinator
 * WHY:  Support synapse elimination
 *
 * @param coordinator Coordinator
 * @param synapse_id Synapse to remove
 * @return 0 on success, -1 on error
 */
int neural_plasticity_unregister_synapse(
    neural_plasticity_coordinator_t* coordinator,
    uint32_t synapse_id
);

/* ============================================================================
 * Simulation API
 * ============================================================================ */

/**
 * @brief Run one simulation step
 *
 * WHAT: Advance entire neural-plasticity system by dt
 * WHY:  Main simulation entry point
 * HOW:
 *   1. Step all neurons (with inputs)
 *   2. Process axon arrivals
 *   3. Update dendrites
 *   4. Run orchestrator update (if interval reached)
 *   5. Sync spine↔weight (if interval reached)
 *
 * @param coordinator Coordinator
 * @param dt_ms Time step in milliseconds (0 = use default)
 * @param neuron_inputs Input currents (one per registered neuron, NULL = 0)
 * @param current_time_us Current simulation time (microseconds)
 * @return Number of spikes this step, or -1 on error
 */
int neural_plasticity_step(
    neural_plasticity_coordinator_t* coordinator,
    float dt_ms,
    const float* neuron_inputs,
    uint64_t current_time_us
);

/**
 * @brief Process reward signal
 *
 * WHAT: Deliver reward/punishment for reinforcement learning
 * WHY:  Modulates eligibility traces
 *
 * @param coordinator Coordinator
 * @param reward Reward magnitude
 * @param timestamp_us Signal time
 * @return 0 on success, -1 on error
 */
int neural_plasticity_reward(
    neural_plasticity_coordinator_t* coordinator,
    float reward,
    uint64_t timestamp_us
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get synapse weight
 *
 * @param coordinator Coordinator
 * @param synapse_id Synapse ID
 * @return Weight, or NaN on error
 */
float neural_plasticity_get_weight(
    const neural_plasticity_coordinator_t* coordinator,
    uint32_t synapse_id
);

/**
 * @brief Set synapse weight
 *
 * @param coordinator Coordinator
 * @param synapse_id Synapse ID
 * @param weight New weight
 * @return 0 on success, -1 on error
 */
int neural_plasticity_set_weight(
    neural_plasticity_coordinator_t* coordinator,
    uint32_t synapse_id,
    float weight
);

/**
 * @brief Get neuron firing rate
 *
 * @param coordinator Coordinator
 * @param neuron_id Neuron ID
 * @return Firing rate in Hz, or -1 on error
 */
float neural_plasticity_get_firing_rate(
    const neural_plasticity_coordinator_t* coordinator,
    uint32_t neuron_id
);

/**
 * @brief Get aggregated statistics
 *
 * @param coordinator Coordinator
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int neural_plasticity_get_stats(
    const neural_plasticity_coordinator_t* coordinator,
    neural_plasticity_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param coordinator Coordinator
 * @return 0 on success, -1 on error
 */
int neural_plasticity_reset_stats(neural_plasticity_coordinator_t* coordinator);

/* ============================================================================
 * Accessors
 * ============================================================================ */

/**
 * @brief Get plasticity orchestrator
 *
 * @param coordinator Coordinator
 * @return Orchestrator or NULL
 */
plasticity_orchestrator_t* neural_plasticity_get_orchestrator(
    neural_plasticity_coordinator_t* coordinator
);

/**
 * @brief Get axon bridge
 *
 * @param coordinator Coordinator
 * @return Axon bridge or NULL
 */
axon_orchestrator_bridge_t* neural_plasticity_get_axon_bridge(
    neural_plasticity_coordinator_t* coordinator
);

/**
 * @brief Get neuron bridge
 *
 * @param coordinator Coordinator
 * @return Neuron bridge or NULL
 */
neuron_orchestrator_bridge_t* neural_plasticity_get_neuron_bridge(
    neural_plasticity_coordinator_t* coordinator
);

/**
 * @brief Get dendrite bridge
 *
 * @param coordinator Coordinator
 * @return Dendrite bridge or NULL
 */
dendrite_orchestrator_bridge_t* neural_plasticity_get_dendrite_bridge(
    neural_plasticity_coordinator_t* coordinator
);

/**
 * @brief Get axon network
 *
 * @param coordinator Coordinator
 * @return Axon network or NULL
 */
axon_network_t* neural_plasticity_get_axon_network(
    neural_plasticity_coordinator_t* coordinator
);

/**
 * @brief Get dendrite network
 *
 * @param coordinator Coordinator
 * @return Dendrite network or NULL
 */
dendrite_network_t* neural_plasticity_get_dendrite_network(
    neural_plasticity_coordinator_t* coordinator
);

/**
 * @brief Set plasticity bridge for backprop gating (Phase 5)
 *
 * When set, the coordinator skips all plasticity updates while backprop is active,
 * preventing STDP/BCM interference with gradient-based training.
 *
 * @param coordinator Coordinator
 * @param tpb         Plasticity bridge context (not owned; NULL to disable gating)
 */
void neural_plasticity_set_plasticity_bridge(
    neural_plasticity_coordinator_t* coordinator,
    tpb_context_t* tpb
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEURAL_PLASTICITY_COORDINATOR_H */
