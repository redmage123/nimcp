/**
 * @file nimcp_dendrite_orchestrator_bridge.h
 * @brief Dendrite-Plasticity Orchestrator Integration Bridge
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional bridge between dendritic spines and plasticity orchestrator
 * WHY:  Spines are the physical substrate of synapses; their state must sync with
 *       the orchestrator's weight and structural plasticity computations
 * HOW:  Maps synapse_id to spine, syncs weight/structural changes bidirectionally
 *
 * BIDIRECTIONAL SYNC:
 *
 * 1. Orchestrator → Spine (weight changes):
 *    - After STDP/BCM computes weight change, update spine's synaptic_weight
 *    - Adjust spine morphology (volume, AMPA receptors) based on weight
 *    - LTP → spine enlargement; LTD → spine shrinkage
 *
 * 2. Spine → Orchestrator (structural changes):
 *    - Spine formation → register new synapse with orchestrator
 *    - Spine elimination → remove synapse from orchestrator
 *    - Spine type transition → update structural plasticity state
 *
 * 3. STDP Event Forwarding:
 *    - Pre-spike at spine → plasticity_orchestrator_pre_spike()
 *    - Post-spike (bAP) at spine → already handled by neuron bridge
 *
 * BIOLOGICAL BASIS:
 * - Spines are ~1-2μm protrusions that receive synaptic input
 * - Spine head volume correlates with synaptic strength (r=0.8+)
 * - AMPA receptor count ∝ spine head surface area
 * - LTP: spine enlarges (thin → mushroom) over hours
 * - LTD: spine shrinks, potentially eliminated
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_DENDRITE_ORCHESTRATOR_BRIDGE_H
#define NIMCP_DENDRITE_ORCHESTRATOR_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>
#include "core/dendrite/nimcp_dendrite.h"
#include "plasticity/nimcp_plasticity_orchestrator.h"
#include "async/nimcp_bio_router.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum spine-synapse mappings */
#define DENDRITE_ORCH_MAX_MAPPINGS 100000

/** Default weight-to-volume scaling factor */
#define DENDRITE_ORCH_DEFAULT_WEIGHT_VOLUME_SCALE 1.0f

/** Default AMPA receptor scaling (receptors per weight unit) */
#define DENDRITE_ORCH_DEFAULT_AMPA_SCALE 10.0f

/* ============================================================================
 * Sync Direction
 * ============================================================================ */

/**
 * @brief Direction of synchronization
 */
typedef enum {
    SYNC_ORCHESTRATOR_TO_SPINE,  /**< Weight → spine morphology */
    SYNC_SPINE_TO_ORCHESTRATOR   /**< Structural change → orchestrator */
} dendrite_sync_direction_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Dendrite-orchestrator bridge configuration
 *
 * WHAT: Configuration options for bridge behavior
 * WHY:  Allow customization of sync behavior and scaling factors
 */
typedef struct {
    /** Automatically sync weight changes to spines */
    bool enable_weight_to_spine_sync;

    /** Automatically sync structural changes to orchestrator */
    bool enable_spine_to_orchestrator_sync;

    /** Forward pre-spike events to orchestrator */
    bool enable_pre_spike_forwarding;

    /** Connect to bio-async messaging system */
    bool enable_bio_async;

    /** Scale factor: weight → spine volume */
    float weight_to_volume_scale;

    /** Scale factor: weight → AMPA receptor count */
    float weight_to_ampa_scale;

    /** Minimum weight change to trigger spine update */
    float min_weight_delta_for_sync;

    /** Initial mapping capacity (0 = default) */
    size_t initial_mapping_capacity;

} dendrite_orchestrator_config_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Bridge statistics for monitoring
 */
typedef struct {
    /** Weight → spine sync operations */
    uint64_t weight_to_spine_syncs;

    /** Spine → orchestrator sync operations */
    uint64_t spine_to_orchestrator_syncs;

    /** Pre-spikes forwarded */
    uint64_t pre_spikes_forwarded;

    /** Spines registered */
    uint64_t spines_registered;

    /** Spines eliminated */
    uint64_t spines_eliminated;

    /** Bio-async messages sent */
    uint64_t bio_async_messages_sent;

    /** Total update calls */
    uint64_t update_calls;

} dendrite_orchestrator_stats_t;

