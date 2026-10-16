/**
 * @file nimcp_axon_orchestrator_bridge.h
 * @brief Axon-Plasticity Orchestrator Integration Bridge
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Connects axon spike arrivals to plasticity orchestrator pre_spike events
 * WHY:  Axon conduction delays critically affect STDP timing; this bridge ensures
 *       spike arrival times (not initiation times) drive plasticity
 * HOW:  Registers callback with axon network, forwards arrivals to orchestrator
 *
 * BIOLOGICAL BASIS:
 * - Axonal conduction delays range from 0.5ms (fast myelinated) to 100ms+ (slow C-fibers)
 * - STDP window is typically 20-40ms, so delays significantly affect timing
 * - This bridge ensures plasticity sees spike arrival at synapse, not soma firing
 *
 * INTEGRATION FLOW:
 * 1. axon_network_process_arrivals() calls our callback for each arriving spike
 * 2. Callback extracts target_synapse_id from spike event
 * 3. Optionally compensates for delay if needed
 * 4. Calls plasticity_orchestrator_pre_spike() with synapse_id and time
 * 5. Sends bio-async message for distributed notification
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_AXON_ORCHESTRATOR_BRIDGE_H
#define NIMCP_AXON_ORCHESTRATOR_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>
#include "core/axon/nimcp_axon.h"
#include "plasticity/nimcp_plasticity_orchestrator.h"
#include "async/nimcp_bio_router.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum synapse-to-axon mappings */
#define AXON_ORCH_MAX_MAPPINGS 100000

/** Default activity EMA time constant (ms) */
#define AXON_ORCH_DEFAULT_ACTIVITY_TAU_MS 100.0f

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Axon-orchestrator bridge configuration
 *
 * WHAT: Configuration options for bridge behavior
 * WHY:  Allow customization of delay handling and activity tracking
 */
typedef struct {
    /** Use spike initiation time instead of arrival time for STDP */
    bool enable_delay_compensation;

    /** Track axon firing rates for activity-dependent modulation */
    bool enable_activity_tracking;

    /** Connect to bio-async messaging system */
    bool enable_bio_async;

    /** Time constant for activity exponential moving average (ms) */
    float activity_ema_tau_ms;

    /** Initial mapping capacity (0 = default) */
    size_t initial_mapping_capacity;
} axon_orchestrator_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics for monitoring
 */
typedef struct {
    /** Total spikes forwarded to orchestrator */
    uint64_t spikes_forwarded;

    /** Spikes where delay compensation was applied */
    uint64_t spikes_delay_compensated;

    /** Spikes dropped due to unmapped synapses */
    uint64_t spikes_unmapped;

    /** Bio-async messages sent */
    uint64_t bio_async_messages_sent;

    /** Total bridge update calls */
    uint64_t update_calls;
} axon_orchestrator_stats_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Axon-orchestrator bridge handle
 */
typedef struct axon_orchestrator_bridge axon_orchestrator_bridge_t;

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
int axon_orchestrator_default_config(axon_orchestrator_config_t* config);

/**
 * @brief Create axon-orchestrator bridge
 *
 * WHAT: Instantiate bridge connecting axon network to orchestrator
 * WHY:  Enable spike arrival → pre_spike event flow
 * HOW:  Allocate bridge, store handles, initialize mappings
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param orchestrator Plasticity orchestrator (required)
 * @param axon_network Axon network (required)
 * @return Bridge handle or NULL on failure
 */
axon_orchestrator_bridge_t* axon_orchestrator_bridge_create(
    const axon_orchestrator_config_t* config,
    plasticity_orchestrator_t* orchestrator,
    axon_network_t* axon_network
);

/**
 * @brief Destroy axon-orchestrator bridge
 *
 * WHAT: Free bridge and all associated resources
 * WHY:  Proper cleanup prevents memory leaks
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void axon_orchestrator_bridge_destroy(axon_orchestrator_bridge_t* bridge);

/* ============================================================================
 * Mapping API
 * ============================================================================ */

