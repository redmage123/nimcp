/**
 * @file nimcp_neuron_orchestrator_bridge.h
 * @brief Neuron-Plasticity Orchestrator Integration Bridge
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Connects neuron spike events to plasticity orchestrator and triggers
 *       downstream events (axon spikes, dendritic backpropagation)
 * WHY:  Neuron firing is the core event that drives plasticity:
 *       - Post-spike triggers STDP post-trace updates
 *       - Spike must propagate to axon terminals (via axon bridge)
 *       - bAP must propagate into dendrites for STDP coincidence
 *       - Firing rate must be tracked for BCM threshold
 * HOW:  Step function integrates neuron dynamics, detects spikes,
 *       cascades to plasticity orchestrator + axon + dendrite systems
 *
 * INTEGRATION FLOW:
 * 1. neuron_orchestrator_step() calls neuron_model_update()
 * 2. If neuron_model_check_spike() returns true:
 *    a. Call plasticity_orchestrator_post_spike(neuron_id, time)
 *    b. For each output axon: axon_initiate_spike(axon, time, amplitude)
 *    c. For each dendrite: dendrite_initiate_bap(dendrite, amplitude, time)
 *    d. Update firing rate for BCM threshold computation
 *    e. Call neuron_model_post_spike() to reset neuron
 * 3. Send bio-async notification of spike event
 *
 * BIOLOGICAL BASIS:
 * - Action potentials are all-or-none events (~100mV, ~1ms)
 * - Each spike propagates forward to axon terminals
 * - bAP propagates backward into dendrites (attenuating with distance)
 * - Firing rate is critical for BCM sliding threshold
 * - STDP requires correlation between pre and post spike times
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEURON_ORCHESTRATOR_BRIDGE_H
#define NIMCP_NEURON_ORCHESTRATOR_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>
#include "core/neuron_models/nimcp_neuron_model.h"
#include "core/axon/nimcp_axon.h"
#include "core/dendrite/nimcp_dendrite.h"
#include "plasticity/nimcp_plasticity_orchestrator.h"
#include "async/nimcp_bio_router.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum neurons per bridge */
#define NEURON_ORCH_MAX_NEURONS 100000

/** Maximum axons per neuron */
#define NEURON_ORCH_MAX_AXONS_PER_NEURON 64

/** Maximum dendrites per neuron */
#define NEURON_ORCH_MAX_DENDRITES_PER_NEURON 32

/** Default firing rate averaging time constant (ms) */
#define NEURON_ORCH_DEFAULT_RATE_TAU_MS 1000.0f

/** Default backpropagating AP amplitude (mV) */
#define NEURON_ORCH_DEFAULT_BAP_AMPLITUDE 30.0f

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Neuron-orchestrator bridge configuration
 *
 * WHAT: Configuration options for bridge behavior
 * WHY:  Allow customization of spike propagation and rate tracking
 */
typedef struct {
    /** Enable automatic axon spike initiation on neuron fire */
    bool enable_axon_propagation;

    /** Enable automatic bAP initiation on neuron fire */
    bool enable_bap_propagation;

    /** Enable firing rate tracking for BCM */
    bool enable_rate_tracking;

    /** Connect to bio-async messaging system */
    bool enable_bio_async;

    /** Time constant for firing rate EMA (ms) */
    float rate_ema_tau_ms;

    /** Default bAP amplitude (mV) */
    float bap_amplitude;

    /** Default axon spike amplitude */
    float spike_amplitude;

    /** Initial neuron capacity (0 = default) */
    size_t initial_neuron_capacity;

} neuron_orchestrator_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics for monitoring
 */
typedef struct {
    /** Total spikes detected */
    uint64_t spikes_detected;

    /** Spikes forwarded to orchestrator */
    uint64_t spikes_forwarded_to_orchestrator;

    /** Axon spikes initiated */
    uint64_t axon_spikes_initiated;

    /** bAPs initiated */
    uint64_t baps_initiated;

    /** Bio-async messages sent */
    uint64_t bio_async_messages_sent;

    /** Total step calls */
    uint64_t step_calls;

    /** Total neurons registered */
    uint32_t neurons_registered;

} neuron_orchestrator_stats_t;

/* ============================================================================
 * Neuron Entry Structure
 * ============================================================================ */