/* ============================================================================
 * Spine-Synapse Mapping
 * ============================================================================ */

/**
 * @brief Mapping between spine and synapse
 *
 * WHAT: Links a dendritic spine to an orchestrator synapse_id
 * WHY:  Enables bidirectional state synchronization
 */
typedef struct {
    uint32_t synapse_id;
    uint32_t dendrite_id;
    uint32_t spine_index;
    bool valid;
} spine_synapse_mapping_t;

/* ============================================================================
 * Bridge Structure
 * ============================================================================ */

/**
 * @brief Dendrite-orchestrator bridge handle
 */
typedef struct dendrite_orchestrator_bridge dendrite_orchestrator_bridge_t;

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
int dendrite_orchestrator_default_config(dendrite_orchestrator_config_t* config);

/**
 * @brief Create dendrite-orchestrator bridge
 *
 * WHAT: Instantiate bridge connecting dendrite spines to orchestrator
 * WHY:  Enable bidirectional weight ↔ spine state synchronization
 * HOW:  Allocate bridge, store handles, initialize mappings
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param orchestrator Plasticity orchestrator (required)
 * @param dendrite_network Dendrite network (required)
 * @return Bridge handle or NULL on failure
 */
dendrite_orchestrator_bridge_t* dendrite_orchestrator_bridge_create(
    const dendrite_orchestrator_config_t* config,
    plasticity_orchestrator_t* orchestrator,
    dendrite_network_t* dendrite_network
);

/**
 * @brief Destroy dendrite-orchestrator bridge
 *
 * WHAT: Free bridge and all associated resources
 * WHY:  Proper cleanup prevents memory leaks
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void dendrite_orchestrator_bridge_destroy(dendrite_orchestrator_bridge_t* bridge);

/* ============================================================================
 * Mapping API
 * ============================================================================ */

/**
 * @brief Map synapse to spine
 *
 * WHAT: Associate synapse_id with a specific spine
 * WHY:  Enables weight ↔ spine synchronization
 * HOW:  Store mapping (synapse_id → dendrite_id, spine_index)
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse identifier
 * @param dendrite_id Dendrite identifier
 * @param spine_index Spine index within dendrite
 * @return 0 on success, -1 on error
 */
int dendrite_orchestrator_map_spine(
    dendrite_orchestrator_bridge_t* bridge,
    uint32_t synapse_id,
    uint32_t dendrite_id,
    uint32_t spine_index
);

/**
 * @brief Unmap synapse from spine
 *
 * WHAT: Remove synapse-spine association
 * WHY:  Support spine elimination
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse to unmap
 * @return 0 on success, -1 on error
 */