/**
 * @brief Map synapse to axon
 *
 * WHAT: Associate synapse_id with axon_id for routing
 * WHY:  When axon spike arrives, we need to know which synapse to notify
 * HOW:  Store mapping in lookup table
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse identifier
 * @param axon_id Axon identifier
 * @return 0 on success, -1 on error
 */
int axon_orchestrator_map_synapse(
    axon_orchestrator_bridge_t* bridge,
    uint32_t synapse_id,
    uint32_t axon_id
);

/**
 * @brief Unmap synapse from axon
 *
 * WHAT: Remove synapse-axon association
 * WHY:  Support synapse removal/rewiring
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unmap
 * @return 0 on success, -1 on error
 */
int axon_orchestrator_unmap_synapse(
    axon_orchestrator_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get axon ID for synapse
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse to look up
 * @return Axon ID or UINT32_MAX if not mapped
 */
uint32_t axon_orchestrator_get_axon_for_synapse(
    const axon_orchestrator_bridge_t* bridge,
    uint32_t synapse_id
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Process arriving spikes and forward to orchestrator
 *
 * WHAT: Main update function - process axon arrivals
 * WHY:  Called each simulation step to forward spikes
 * HOW:  Calls axon_network_process_arrivals with our callback
 *
 * @param bridge Bridge handle
 * @param current_time_us Current simulation time (microseconds)
 * @return Number of spikes processed, or -1 on error
 */
int axon_orchestrator_bridge_update(
    axon_orchestrator_bridge_t* bridge,
    uint64_t current_time_us
);

/* ============================================================================
 * Spike Handling
 * ============================================================================ */

/**
 * @brief Spike arrival callback for axon network
 *
 * WHAT: Callback invoked when spike arrives at axon terminal
 * WHY:  This is how we receive spike events from axon network
 * HOW:  Called by axon_network_process_arrivals
 *
 * @param axon Axon that carried the spike
 * @param spike Spike event details
 * @param user_data Pointer to axon_orchestrator_bridge_t
 *
 * @note This function is public so it can be passed to axon_network_process_arrivals
 */
void axon_orchestrator_spike_callback(
    axon_t* axon,
    const axon_spike_event_t* spike,
    void* user_data
);

/**
 * @brief Get effective spike time for STDP
 *
 * WHAT: Compute time to use for STDP calculation
 * WHY:  May need to compensate for axon delay
 * HOW:  If delay_compensation enabled, return initiation_time; else arrival_time
 *
 * @param bridge Bridge handle
 * @param spike Spike event
 * @return Effective time in milliseconds
 */
uint64_t axon_orchestrator_get_effective_time_ms(
    const axon_orchestrator_bridge_t* bridge,
    const axon_spike_event_t* spike
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int axon_orchestrator_get_stats(
    const axon_orchestrator_bridge_t* bridge,
    axon_orchestrator_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int axon_orchestrator_reset_stats(axon_orchestrator_bridge_t* bridge);

/**
 * @brief Get number of mapped synapses
 *
 * @param bridge Bridge handle
 * @return Number of mappings
 */
size_t axon_orchestrator_get_mapping_count(
    const axon_orchestrator_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async messaging system
 *
 * WHAT: Register with bio-async router for inter-module messaging
 * WHY:  Enable distributed notification of spike events
 * HOW:  Register as BIO_MODULE_ORCHESTRATOR_AXON
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int axon_orchestrator_connect_bio_async(axon_orchestrator_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async messaging system
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int axon_orchestrator_disconnect_bio_async(axon_orchestrator_bridge_t* bridge);

/**
 * @brief Check if connected to bio-async
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool axon_orchestrator_is_bio_async_connected(
    const axon_orchestrator_bridge_t* bridge
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
plasticity_orchestrator_t* axon_orchestrator_get_orchestrator(
    axon_orchestrator_bridge_t* bridge
);

/**
 * @brief Get axon network handle
 *
 * @param bridge Bridge handle
 * @return Axon network or NULL
 */
axon_network_t* axon_orchestrator_get_axon_network(
    axon_orchestrator_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AXON_ORCHESTRATOR_BRIDGE_H */
