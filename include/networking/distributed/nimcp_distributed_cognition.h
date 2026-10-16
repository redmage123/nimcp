/**
 * @file nimcp_distributed_cognition.h
 * @brief Phase 3: Distributed Cognitive Integration Layer
 *
 * WHAT: Integrates P2P networking with cognitive features (neuromodulators,
 *       glial cells, brain regions) enabling distributed neural systems.
 *
 * WHY:  Single-node cognition is limited. Real brains operate as distributed
 *       systems with chemical and electrical signaling across vast networks.
 *
 * HOW:  Event-driven bridge between local cognitive systems and P2P network:
 *       1. Neuromodulator Diffusion: Sync dopamine/serotonin/etc across nodes
 *       2. Glial Coordination: Microglia pruning, astrocyte calcium waves
 *       3. Brain Region Sync: Share regional activity and state
 *
 * ARCHITECTURE:
 *
 *   ┌──────────────────────────────────────────────────────────┐
 *   │         Distributed Cognition Coordinator                │
 *   │  - Neuromodulator Network Sync                           │
 *   │  - Glial Cross-Node Coordination                         │
 *   │  - Brain Region State Sharing                            │
 *   └─────┬────────────────────────────────────────┬──────────┘
 *         │                                        │
 *    ┌────▼──────────┐                      ┌─────▼──────────┐
 *    │ Local Systems │                      │  P2P Network   │
 *    │ - Neuromod    │◄────Protocol────────►│ - NIMCP 2.0    │
 *    │ - Glial Cells │     Messages         │ - Event Packets│
 *    │ - Regions     │                      │ - Control Msgs │
 *    └───────────────┘                      └────────────────┘
 *
 * PROTOCOL USAGE:
 * - FEATURE_DOMAIN_NEUROMOD: Neuromodulator signaling
 * - FEATURE_DOMAIN_GLIAL: Glial coordination
 * - FEATURE_DOMAIN_BRAIN_REGION: Region sync
 * - CTRL_MSG_NEUROMOD_LEVEL: Set concentrations
 * - CTRL_MSG_GLIAL_PRUNING: Coordinate pruning
 * - CTRL_MSG_REGION_ACTIVITY: Share statistics
 *
 * DESIGN PATTERNS:
 * - Mediator: Coordinates between local systems and network
 * - Observer: Listens for local changes, broadcasts to network
 * - Strategy: Different sync strategies per cognitive feature
 * - Adapter: Adapts local APIs to network protocol
 *
 * THREAD SAFETY:
 * - All operations protected by rwlock (readers-writer lock)
 * - Lock-free reads for high-frequency queries
 * - Write operations serialize network updates
 *
 * PERFORMANCE:
 * - Neuromodulator broadcast: O(P) where P = peer count
 * - Glial coordination: O(S×P) where S = synapses, P = peers
 * - Region sync: O(R×P) where R = regions, P = peers
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version Phase 3
 */

#ifndef NIMCP_DISTRIBUTED_COGNITION_H
#define NIMCP_DISTRIBUTED_COGNITION_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "networking/protocol/nimcp_protocol.h"
#include "networking/p2p/nimcp_p2pnode.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "glial/integration/nimcp_glial_integration.h"
#include "core/brain_regions/nimcp_brain_regions.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration and Types
//=============================================================================

/**
 * @brief Synchronization modes for distributed cognition
 */
typedef enum {
    SYNC_MODE_DISABLED,     /**< No synchronization */
    SYNC_MODE_PUSH,         /**< Push local changes to network */
    SYNC_MODE_PULL,         /**< Pull remote changes to local */
    SYNC_MODE_BIDIRECTIONAL /**< Full bidirectional sync (default) */
} sync_mode_t;

/**
 * @brief Configuration for distributed cognition system
 */
typedef struct {
    // Neuromodulator sync
    bool enable_neuromod_sync;     /**< Enable neuromodulator diffusion */
    uint32_t neuromod_broadcast_interval_ms; /**< Broadcast interval */
    float neuromod_diffusion_rate; /**< Cross-node diffusion rate (0-1) */

    // Glial sync
    bool enable_glial_sync;        /**< Enable glial coordination */
    uint32_t glial_sync_interval_ms; /**< Sync interval */

    // Brain region sync
    bool enable_region_sync;       /**< Enable region synchronization */
    uint32_t region_sync_interval_ms; /**< Sync interval */

    // General
    sync_mode_t sync_mode;         /**< Synchronization mode */
    uint32_t max_message_queue;    /**< Max queued network messages */
    bool enable_bio_async;         /**< Enable bio-async communication */
} distrib_cognition_config_t;

/**
 * @brief Statistics for distributed cognition system
 */