int dendrite_orchestrator_unmap_spine(
    dendrite_orchestrator_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Get spine mapping for synapse
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse to look up
 * @param dendrite_id_out Output: dendrite ID
 * @param spine_index_out Output: spine index
 * @return 0 on success, -1 if not found
 */
int dendrite_orchestrator_get_spine_mapping(
    const dendrite_orchestrator_bridge_t* bridge,
    uint32_t synapse_id,
    uint32_t* dendrite_id_out,
    uint32_t* spine_index_out
);

/* ============================================================================
 * Synchronization API
 * ============================================================================ */

/**
 * @brief Sync weight change to spine
 *
 * WHAT: Update spine morphology based on orchestrator weight
 * WHY:  LTP/LTD affects spine size and receptor count
 * HOW:
 *   1. Get weight from orchestrator
 *   2. Scale to volume change
 *   3. Update spine head diameter
 *   4. Update AMPA receptor estimate
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse whose weight changed
 * @return 0 on success, -1 on error
 */
int dendrite_orchestrator_sync_weight_to_spine(
    dendrite_orchestrator_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Sync spine state to orchestrator
 *
 * WHAT: Update orchestrator based on spine structural change
 * WHY:  Structural plasticity (formation, elimination) affects synapse existence
 * HOW:
 *   - SPINE_STATE_POTENTIATED → increase weight
 *   - SPINE_STATE_ELIMINATED → remove synapse
 *   - Type changes (thin→mushroom) → update structural state
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse associated with spine
 * @return 0 on success, -1 on error
 */
int dendrite_orchestrator_sync_spine_to_orchestrator(
    dendrite_orchestrator_bridge_t* bridge,
    uint32_t synapse_id
);

/**
 * @brief Sync all mapped spines
 *
 * WHAT: Full bidirectional sync for all mappings
 * WHY:  Periodic consistency check
 *
 * @param bridge Bridge handle
 * @param direction Which direction to sync
 * @return Number of spines synced, or -1 on error
 */
int dendrite_orchestrator_sync_all(
    dendrite_orchestrator_bridge_t* bridge,
    dendrite_sync_direction_t direction
);

/* ============================================================================
 * Event Handling API
 * ============================================================================ */

/**
 * @brief Handle pre-synaptic spike at spine
 *
 * WHAT: Forward pre-spike event to orchestrator
 * WHY:  STDP requires pre-spike timing
 * HOW:  Calls plasticity_orchestrator_pre_spike()
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse receiving input
 * @param timestamp_ms Spike time
 * @return 0 on success, -1 on error
 */
int dendrite_orchestrator_pre_spike(
    dendrite_orchestrator_bridge_t* bridge,
    uint32_t synapse_id,
    uint64_t timestamp_ms
);

/**
 * @brief Handle spine formation event
 *
 * WHAT: Register new spine with orchestrator
 * WHY:  New synapse needs to be tracked for plasticity
 * HOW:  Map spine, initialize synapse weight in orchestrator
 *
 * @param bridge Bridge handle
 * @param dendrite_id Dendrite where spine formed
 * @param spine_index Index of new spine
 * @param synapse_id Synapse ID to assign
 * @param initial_weight Initial synaptic weight
 * @return 0 on success, -1 on error
 */
int dendrite_orchestrator_spine_formed(
    dendrite_orchestrator_bridge_t* bridge,
    uint32_t dendrite_id,
    uint32_t spine_index,
    uint32_t synapse_id,
    float initial_weight
);

/**
 * @brief Handle spine elimination event
 *
 * WHAT: Remove eliminated spine from orchestrator
 * WHY:  Pruned synapses should not receive plasticity updates
 * HOW:  Unmap spine, remove synapse from orchestrator
 *
 * @param bridge Bridge handle
 * @param synapse_id Synapse being eliminated
 * @return 0 on success, -1 on error
 */
int dendrite_orchestrator_spine_eliminated(
    dendrite_orchestrator_bridge_t* bridge,
    uint32_t synapse_id
);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Periodic bridge update
 *
 * WHAT: Run periodic sync and maintenance
 * WHY:  Catch any missed sync events, update activity stats
 *
 * @param bridge Bridge handle
 * @param current_time_us Current simulation time
 * @return 0 on success, -1 on error
 */
int dendrite_orchestrator_bridge_update(
    dendrite_orchestrator_bridge_t* bridge,
    uint64_t current_time_us
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
int dendrite_orchestrator_get_stats(
    const dendrite_orchestrator_bridge_t* bridge,
    dendrite_orchestrator_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int dendrite_orchestrator_reset_stats(dendrite_orchestrator_bridge_t* bridge);

/**
 * @brief Get number of mapped spines
 *
 * @param bridge Bridge handle
 * @return Number of mappings
 */
size_t dendrite_orchestrator_get_mapping_count(
    const dendrite_orchestrator_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async messaging system
 *
 * WHAT: Register with bio-async router for inter-module messaging
 * WHY:  Enable distributed notification of sync events
 * HOW:  Register as BIO_MODULE_ORCHESTRATOR_DENDRITE
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int dendrite_orchestrator_connect_bio_async(dendrite_orchestrator_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async messaging system
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
int dendrite_orchestrator_disconnect_bio_async(dendrite_orchestrator_bridge_t* bridge);

/**
 * @brief Check if connected to bio-async
 *
 * @param bridge Bridge handle
 * @return true if connected
 */
bool dendrite_orchestrator_is_bio_async_connected(
    const dendrite_orchestrator_bridge_t* bridge
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
plasticity_orchestrator_t* dendrite_orchestrator_get_orchestrator(
    dendrite_orchestrator_bridge_t* bridge
);

/**
 * @brief Get dendrite network handle
 *
 * @param bridge Bridge handle
 * @return Dendrite network or NULL
 */
dendrite_network_t* dendrite_orchestrator_get_dendrite_network(
    dendrite_orchestrator_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_DENDRITE_ORCHESTRATOR_BRIDGE_H */