/**
 * @brief Per-neuron entry in the bridge
 *
 * WHAT: Tracks neuron state, connections, and firing rate
 * WHY:  Need to map neuron to its axons/dendrites and track activity
 */
typedef struct {
    /** Neuron ID */
    uint32_t neuron_id;

    /** Neuron model state */
    neuron_model_state_t model_state;

    /** Neuron model vtable (for model-agnostic operations) */
    const neuron_model_vtable_t* vtable;

    /** Output axon IDs */
    uint32_t axon_ids[NEURON_ORCH_MAX_AXONS_PER_NEURON];
    uint32_t num_axons;

    /** Dendrite IDs */
    uint32_t dendrite_ids[NEURON_ORCH_MAX_DENDRITES_PER_NEURON];
    uint32_t num_dendrites;

    /** Firing rate tracking */
    float firing_rate_hz;
    float rate_ema;
    uint64_t last_spike_time_us;
    uint32_t spike_count;

    /** Is entry valid */
    bool valid;

} neuron_bridge_entry_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Neuron-orchestrator bridge handle
 */
typedef struct neuron_orchestrator_bridge neuron_orchestrator_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Initialize configuration with defaults
 *
 * WHAT: Set reasonable default configuration values
 * WHY:  Allow quick setup without specifying all options
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on NULL config
 */
int neuron_orchestrator_default_config(neuron_orchestrator_config_t* config);

/**
 * @brief Create neuron-orchestrator bridge
 *
 * WHAT: Instantiate bridge connecting neurons to orchestrator
 * WHY:  Enable neuron spike → post_spike event + axon + bAP cascade
 * HOW:  Allocate bridge, store handles, initialize neuron registry
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param orchestrator Plasticity orchestrator (required)
 * @param axon_network Axon network for spike propagation (optional)
 * @param dendrite_network Dendrite network for bAP propagation (optional)
 * @return Bridge handle or NULL on failure
 */
neuron_orchestrator_bridge_t* neuron_orchestrator_bridge_create(
    const neuron_orchestrator_config_t* config,
    plasticity_orchestrator_t* orchestrator,
    axon_network_t* axon_network,
    dendrite_network_t* dendrite_network
);

/**
 * @brief Destroy neuron-orchestrator bridge
 *
 * WHAT: Free bridge and all associated resources
 * WHY:  Proper cleanup prevents memory leaks
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void neuron_orchestrator_bridge_destroy(neuron_orchestrator_bridge_t* bridge);

/* ============================================================================
 * Neuron Registration API
 * ============================================================================ */

/**
 * @brief Register neuron with the bridge
 *
 * WHAT: Add a neuron to be managed by the bridge
 * WHY:  Bridge needs to track neurons for spike detection and propagation
 * HOW:  Create entry with neuron model state and connections
 *
 * @param bridge Bridge handle
 * @param neuron_id Unique neuron identifier
 * @param model_state Neuron model state
 * @param vtable Neuron model vtable (required for operations)
 * @return 0 on success, -1 on error
 */
int neuron_orchestrator_register_neuron(
    neuron_orchestrator_bridge_t* bridge,
    uint32_t neuron_id,
    neuron_model_state_t model_state,
    const neuron_model_vtable_t* vtable
);

/**
 * @brief Unregister neuron from the bridge
 *
 * WHAT: Remove a neuron from bridge management
 * WHY:  Support dynamic neuron creation/destruction
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron to unregister
 * @return 0 on success, -1 on error
 */
int neuron_orchestrator_unregister_neuron(
    neuron_orchestrator_bridge_t* bridge,
    uint32_t neuron_id
);

/**
 * @brief Add axon to neuron's output list
 *
 * WHAT: Register an axon as output of a neuron
 * WHY:  When neuron fires, spike propagates to all output axons
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @param axon_id Axon identifier
 * @return 0 on success, -1 on error
 */
int neuron_orchestrator_add_axon(
    neuron_orchestrator_bridge_t* bridge,
    uint32_t neuron_id,
    uint32_t axon_id
);

/**
 * @brief Add dendrite to neuron's dendrite list
 *
 * WHAT: Register a dendrite as part of a neuron
 * WHY:  When neuron fires, bAP propagates into all dendrites
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @param dendrite_id Dendrite identifier
 * @return 0 on success, -1 on error
 */