typedef struct {
    // Network stats
    uint32_t messages_sent;
    uint32_t messages_received;
    uint32_t messages_dropped;
    uint32_t peers_connected;

    // Neuromodulator sync stats
    uint32_t neuromod_broadcasts;
    uint32_t neuromod_updates_received;
    float avg_neuromod_latency_ms;

    // Glial sync stats
    uint32_t glial_pruning_coordinations;
    uint32_t glial_calcium_propagations;

    // Region sync stats
    uint32_t region_state_syncs;
    uint32_t region_activity_broadcasts;

    // Timing
    uint64_t last_neuromod_sync;
    uint64_t last_glial_sync;
    uint64_t last_region_sync;
} distrib_cognition_stats_t;

/**
 * @brief Opaque handle to distributed cognition coordinator
 */
typedef struct distrib_cognition_struct* distrib_cognition_t;

//=============================================================================
// Creation and Destruction
//=============================================================================

/**
 * @brief Create distributed cognition coordinator
 *
 * @param config Configuration parameters
 * @param p2p_node P2P network node for message transport
 * @return Handle to coordinator, or NULL on failure
 *
 * OWNERSHIP: Caller owns returned handle, must call destroy
 * THREAD SAFETY: Thread-safe creation
 */
NIMCP_EXPORT distrib_cognition_t distrib_cognition_create(
    const distrib_cognition_config_t* config,
    p2p_node_t p2p_node
);

/**
 * @brief Destroy distributed cognition coordinator
 *
 * @param dc Coordinator handle (NULL-safe)
 *
 * CLEANUP: Stops all sync threads, releases network resources
 * THREAD SAFETY: Thread-safe destruction
 */
NIMCP_EXPORT void distrib_cognition_destroy(distrib_cognition_t dc);

//=============================================================================
// Neuromodulator Network Synchronization
//=============================================================================

/**
 * @brief Register neuromodulator pool for network synchronization
 *
 * @param dc Coordinator handle
 * @param pool Local neuromodulator pool to sync
 * @return true on success, false on failure
 *
 * BEHAVIOR:
 * - Local changes broadcast to network (CTRL_MSG_NEUROMOD_LEVEL)
 * - Remote changes applied to local pool (with diffusion rate)
 * - Automatic conflict resolution (weighted average by peer count)
 *
 * FREQUENCY: Broadcasts at neuromod_broadcast_interval_ms
 */
NIMCP_EXPORT bool distrib_cognition_register_neuromod_pool(
    distrib_cognition_t dc,
    neuromodulator_pool_t* pool
);

/**
 * @brief Manually trigger neuromodulator broadcast
 *
 * @param dc Coordinator handle
 * @param type Neuromodulator type to broadcast
 * @param concentration Current concentration (0.0-1.0)
 * @return true on success, false on failure
 *
 * USE CASE: Trigger immediate broadcast on significant events
 * EXAMPLE: Reward signal (dopamine surge) should propagate fast
 */
NIMCP_EXPORT bool distrib_cognition_broadcast_neuromod(
    distrib_cognition_t dc,
    neuromodulator_type_t type,
    float concentration
);

//=============================================================================
// Async Neuromodulator Operations
//=============================================================================

// Forward declare future type to avoid header dependency
struct nimcp_future_struct;
typedef struct nimcp_future_struct* nimcp_future_t;

/**
 * @brief Async version of neuromodulator broadcast
 *
 * @param dc Coordinator handle
 * @param type Neuromodulator type to broadcast
 * @param concentration Current concentration (0.0-1.0)
 * @return Future that completes with bool result, or NULL on failure
 *
 * THREAD SAFETY: Thread-safe, non-blocking
 * COMPLETION: Future completes when broadcast is confirmed by network
 */
NIMCP_EXPORT nimcp_future_t distrib_cognition_broadcast_neuromod_async(
    distrib_cognition_t dc,
    neuromodulator_type_t type,
    float concentration
);

/**
 * @brief Async version of calcium wave propagation
 *
 * @param dc Coordinator handle
 * @param astrocyte_id Source astrocyte ID
 * @param calcium_level Calcium concentration (0.0-1.0)
 * @param wave_velocity Propagation velocity (µm/s)
 * @return Future that completes with bool result, or NULL on failure
 */
NIMCP_EXPORT nimcp_future_t distrib_cognition_propagate_calcium_wave_async(
    distrib_cognition_t dc,
    uint32_t astrocyte_id,
    float calcium_level,
    float wave_velocity
);

/**
 * @brief Async version of pruning coordination
 *
 * @param dc Coordinator handle
 * @param source_neuron_id Source neuron ID
 * @param target_neuron_id Target neuron ID
 * @param activity_score Activity score (0.0-1.0)
 * @param action Pruning action (0=monitor, 1=prune, 2=preserve)
 * @return Future that completes with bool result, or NULL on failure
 */