int neuron_orchestrator_add_dendrite(
    neuron_orchestrator_bridge_t* bridge,
    uint32_t neuron_id,
    uint32_t dendrite_id
);

/* ============================================================================
 * Step/Update API
 * ============================================================================ */

/**
 * @brief Step a single neuron and handle spike cascade
 *
 * WHAT: Update neuron dynamics, detect spike, trigger cascade
 * WHY:  Core function for neuron-plasticity integration
 * HOW:
 *       1. Call neuron_model_update()
 *       2. If spike detected:
 *          - Call plasticity_orchestrator_post_spike()
 *          - Initiate spikes on output axons
 *          - Initiate bAP on dendrites
 *          - Update firing rate
 *          - Call neuron_model_post_spike()
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron to step
 * @param dt_ms Time step in milliseconds
 * @param input_current Input current to neuron
 * @param current_time_us Current simulation time (microseconds)
 * @return 1 if neuron spiked, 0 if not, -1 on error
 */
int neuron_orchestrator_step(
    neuron_orchestrator_bridge_t* bridge,
    uint32_t neuron_id,
    float dt_ms,
    float input_current,
    uint64_t current_time_us
);

/**
 * @brief Step all registered neurons
 *
 * WHAT: Update all neurons in one call
 * WHY:  Efficient batch processing
 * HOW:  Iterate over all registered neurons and call step
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @param inputs Array of input currents (one per neuron, NULL = 0)
 * @param current_time_us Current simulation time
 * @return Number of neurons that spiked, or -1 on error
 */
int neuron_orchestrator_step_all(
    neuron_orchestrator_bridge_t* bridge,
    float dt_ms,
    const float* inputs,
    uint64_t current_time_us
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get firing rate for a neuron
 *
 * WHAT: Retrieve current firing rate estimate
 * WHY:  Used for BCM threshold computation
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @return Firing rate in Hz, or -1 on error
 */
float neuron_orchestrator_get_firing_rate(
    const neuron_orchestrator_bridge_t* bridge,
    uint32_t neuron_id
);

/**
 * @brief Get neuron entry
 *
 * WHAT: Access to full neuron entry for advanced queries
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron identifier
 * @return Neuron entry or NULL if not found
 */
const neuron_bridge_entry_t* neuron_orchestrator_get_entry(
    const neuron_orchestrator_bridge_t* bridge,
    uint32_t neuron_id
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int neuron_orchestrator_get_stats(
    const neuron_orchestrator_bridge_t* bridge,
    neuron_orchestrator_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int neuron_orchestrator_reset_stats(neuron_orchestrator_bridge_t* bridge);

/**
 * @brief Get number of registered neurons
 *
 * @param bridge Bridge handle
 * @return Number of neurons
 */
size_t neuron_orchestrator_get_neuron_count(
    const neuron_orchestrator_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async messaging system
 *
 * WHAT: Register with bio-async router for inter-module messaging
 * WHY:  Enable distributed notification of spike events
 * HOW:  Register as BIO_MODULE_ORCHESTRATOR_NEURON
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int neuron_orchestrator_connect_bio_async(neuron_orchestrator_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async messaging system
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int neuron_orchestrator_disconnect_bio_async(neuron_orchestrator_bridge_t* bridge);

/**
 * @brief Check if connected to bio-async
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool neuron_orchestrator_is_bio_async_connected(
    const neuron_orchestrator_bridge_t* bridge
);

/* ============================================================================
 * Accessors
 * ============================================================================ */

/**
 * @brief Get orchestrator handle
 *
 * @param bridge Bridge handle
 * @return Orchestrator or NULL
 */
plasticity_orchestrator_t* neuron_orchestrator_get_orchestrator(
    neuron_orchestrator_bridge_t* bridge
);

/**
 * @brief Get axon network handle
 *
 * @param bridge Bridge handle
 * @return Axon network or NULL
 */
axon_network_t* neuron_orchestrator_get_axon_network(
    neuron_orchestrator_bridge_t* bridge
);

/**
 * @brief Get dendrite network handle
 *
 * @param bridge Bridge handle
 * @return Dendrite network or NULL
 */
dendrite_network_t* neuron_orchestrator_get_dendrite_network(
    neuron_orchestrator_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEURON_ORCHESTRATOR_BRIDGE_H */