NIMCP_EXPORT nimcp_future_t distrib_cognition_coordinate_pruning_async(
    distrib_cognition_t dc,
    uint32_t source_neuron_id,
    uint32_t target_neuron_id,
    float activity_score,
    uint8_t action
);

//=============================================================================
// Glial Network Coordination
//=============================================================================

/**
 * @brief Register glial integration system for network coordination
 *
 * @param dc Coordinator handle
 * @param glial Local glial integration system
 * @return true on success, false on failure
 *
 * BEHAVIOR:
 * - Microglia pruning decisions shared (CTRL_MSG_GLIAL_PRUNING)
 * - Astrocyte calcium waves propagate (CTRL_MSG_GLIAL_CALCIUM)
 * - Cross-node coordination for distributed learning
 */
NIMCP_EXPORT bool distrib_cognition_register_glial_system(
    distrib_cognition_t dc,
    glial_integration_t* glial
);

/**
 * @brief Coordinate synaptic pruning across network
 *
 * @param dc Coordinator handle
 * @param source_neuron_id Source neuron ID
 * @param target_neuron_id Target neuron ID
 * @param activity_score Activity score (0.0-1.0)
 * @param action Pruning action (0=monitor, 1=prune, 2=preserve)
 * @return true on success, false on failure
 *
 * USE CASE: Multi-node pruning consensus
 * ALGORITHM: Nodes vote on pruning, majority wins
 */
NIMCP_EXPORT bool distrib_cognition_coordinate_pruning(
    distrib_cognition_t dc,
    uint32_t source_neuron_id,
    uint32_t target_neuron_id,
    float activity_score,
    uint8_t action
);

/**
 * @brief Propagate astrocyte calcium wave across network
 *
 * @param dc Coordinator handle
 * @param astrocyte_id Source astrocyte ID
 * @param calcium_level Calcium concentration (0.0-1.0)
 * @param wave_velocity Propagation velocity (µm/s)
 * @return true on success, false on failure
 *
 * USE CASE: Distributed astrocyte signaling
 * BIOLOGICAL: Calcium waves can span multiple brain regions
 */
NIMCP_EXPORT bool distrib_cognition_propagate_calcium_wave(
    distrib_cognition_t dc,
    uint32_t astrocyte_id,
    float calcium_level,
    float wave_velocity
);

//=============================================================================
// Brain Region Synchronization
//=============================================================================

/**
 * @brief Register brain region for network synchronization
 *
 * @param dc Coordinator handle
 * @param region Brain region to sync
 * @return true on success, false on failure
 *
 * BEHAVIOR:
 * - Regional activity statistics shared (CTRL_MSG_REGION_ACTIVITY)
 * - State updates synchronized (CTRL_MSG_REGION_SYNC)
 * - Multi-node brain coordination
 */
NIMCP_EXPORT bool distrib_cognition_register_brain_region(
    distrib_cognition_t dc,
    brain_region_t* region
);

/**
 * @brief Broadcast brain region activity to network
 *
 * @param dc Coordinator handle
 * @param region_type Region type identifier
 * @param avg_activity Average activity level (0.0-1.0)
 * @param spike_rate Average spike rate (Hz)
 * @param active_neurons Number of active neurons
 * @param total_neurons Total neurons in region
 * @return true on success, false on failure
 *
 * USE CASE: Share regional state for distributed coordination
 */
NIMCP_EXPORT bool distrib_cognition_broadcast_region_activity(
    distrib_cognition_t dc,
    uint16_t region_type,
    float avg_activity,
    float spike_rate,
    uint32_t active_neurons,
    uint32_t total_neurons
);

//=============================================================================
// Control and Monitoring
//=============================================================================

/**
 * @brief Start distributed cognition synchronization
 *
 * @param dc Coordinator handle
 * @return true on success, false on failure
 *
 * BEHAVIOR: Starts sync threads for all enabled features
 */
NIMCP_EXPORT bool distrib_cognition_start(distrib_cognition_t dc);

/**
 * @brief Stop distributed cognition synchronization
 *
 * @param dc Coordinator handle
 * @return true on success, false on failure
 *
 * BEHAVIOR: Stops sync threads, flushes pending messages
 */
NIMCP_EXPORT bool distrib_cognition_stop(distrib_cognition_t dc);

/**
 * @brief Get distributed cognition statistics
 *
 * @param dc Coordinator handle
 * @param stats Output statistics structure
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool distrib_cognition_get_stats(
    distrib_cognition_t dc,
    distrib_cognition_stats_t* stats
);

/**
 * @brief Set synchronization mode
 *
 * @param dc Coordinator handle
 * @param mode New sync mode
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool distrib_cognition_set_sync_mode(
    distrib_cognition_t dc,
    sync_mode_t mode
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_DISTRIBUTED_COGNITION_H
